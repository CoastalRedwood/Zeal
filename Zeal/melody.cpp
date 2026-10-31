#include "melody.h"

#include <algorithm>

#include "callbacks.h"
#include "commands.h"
#include "equip_item.h"  // click-from-inventory setting: bag steps and name matches need it
#include "game_functions.h"
#include "hook_wrapper.h"
#include "string_util.h"
#include "zeal.h"

// Requirements for Melody per Secrets in discord zeal-discussions ~ 2024/03/25
// - Bards only
// - 5 song limit
// - Retries allowed on missed notes
// - Character stuns must end melody

// Test cases:
// - Command line behavior and messages:
//   - Bard class only
//   - # of songs limit <= 5
//   - Only ints as parameters
//   - Zero parameter melody ends melody
//   - Start is prevented when not standing
//   - New /melody without a /stopsong transitions cleanly after current song
//   - /stopsong immediately stops (aborts) active song
// - Check basic song looping functionality (single song, multiple songs)
// - Retry logic for missed notes (correct rewind of song index, retry timeout)\
//   - Should advance song after 8 retries (try Selo's indoors)
//   - Should terminate melody after 15 failures without a success
// - Graceful handling of spells without single target
//   - Skipping of song with single line complaint
//   - Termination of melody after retry limit if all songs are failing
// - Terminated when sitting
// - Paused when zoning, trading, looting, or ducking and then resumed

constexpr int RETRY_COUNT_REWIND_LIMIT = 8;          // Will rewind up to 8 times.
constexpr int RETRY_COUNT_END_LIMIT = 15;            // Will terminate if 15 retries w/out a 'success'.
constexpr unsigned int MELODY_ZONE_IN_DELAY = 2000;  // Minimum wait after zoning before attempting to continue melody.
constexpr unsigned int MELODY_WAIT_TIMEOUT = 1500;   // Maximum wait after the casting timer expires before retrying.
constexpr unsigned int MELODY_WAIT_USE_ITEM_TIMEOUT = 500;  // Wait time after using a clicky.
constexpr unsigned int RETRY_BACKOFF_STEP = 250;   // Added wait per failure in a row after the first.
constexpr unsigned int RETRY_BACKOFF_MAX = 1000;   // Longest wait between attempts.
constexpr unsigned int USE_ITEM_QUEUE_TIMEOUT =
    3650;  // Max duration a useitem will stay queued for before giving up (mostly to prevent ultra-stale clicks).

enum UseItemState : int { Idle = 0, CastRequested, CastStarted };

bool Melody::start(const std::vector<MelodyStep> &new_songs, bool resume) {
  if (!Zeal::Game::is_in_game()) return false;

  Zeal::GameStructures::GAMECHARINFO *char_info = Zeal::Game::get_char_info();
  if (!char_info || char_info->StunnedState) {
    Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, "Can not start melody while stunned.");
    return false;
  }

  Zeal::GameStructures::Entity *self = Zeal::Game::get_self();
  if (!self || (self->StandingState != Stance::Stand)) {
    Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, "Can only start melody when standing.");
    return false;
  }

  // Confirm every gem step is a memorized spell and every item step is a real item. A missing one
  // is skipped with a complaint; a rotation of nothing valid is refused.
  std::vector<MelodyStep> valid_songs;
  int song_count = 0;
  for (const MelodyStep &step : new_songs) {
    if (step.is_item()) {
      if (!Zeal::Game::get_inventory_item_from_global_slot_id(step.item_slot, false)) {
        Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, "Error: skipping %s, no item there", step.key.c_str());
        continue;
      }
      valid_songs.push_back(step);
      continue;
    }
    const int gem_index = step.gem;
    if (gem_index < 0 || gem_index >= GAME_NUM_SPELL_GEMS) {
      Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, "Error: Invalid spell gem %i", gem_index + 1);
      return false;
    }

    if (char_info->MemorizedSpell[gem_index] == -1)
      Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, "Error: skipping empty spell gem %i", gem_index + 1);
    else {
      valid_songs.push_back(step);
      song_count++;
    }
  }

  if (song_count == 0 && !new_songs.empty()) {
    Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, "Error: no valid songs");
    return false;
  }

  if (!enter_zone_time)  // Ensure this gets set so melody starts immediately.
    enter_zone_time = GetTickCount64() - MELODY_ZONE_IN_DELAY;

  // Start at the beginning if not resuming or the resumed song list has changed or invalid index.
  // We are assuming that the new_songs list is the songs list from the resume() call.
  if (!resume || (new_songs.size() != valid_songs.size()) || (current_index < 0) ||
      (current_index >= valid_songs.size()))
    current_index = -1;  // Reset to start of the songs list.

  songs = valid_songs;
  is_active = !songs.empty();
  retry_count = 0;
  retry_not_before = 0;
  casting_melody_spell_id = kInvalidSpellId;
  retry_spell_id = kInvalidSpellId;
  deferred_spell_id = kInvalidSpellId;
  use_item_index = -1;
  use_item_ack_state = UseItemState::Idle;
  if (is_active) Zeal::Game::print_chat(USERCOLOR_SPELLS, "You begin playing a melody.");
  return true;
}

// "3" -> gem 3; "i22" -> /useitem slot 22; "i2.3" -> bag 2, slot 3; "iBreath" -> the first ready
// clicky whose name starts with "Breath" (case sensitive). A step cannot hold a space, so an
// underscore stands for one: "iComposers_Chestplate" clicks Composers Chestplate wherever it is,
// where "iComposers" would take whichever Composers clicky comes first (worn before bagged).
// Slot numbering is /useitem's.
bool Melody::parse_step(const std::string &token, MelodyStep *out) {
  *out = MelodyStep();
  out->key = token;
  int number = -1;
  if (Zeal::String::tryParse(token, &number, true)) {
    out->gem = number - 1;  // base 0
    return true;
  }
  if (token.size() < 2 || (token[0] != 'i' && token[0] != 'I')) {
    Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, "Melody parsing error: Usage example: /melody 1 2 3 4 i22");
    return false;
  }
  const std::string rest = token.substr(1);
  const bool check_bags = ZealService::get_instance()->equip_item_hook &&
                          ZealService::get_instance()->equip_item_hook->setting_click_from_inventory.get();
  const size_t dot = rest.find('.');
  int slot = -1;
  if (dot != std::string::npos) {
    int bag = 0, bagslot = 0;
    if (Zeal::String::tryParse(rest.substr(0, dot), &bag, true) &&
        Zeal::String::tryParse(rest.substr(dot + 1), &bagslot, true)) {
      if (bag < 1 || bag > GAME_NUM_INVENTORY_PACK_SLOTS || bagslot < 1 || bagslot > GAME_NUM_CONTAINER_SLOTS) {
        Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, "Melody: %s needs bag 1-%i and slot 1-%i", token.c_str(),
                               GAME_NUM_INVENTORY_PACK_SLOTS, GAME_NUM_CONTAINER_SLOTS);
        return false;
      }
      if (!check_bags) {
        Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, "Melody: enable Zeal click from inventory to use %s",
                               token.c_str());
        return false;
      }
      slot = GAME_CONTAINER_SLOTS_START + (bag - 1) * GAME_NUM_CONTAINER_SLOTS + bagslot - 1;
    }
  } else if (Zeal::String::tryParse(rest, &slot, true)) {
    if (slot < 0 || slot > GAME_PACKS_SLOTS_END || slot == GAME_EQUIPMENT_SLOTS_END) {
      Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, "Melody: item slots are 0 to %i (see /useitem)",
                             GAME_PACKS_SLOTS_END);
      return false;
    }
    if (slot < GAME_EQUIPMENT_SLOTS_END) slot += GAME_EQUIPMENT_SLOTS_START;  // /useitem's translation.
  } else {
    std::string name = rest;
    for (char &c : name)
      if (c == '_') c = ' ';
    slot = Zeal::Game::find_use_item_by_name(name, check_bags);
    if (slot < GAME_EQUIPMENT_SLOTS_START) {
      Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, "Melody: no ready clicky starting with %s", name.c_str());
      return false;
    }
  }
  if (slot < 0) {
    Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, "Melody parsing error: Usage example: /melody 1 2 3 4 i22");
    return false;
  }
  out->item_slot = slot;
  return true;
}

// An item step is played by the same path as a queued /useitem: if the step after the current one
// is an item, it becomes the pending click and the rotation moves on to it. Retries and deferred
// songs come first, exactly as they do for the next gem.
void Melody::queue_item_step_if_next() {
  if (use_item_index >= 0 || songs.empty()) return;
  if (retry_spell_id != kInvalidSpellId || deferred_spell_id != kInvalidSpellId) return;
  int next = current_index + 1;
  if (next >= static_cast<int>(songs.size()) || next < 0) next = 0;
  const MelodyStep &step = songs[next];
  if (!step.is_item()) return;
  current_index = next;
  use_item_index = step.item_slot;
  use_item_timeout = GetTickCount64() + USE_ITEM_QUEUE_TIMEOUT;
}

void Melody::resume() {
  if (is_active) return;  // Already running, nothing to do.

  start(songs, true);
}

void Melody::end(bool do_print) {
  if (is_active) {
    // songs is left alone and and current_index is rewound (if needed) to before the interrupted song.
    if (casting_melody_spell_id != kInvalidSpellId) {
      current_index--;
      if (current_index < 0) {  // Handle wraparound.
        current_index = songs.size() - 1;
      }
    }
    is_active = false;
    retry_count = 0;
    retry_not_before = 0;
    casting_melody_spell_id = kInvalidSpellId;
    retry_spell_id = kInvalidSpellId;
    deferred_spell_id = kInvalidSpellId;
    use_item_index = -1;
      use_item_ack_state = UseItemState::Idle;

    if (do_print) Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, "Your melody has ended.");
  }
}

bool Melody::use_item(int item_index) {
  if (!is_active) return false;
  // Set fields so use_item(item_index) will execute during tick().
  use_item_index = item_index;
  use_item_timeout = GetTickCount64() + USE_ITEM_QUEUE_TIMEOUT;
  return true;
}

void Melody::handle_stop_cast_callback(BYTE reason, WORD spell_id) {
  // Terminate melody on stop except for missed note (part of reason == 3) rewind attempts.
  if (reason != 3 || !is_active) {
    end(true);
    return;
  }

  // Support rewinding to the interrupted last song (primarily for missed notes).
  // Note that reason code == 3 is shared by missed notes as well as other failures (such as the spell
  // is not allowed in the zone), so we use a retry_count to limit the spammy loop that is
  // difficult to click off with UI spell gems (/stopsong, /melody still work fine). The modulo
  // check skips the rewind so it advances to the next song but then allows that song to retry.
  if (casting_melody_spell_id == spell_id && (++retry_count % RETRY_COUNT_REWIND_LIMIT))
    retry_spell_id = casting_melody_spell_id;
  casting_melody_spell_id = kInvalidSpellId;
  back_off_after_failure();
}

// The first retry goes out at once, so a lone missed note costs nothing. A failure that repeats
// is usually the server still busy (e.g. finishing a clicky song with a cast time, which sends no
// ack and is only waited out by MELODY_WAIT_USE_ITEM_TIMEOUT): retrying every frame then burns the
// whole retry budget in a fraction of a second and spams chat, so each further attempt waits
// RETRY_BACKOFF_STEP longer, up to RETRY_BACKOFF_MAX. A cast that holds for a second resets
// retry_count and with it the backoff.
void Melody::back_off_after_failure() {
  if (retry_count <= 1) return;
  const ULONGLONG wait = std::min<ULONGLONG>(static_cast<ULONGLONG>(retry_count - 1) * RETRY_BACKOFF_STEP,
                                             RETRY_BACKOFF_MAX);
  retry_not_before = GetTickCount64() + wait;
}

// A song is a spell cast with one of the bard skills: singing or an instrument.
static bool is_bard_song(const Zeal::GameStructures::SPELL &spell) {
  using namespace Zeal::GameEnums;
  switch (spell.Skill) {
    case SkillSinging:
    case SkillBrassInstruments:
    case SkillStringedInstruments:
    case SkillWindInstruments:
    case SkillPercussionInstruments:
      return true;
    default:
      return false;
  }
}

void __fastcall StopCast(int t, int u, BYTE reason, WORD spell_id) {
  ZealService::get_instance()->melody->handle_stop_cast_callback(reason, spell_id);
  ZealService::get_instance()->hooks->hook_map["StopCast"]->original(StopCast)(t, u, reason, spell_id);
}

void Melody::stop_current_cast() {
  Zeal::GameStructures::GAMECHARINFO *char_info = Zeal::Game::get_char_info();
  Zeal::GameStructures::Entity *self = Zeal::Game::get_self();
  if (char_info && self && self->ActorInfo && self->ActorInfo->CastingSpellId != kInvalidSpellId) {
    ZealService::get_instance()->hooks->hook_map["StopCast"]->original(StopCast)((int)char_info, 0, 0,
                                                                                 self->ActorInfo->CastingSpellId);
  }
  casting_melody_spell_id = kInvalidSpellId;
}

// This is a simplified version of the StopCast (StopSpellCast) call that should only be called
// if Zeal::Game::GameInternal::IsPlayerABardAndSingingASong() is true. It eliminates the
// "Your song ends" log spam and just performs the bard song stop processing.
static void stop_current_song_quietly() {
  Zeal::GameStructures::Entity *self = Zeal::Game::get_self();
  Zeal::GameStructures::GAMECHARINFO *char_info = Zeal::Game::get_char_info();

  // Clear the casting state per StopSpellCast() for the bard case.
  *(int16_t *)(0x007ce45a) = -1;  // Some sort of casting SPELL.SpellType cache.
  self->ActorInfo->CastingSpellId = kInvalidSpellId;
  self->ActorInfo->CastingSpellGemNumber = 0;
  self->ActorInfo->CastingTimeout = 0;
  self->ActorInfo->FizzleTimeout = 0;

  // For some reason stop is updating the RecastTimeouts, so duplicate that also.
  Zeal::GameStructures::SPELLMGR *get_spell_mgr();
  auto *spell_mgr = Zeal::Game::get_spell_mgr();
  for (int i = 0; i < GAME_NUM_SPELL_GEMS; ++i) {
    int spell_id = char_info->MemorizedSpell[i];
    if (spell_mgr && spell_id > 0 && spell_id < 4000) {
      auto *spell_info = spell_mgr->Spells[spell_id];
      if (spell_info && spell_info->RecastTime) continue;
    }
    self->ActorInfo->RecastTimeout[i] = 0;
  }

  // Send a message to the server to halt the bard song. This should not generate any packets
  // back to the client and with the casting ack handshake things should not get out of sync.
  const int OP_ManaChange = 0x417f;
  Zeal::Game::send_message(OP_ManaChange, nullptr, 0, 1);  // Stops the bard song on server.
}

void Melody::tick() {
  if (!is_active) return;

  Zeal::GameStructures::Entity *self = Zeal::Game::get_self();
  Zeal::GameStructures::GAMECHARINFO *char_info = Zeal::Game::get_char_info();

  // Handle various reasons to terminate Zeal automatically.
  if (!Zeal::Game::is_in_game() || !self || !self->ActorInfo || !char_info || (self->StandingState == Stance::Sit) ||
      (char_info->StunnedState) || (retry_count > RETRY_COUNT_END_LIMIT)) {
    end(true);
    return;
  }

  // Use timestamps to add minimum delay after casting ends and detect excessive retries.
  static ULONGLONG casting_visible_timestamp = GetTickCount64();
  static ULONGLONG start_of_cast_timestamp = casting_visible_timestamp;

  ULONGLONG current_timestamp = GetTickCount64();

  // Pause updates from start of zoning until a small delay after entering to let things stabilize.
  if (!enter_zone_time || current_timestamp < enter_zone_time + MELODY_ZONE_IN_DELAY) return;

  // Wait until the currently casting song / spell has finished.
  if (Zeal::Game::GetSpellCastingTime() != -1)  // Used by CCastingWnd.
  {
    casting_visible_timestamp = current_timestamp;
    // reset retry_count if the song cast window has been visible for > 1 second.
    if ((casting_visible_timestamp - start_of_cast_timestamp) > 1000) retry_count = 0;
    return;
  } else if (use_item_ack_state == UseItemState::CastRequested) {  // Block until server acknowledges.
    if ((current_timestamp - start_of_cast_timestamp) < 1000) return;
    Zeal::Game::print_chat("Melody use item start sync ack failure");
  }

  // Either casting finished normally or the retry logic has already kicked in,
  // so resetting the casting and deferred spell ids prevents the song from repeating
  // after this point.
  if (casting_melody_spell_id == deferred_spell_id) deferred_spell_id = kInvalidSpellId;
  casting_melody_spell_id = kInvalidSpellId;

  // Notes on client / server handshaking:
  // (1) Bard song casts:
  // A call to CastSpell() with a gem slot (not an item click) immediately sets both
  // ActorInfo->CastingSpellId and ->CastingSpellGemNumber to valid values. The server
  // receives the cast opcode and then after the cast timer sends an OP_MemorizeSpell
  // in Mob::CastedSpellFinished to update the gem bar state which sets the
  // CastingSpellGemNumber to 0xff. This provides a server ack that the casting timer
  // has expired and it is now in steady state bard song (so a new cast can start).
  // (2) Item clickies with non-bard songs:
  // Item clickies work differently. For standard use_item clickies, the CastingSpellId
  // is not set until an OP_BeginCast is received and the CastingSpellGemNumber is never
  // updated from the zero value set in stop casting. The item click cast does set
  // the CastingSpellCastTime to 0 and the FizzleTimer to current time + 10 sec. The
  // OP_BeginCast sets the CastingSpellId, CastingTimeout, and CastingSpellCastTime
  // which triggers the visible casting bar. For a normal clicky, the server sends an
  // OP_ManaChange that calls StopSpellCast() which sets CastingSpellId to kInvalidSpell,
  // CastingSpellGemNumber to zero, and CastingTimeout to 0.
  // (3) Item clickies with bard songs with non-zero cast times:
  // These are similar to normal clickies however the OP_ManaChange message is not sent
  // so we are stuck relying on a fixed MELODY_WAIT_USE_ITEM_TIMEOUT to be long enough
  // that the server will be finished and ready for the next melody start of cast.
  // (4) Item clickies with bard songs with zero cast times (e.g. Breath of Harmony)
  // These basically complete immediately so just skip all handshaking and go to idle.

  // The timeout is for debug reporting and recovery if something goes wrong.
  bool casting_active = self->ActorInfo->CastingSpellId != kInvalidSpellId;
  bool server_ack_cast = (self->ActorInfo->CastingSpellGemNumber == 0xff);
  if (casting_active && !server_ack_cast) {
    bool use_item_active = (use_item_ack_state != UseItemState::Idle);
    unsigned int timeout = use_item_active ? MELODY_WAIT_USE_ITEM_TIMEOUT : MELODY_WAIT_TIMEOUT;
    bool timed_out = ((current_timestamp - casting_visible_timestamp) > timeout);
    if (!timed_out)
      return;  // Wait for the ack.
    else if (!use_item_active) {
      Zeal::Game::print_chat("Melody: ack time out error, trying to restart");
      stop_current_cast();  // Something is out of sync. Abort current casting.
    }
  }

  // Handles situations like trade windows, looting (Stance::Bind), and ducking.
  if (!Zeal::Game::get_game() || !Zeal::Game::get_game()->IsOkToTransact() || self->StandingState != Stance::Stand)
    return;

  if (current_timestamp < retry_not_before) return;  // Backing off after repeated failures.

  // An item step in the rotation becomes the pending click, then plays through the same path as
  // a queued /useitem.
  use_item_ack_state = UseItemState::Idle;
  queue_item_step_if_next();

  // Execute a pending use_item() call here
  if (use_item_index >= 0) {
    stop_current_cast();  // Terminate bard song (if active) in order to cast.
    Zeal::GameStructures::GAMEITEMINFO *used_item = nullptr;
    bool success = (use_item_timeout >= current_timestamp) && Zeal::Game::use_item(use_item_index, false, &used_item);
    use_item_index = -1;
      if (success) {
      use_item_ack_state = UseItemState::CastRequested;
      if (used_item && used_item->Common.CastTime == 0 && Zeal::Game::get_game()->IsOkToTransact())
        use_item_ack_state = UseItemState::Idle;  // Instant bard clicky. Can't use request/ack format.
      // The handshake cannot be trusted to hold the next song back after a click with a cast
      // time: the stop of the song before the click makes the server send an OP_ManaChange, and
      // when that beats the click's OP_BeginCast it reads as "done" (Composers Chestplate, a 3 s
      // cast: the next song went out in the same second as the glow and was refused ~20 times in
      // 2 s, ending the melody). So such a click holds the
      // next step for its cast time, its spell's recovery, and the margin the ack path waits after
      // the cast bar (MELODY_WAIT_USE_ITEM_TIMEOUT, case (3) above) - without that margin the next
      // song left right as the Chestplate's landed and the melody ended. Instant clicks
      // (Breath of Harmony) get no hold: their spell's cast time does not apply to the click, and
      // holding for it put ~3 s gaps in the rotation. Clicks whose spell is not
      // a bard song (the Girdle's damage shield) get none either: they show a cast bar and end with
      // their own OP_ManaChange (case (2)), which already holds the next song, and the extra
      // recovery + margin left a long pause after the last step of the rotation.
      // The retry backoff stays the fallback.
      const int spell_id = used_item ? used_item->Common.SpellIdEx : 0;
      auto *spell_mgr = Zeal::Game::get_spell_mgr();
      const Zeal::GameStructures::SPELL *spell =
          (spell_mgr && spell_id > 0 && spell_id < GAME_NUM_SPELLS) ? spell_mgr->Spells[spell_id] : nullptr;
      if (used_item && used_item->Common.CastTime > 0 && spell && is_bard_song(*spell)) {
        const ULONGLONG busy = static_cast<ULONGLONG>(used_item->Common.CastTime) + spell->FizzleTime +
                               MELODY_WAIT_USE_ITEM_TIMEOUT;
        retry_not_before = std::max<ULONGLONG>(retry_not_before, current_timestamp + busy);
      }
      start_of_cast_timestamp = current_timestamp;    // Used in timeout check.
      casting_visible_timestamp = current_timestamp;  // Insta-clickies may not update.
      return;
    }
  }

  int current_gem = get_next_gem_index();
  if (current_gem < 0) return;  // Next song wasn't ready (possibly deferred), so skip and try again next tick.
  WORD current_gem_spell_id = char_info->MemorizedSpell[current_gem];
  if (current_gem_spell_id == kInvalidSpellId) return;  // simply skip empty gem slots (unexpected to occur)

  // handle a common issue of no target gracefully (notify once and skip to next song w/out retry failures).
  if (Zeal::Game::get_spell_mgr() &&
      (Zeal::Game::get_spell_mgr()->Spells[current_gem_spell_id]->TargetType ==
           Zeal::GameEnums::SpellTargetType::Target ||
       Zeal::Game::get_spell_mgr()->Spells[current_gem_spell_id]->TargetType ==
           Zeal::GameEnums::SpellTargetType::TargetedAE) &&
      !Zeal::Game::get_target()) {
    Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, "You must first select a target for spell %i", current_gem + 1);
    retry_count++;  // Re-use the retry logic to limit runaway spam if entire song list is target-based.
    back_off_after_failure();
    return;
  }

  if (Zeal::Game::GameInternal::IsPlayerABardAndSingingASong())
    stop_current_song_quietly();  // Use the custom stop to reduce log spam.
  else
    stop_current_cast();  // Just in case call. Does nothing if casting not active (expected).

  if (char_info->cast(current_gem, current_gem_spell_id, 0, -1))
    casting_melody_spell_id = current_gem_spell_id;  // Successful start of cast; arm retry.
  else {
    retry_count++;  // Re-use the retry logic to limit runaway spam if entire song list is invalid.
    back_off_after_failure();
  }

  start_of_cast_timestamp = current_timestamp;
}

// Returns true if the gem's recast timer is not active.
bool Melody::is_gem_ready(int gem_index) {
  bool invalid_index = gem_index < 0 || gem_index >= GAME_NUM_SPELL_GEMS;
  auto self = Zeal::Game::get_self();
  auto actor_info = self ? self->ActorInfo : nullptr;
  auto char_info = Zeal::Game::get_char_info();
  auto display = Zeal::Game::get_display();
  if (invalid_index || !self || !actor_info || !char_info || !display) return true;  // Default to true.

  int game_time = display->GameTimeMs;
  int spell_id = char_info->MemorizedSpell[gem_index];
  if (spell_id != kInvalidSpellId && actor_info->RecastTimeout[gem_index] > game_time) return false;

  return true;
}

// Returns the gem index of the next song to cast (based on retries, deferred, index loop).
int Melody::get_next_gem_index() {
  auto char_info = Zeal::Game::get_char_info();

  // First check if there is a valid song to retry.
  // songs is 'guaranteed' to have a valid gem index from start().
  if (char_info && retry_spell_id != kInvalidSpellId) {
    int spell_id = retry_spell_id;
    retry_spell_id = kInvalidSpellId;  // Reset so it only retries once.
    for (const auto &step : songs)
      if (!step.is_item() && char_info->MemorizedSpell[step.gem] == spell_id) return step.gem;
  }

  // Then check if there is already a deferred song.
  if (char_info && deferred_spell_id) {
    for (const auto &step : songs)
      if (!step.is_item() && char_info->MemorizedSpell[step.gem] == deferred_spell_id && is_gem_ready(step.gem))
        return step.gem;
  }

  // Finally if neither of those, advance to the next song. If the next step is an item, stay put:
  // queue_item_step_if_next() turns it into the pending click on the next tick, once the retry and
  // deferred songs above are out of the way.
  int next = current_index + 1;
  if (next >= static_cast<int>(songs.size()) || next < 0) next = 0;
  if (songs[next].is_item()) return -1;
  current_index = next;
  int current_gem = songs[current_index].gem;
  if (is_gem_ready(current_gem)) return current_gem;

  // The song wasn't ready so try to defer. Our defer queue supports only one song.
  if (deferred_spell_id == kInvalidSpellId)
    deferred_spell_id = char_info ? char_info->MemorizedSpell[current_gem] : kInvalidSpellId;

  return -1;  // Signal the loop to try again next tick.
}

// The player state gets wiped on zoning, so pause melody during the transition time and
// rewind the state if there was an interrupted song cast.
void Melody::handle_deactivate_ui() {
  enter_zone_time = 0;  // Pauses melody processing loop

  // Bail out if melody not active (bard or not) and if not a bard singing a song.
  if (!is_active || !Zeal::Game::GameInternal::IsPlayerABardAndSingingASong()) return;

  // Re-use the interrupted logic to rewind melody to cleanly continue after zone in.
  // CastingSpellId must be valid to have gotten here.
  handle_stop_cast_callback(3, Zeal::Game::get_self()->ActorInfo->CastingSpellId);
}

// Extra server handshake tracking for clicky casts.
bool Melody::handle_opcode(int opcode) {
  if (use_item_ack_state == UseItemState::Idle) return false;

  const int OP_BeginCast = 0x40a9;      // Sent in responce to a CastSpell().
  const int OP_ManaChange = 0x417f;     // Sent after normal clickiese or song end.
  const int OP_MemorizeSpell = 0x4182;  // Sent after normal bard song finishes casting.
  if (opcode == OP_ManaChange || opcode == OP_MemorizeSpell) {
    use_item_ack_state = UseItemState::Idle;  // Needed for insta clicky to clear wait for begin cast.
  } else if (opcode == OP_BeginCast && use_item_ack_state == UseItemState::CastRequested) {
    use_item_ack_state = UseItemState::CastStarted;  // Needed for pending cast start of clickies.
  }
  return false;
}

Melody::Melody(ZealService *zeal) {
  if (!Zeal::Game::is_new_ui()) return;  // Old UI not supported.

  zeal->callbacks->AddGeneric([this]() { tick(); });
  zeal->callbacks->AddGeneric([this]() { end(); }, callback_type::CharacterSelect);
  zeal->callbacks->AddGeneric([this]() { handle_deactivate_ui(); }, callback_type::DeactivateUI);
  zeal->callbacks->AddGeneric([this]() { enter_zone_time = GetTickCount64(); }, callback_type::EnterZone);
  zeal->callbacks->AddPacket([this](UINT opcode, char *buffer, UINT len) { return handle_opcode(opcode); },
                             callback_type::WorldMessage);
  zeal->hooks->Add("StopCast", 0x4cb510, StopCast, hook_type_detour);  // Hook in to end melody as well.
  zeal->commands_hook->Add(
      "/melody", {"/mel"}, "Bard only, auto cycles up to 5 songs and item clicks (i22, i2.3, iName).",
      [this](std::vector<std::string> &args) {
        if (args.size() > 1 && args[1] == "resume") {
          resume();  // Continues an interrupted melody.
          return true;
        }

        end(true);  // otherwise any active melodies are always terminated

        if (!Zeal::Game::get_char_info() || Zeal::Game::get_char_info()->Class != Zeal::GameEnums::ClassTypes::Bard) {
          Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, "Only bards can keep a melody.");
          return true;
        }

        std::vector<MelodyStep> new_songs;
        int song_count = 0;
        for (size_t i = 1; i < args.size(); i++)  // start at argument 1 because 0 is the command itself
        {
          MelodyStep step;
          if (!parse_step(args[i], &step)) return true;
          if (!step.is_item()) song_count++;
          new_songs.push_back(step);
        }
        if (song_count > 5) {
          Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, "A melody can only consist of up to 5 songs.");
          return true;
        }
        if (new_songs.size() > 10) {
          Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, "A melody can only consist of up to 10 steps.");
          return true;
        }
        start(new_songs);
        return true;  // return true to stop the game from processing any further on this command, false if you want to
                      // just add features to an existing cmd
      });

  // Hooking '/stopsong' to address a client bug: '/stopsong' during a clicky-casted causes client/server desync in
  // casting state:
  // - Client cast bar disappears, but the spell is not interrupted on the server side.
  //   - Client is wiping the casting state and bailing out without sending a message at 0x004cb5bc for bards.
  // - To fix, we will just ignore '/stopsong' if the bard isn't singing bard song
  zeal->commands_hook->Add(
      "/stopsong", {}, "Stops the current bard song from casting", [this](std::vector<std::string> &args) {
        if (Zeal::Game::GameInternal::IsPlayerABardAndSingingASong())
          return false;  // Let regular /stopsong logic run to interrupt it

        return true;  // casting a non-gem'd spell (likely a clicky). Prevent '/stopsong' from running.
      });
}

Melody::~Melody() {}
