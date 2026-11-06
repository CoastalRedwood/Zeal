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
//   - Only ints or bard song click items as parameters
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
constexpr unsigned int USE_ITEM_QUEUE_TIMEOUT =
    3650;  // Max duration a useitem will stay queued for before giving up (mostly to prevent ultra-stale clicks).

enum UseItemState : int { Idle = 0, CastRequested, CastStarted };

// Parses the token argument to configure the melody step object.
MelodyStep::MelodyStep(const std::string &token) {
  key = token;

  // The MelodyStep() initializes as invalid. Try to parse the token to make it valid.
  // First check the standard numeric gem format of 1 to 8.
  int number = -1;
  if (Zeal::String::tryParse(token, &number, true)) {
    if (number > 0 && number <= GAME_NUM_SPELL_GEMS)
      gem = number - 1;  // Convert from base 1 UI to base 0 indexing.
    else
      Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, "Parsing error: Invalid gem # %d, use 1-8", number);
    return;
  }

  // The non-gem format should be either #.# or the start of an item name.
  // Check for the #.# item slot format. Item names do not have a '.' in them, so filter by that.
  const size_t dot = token.find('.');
  if (dot != std::string::npos) {
    int bag = 0, bagslot = 0;
    if (Zeal::String::tryParse(token.substr(0, dot), &bag, true) &&
        Zeal::String::tryParse(token.substr(dot + 1), &bagslot, true) && bag >= 0 &&
        bag <= GAME_NUM_INVENTORY_PACK_SLOTS) {
      if (bag == 0) {  // Equipped slots.
        if (bagslot >= 0 && bagslot <= GAME_PACKS_SLOTS_END && bagslot != GAME_EQUIPMENT_SLOTS_END) {
          if (bagslot < GAME_EQUIPMENT_SLOTS_END)
            bagslot += GAME_EQUIPMENT_SLOTS_START;  // Copy /useitem's translation.
          item_slot_id = bagslot;
          return;
        }
      } else if (bagslot >= 1 && bagslot <= GAME_NUM_CONTAINER_SLOTS) {
        item_slot_id = GAME_CONTAINER_SLOTS_START + (bag - 1) * GAME_NUM_CONTAINER_SLOTS + bagslot - 1;
        return;
      }
    }
    Zeal::Game::print_chat(
        USERCOLOR_SPELL_FAILURE,
        "Invalid inventory location %s, use <#.#> like 0.10 for Ranged (equipment slot 10) or 2.3 for bag 2, slot 3",
        key.c_str());
    return;
  }

  // That failed so try the name search for the token.
  std::string name = token;
  std::replace(name.begin(), name.end(), '_', ' ');
  item_slot_id = Zeal::Game::find_use_item_by_name(name, true);  // Returns -1 if fails to match.
  if (item_slot_id < 0)
    Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, "Failed to find a usable clickable item that matches %s",
                           key.c_str());
}

// Returns true if this step points to an inventory slot with an item that has a usable bard song clicky effect.
bool MelodyStep::is_item_valid(bool do_print) const {
  if (!is_item()) return false;
  bool is_equipped = (GAME_EQUIPMENT_SLOTS_START <= item_slot_id && item_slot_id <= GAME_EQUIPMENT_SLOTS_END);
  const auto *item = Zeal::Game::get_inventory_item_from_global_slot_id(item_slot_id, false);
  if (!item || !Zeal::Game::is_valid_item_to_use(item, is_equipped, false)) {
    if (do_print)
      Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, "Melody Error: item %s is not a usable clickable item",
                             key.c_str());
    return false;
  }

  // If it got to here, it should have a valid spell index so ensure it is a bard-song spell.
  const auto *spell_mgr = Zeal::Game::get_spell_mgr();
  const auto *spell = spell_mgr ? spell_mgr->Spells[item->Common.SpellIdEx] : nullptr;
  if (spell && spell->IsBardSong()) return true;

  if (do_print)
    Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, "Melody Error: item %s does not have a clickable bard song effect",
                           key.c_str());
  return false;
}

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

  // Ensure all steps either have valid memorized songs in the gem slots or valid items in the item_slot_id.
  // An invalid step is skipped with a complaint.
  std::vector<MelodyStep> valid_songs;
  for (const MelodyStep &step : new_songs) {
    if (step.is_item()) {
      if (!step.is_item_valid()) {
        Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, "Error: skipping %s (not a valid bard song item)",
                               step.key.c_str());
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
    else
      valid_songs.push_back(step);
  }

  if (valid_songs.empty() && !new_songs.empty()) {
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
  casting_melody_spell_id = kInvalidSpellId;
  retry_spell_id = kInvalidSpellId;
  deferred_spell_id = kInvalidSpellId;
  use_item_index = -1;
  // Do not reset use_item_ack_state in case this is an aligned melody re-start.
  if (is_active) Zeal::Game::print_chat(USERCOLOR_SPELLS, "You begin playing a melody.");
  return true;
}

void Melody::resume() {
  if (is_active) return;  // Already running, nothing to do.

  start(songs, true);
}

void Melody::end(bool do_print, bool full_stop) {
  if (is_active) {
    // songs is left alone and and current_index is rewound (if needed) to before the interrupted song.
    if (casting_melody_spell_id != kInvalidSpellId) {
      current_index--;
      if (current_index < 0) {  // Handle wraparound.
        current_index = songs.size() - 1;
      }
    }

    if (is_verbose)
      Zeal::Game::print_chat("Melody end: spell ID: %d, state: %d", casting_melody_spell_id, use_item_ack_state);

    is_active = false;
    retry_count = 0;
    casting_melody_spell_id = kInvalidSpellId;
    retry_spell_id = kInvalidSpellId;
    deferred_spell_id = kInvalidSpellId;
    use_item_index = -1;

    // Do not reset the use_item_ack_state (could be in the middle of a clicky cast) unless a full stop.
    if (full_stop) use_item_ack_state = UseItemState::Idle;

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
  // Server SendSpellBarEnable() calls generate OP_ManaChange packets with reason = 3.
  // This is called by the server's CastedSpellFinished() for non-bard song clickies and by
  // InterruptSpell() if it wasn't a client triggered bard song stop.
  if (reason != 3 || !is_active) {
    end(true);
    return;
  }

  // Support rewinding to the interrupted last song (primarily for missed notes, so not items).
  // Note that reason code == 3 is shared by missed notes as well as other failures (such as the spell
  // is not allowed in the zone), so we use a retry_count to limit the spammy loop that is
  // difficult to click off with UI spell gems (/stopsong, /melody still work fine). The modulo
  // check skips the rewind so it advances to the next song but then allows that song to retry.
  bool is_rewindable_cast = (casting_melody_spell_id == spell_id) && (use_item_ack_state == UseItemState::Idle);
  if (is_rewindable_cast && (++retry_count % RETRY_COUNT_REWIND_LIMIT)) retry_spell_id = casting_melody_spell_id;

  if (is_verbose)
    Zeal::Game::print_chat("Melody stop_cast: %d vs %d, state: %d", spell_id, casting_melody_spell_id,
                           use_item_ack_state);

  if (spell_id == casting_melody_spell_id)
    use_item_ack_state = UseItemState::Idle;  // Normal clicky cast finished if it wasn't idle.

  casting_melody_spell_id = kInvalidSpellId;
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
  } else if (use_item_ack_state == UseItemState::CastRequested) {
    // Wait until server acknowledges the use item cast request with an OP_BeginCast packet.
    if ((current_timestamp - start_of_cast_timestamp) < 750) return;  // Allow 750s of response lag.
    Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, "Melody: Timeout waiting for the server to send item begin cast");
  } else if (use_item_ack_state == UseItemState::CastStarted) {
    // Wait until server acknowledges the end of cast with the stop for normal or action for bard songs.
    if ((current_timestamp - casting_visible_timestamp) < 750) return;  // Allow 750s of response lag.
    Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, "Melody: Timeout waiting for the server to send item end cast");
  }

  // Either casting finished normally or the retry logic has already kicked in,
  // so resetting the casting and deferred spell ids prevents the song from repeating
  // after this point.
  if (casting_melody_spell_id == deferred_spell_id) deferred_spell_id = kInvalidSpellId;
  casting_melody_spell_id = kInvalidSpellId;

  // Notes on client / server handshaking:
  // (1) Bard song gem casts:
  // A call to CastSpell() with a gem slot (not an item click) immediately sets both
  // ActorInfo->CastingSpellId and ->CastingSpellGemNumber to valid values which makes
  // the casting bar visible. The server receives the cast opcode and then after the
  // cast timer sends an OP_MemorizeSpell in Mob::CastedSpellFinished to update the
  // gem bar state which sets the CastingSpellGemNumber to 0xff. This provides a server
  // ack that the casting timer has expired and it is now in steady state bard song
  // (so a new melody cast can start).
  // (2) Item clickies with non-bard songs (normal clickies):
  // Item clickies work differently. For normal use_item clickies with cast times, the
  // CastingSpellId is not set until an OP_BeginCast is received and the CastingSpellGemNumber
  // is never updated from the zero value set in stop casting. The item click cast does set
  // the CastingSpellCastTime to 0 and the FizzleTimer to current time + 10 sec. The
  // OP_BeginCast sets the CastingSpellId, CastingTimeout, and CastingSpellCastTime
  // which triggers the visible casting bar. When casting finishes, the server sends an
  // OP_ManaChange that calls StopSpellCast() which sets CastingSpellId to kInvalidSpell,
  // CastingSpellGemNumber to zero, and CastingTimeout to 0. Normal insta-clickies skip
  // the OP_BeginCast / visible casting bar completely and just receive the StopSpellCast()
  // when the sequence is complete and ready to cast the next song.
  // (3) Item clickies with bard songs with cast times:
  // These start similar to normal clickies however they really are bard song casts (so
  // the cast is interruptible) however the server never directly sends back any
  // message that casting is finished. There is no OP_MemorizeSpell since the gem #
  // isn't valid and no OP_ManaChange message since it is a bard song. The visible casting
  // bar works for most of the cast time but there can still be drift between the client
  // and server, so in order to get a positive handshake we use the OP_Action packet from
  // when the spell hits to signal casting is truly finished and Melody can advance.
  // (4) Item clickies with insta-cast bard songs:
  // These are the simplest since there is no server handshaking and no server/client
  // state to get out of sync, so Melody just sends the item cast request and moves on
  // to the next song step.

  // The timeouts are for debug reporting and recovery if something goes wrong.
  bool casting_active = self->ActorInfo->CastingSpellId != kInvalidSpellId;
  bool server_ack_cast = (self->ActorInfo->CastingSpellGemNumber == 0xff);
  if (casting_active && !server_ack_cast) {
    bool use_item_active = (use_item_ack_state != UseItemState::Idle);
    unsigned int timeout = use_item_active ? MELODY_WAIT_USE_ITEM_TIMEOUT : MELODY_WAIT_TIMEOUT;
    bool timed_out = ((current_timestamp - casting_visible_timestamp) > timeout);
    if (!timed_out)
      return;  // Wait for the ack.
    else if (!use_item_active) {
      Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, "Melody: ack time out error, trying to restart");
      stop_current_cast();  // Something is out of sync. Abort current casting.
    }
  }

  // Handles situations like trade windows, looting (Stance::Bind), and ducking.
  if (!Zeal::Game::get_game() || !Zeal::Game::get_game()->IsOkToTransact() || self->StandingState != Stance::Stand)
    return;

  // Execute a pending use_item() call here
  use_item_ack_state = UseItemState::Idle;
  if (activate_use_item()) return;

  int current_gem = get_next_gem_index();
  if (current_gem < 0) return;  // Either the next song wasn't ready (possibly deferred) or activated an item.
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
    return;
  }

  if (Zeal::Game::GameInternal::IsPlayerABardAndSingingASong())
    stop_current_song_quietly();  // Use the custom stop to reduce log spam.
  else
    stop_current_cast();  // Just in case call. Does nothing if casting not active (expected).

  if (char_info->cast(current_gem, current_gem_spell_id, 0, -1))
    casting_melody_spell_id = current_gem_spell_id;  // Successful start of cast; arm retry.
  else
    retry_count++;  // Re-use the retry logic to limit runaway spam if entire song list is invalid.

  start_of_cast_timestamp = current_timestamp;
}

// Activates (casts) from an item if there is a valid queued use_item_index.
bool Melody::activate_use_item() {
  if (use_item_index < 0) return false;

  if (Zeal::Game::GameInternal::IsPlayerABardAndSingingASong())
    stop_current_song_quietly();  // Use the custom stop to reduce log spam.
  else
    stop_current_cast();  // Not expected to be needed but just in case.

  Zeal::GameStructures::GAMEITEMINFO *used_item = nullptr;
  ULONGLONG current_timestamp = GetTickCount64();
  bool success = (use_item_timeout >= current_timestamp) && Zeal::Game::use_item(use_item_index, false, &used_item);
  if (success) {
    casting_melody_spell_id = used_item ? used_item->Common.SpellIdEx : kInvalidSpellId;
    if (used_item && used_item->Common.CastTime <= 0) {  // Blazing vambraces == -1 vs 0 for most instants.
      // Instant clickies. Bard song insta-clicks do not receive an OP_ManaChange message (stop_cast) so there is
      // no ack handshaking required while normal clickies do so we set the CastStarted state to expect it.
      const auto *spell = Zeal::Game::get_spell_from_id(casting_melody_spell_id);
      bool is_bard_song = (spell && spell->IsBardSong());
      use_item_ack_state = is_bard_song ? UseItemState::Idle : UseItemState::CastStarted;
    } else {
      // Clickies with a cast time have a time gap where the client is not aware of the cast yet and is waiting
      // for the server to send back OP_BeginCast from activating the item. The CastRequested blocks the
      // melody tick() from trying to advance to the next song step.
      use_item_ack_state = UseItemState::CastRequested;
    }
    start_of_cast_timestamp = current_timestamp;    // Used in timeout check.
    casting_visible_timestamp = current_timestamp;  // Insta-clickies may not update.
  }
  if (is_verbose) Zeal::Game::print_chat("Melody use item: %d, state: %d", casting_melody_spell_id, use_item_ack_state);
  use_item_index = -1;
  return success;
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

// Returns the gem index of the next song to cast (based on retries, deferred, index loop) or
// internally activates an item step if that is the next valid step.
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

  // Finally if neither of those, advance to the next song step.
  current_index++;
  if (current_index >= songs.size() || current_index < 0) current_index = 0;

  // If the next step is an item, then check if it is valid and if so immediately activate it.
  if (songs[current_index].is_item()) {
    if (songs[current_index].is_item_valid()) {
      use_item(songs[current_index].item_slot_id);
      activate_use_item();
    }
    return -1;  // Done with this tick cycle (no gem to cast).
  }

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

// Extra server handshake tracking for clicky casts. The OP_BeginCast packet is used to
// transition the use_item_ack_state from CastRequested to CastStarted to cover the gap
// until casting actually starts on the client. The OP_Action packet is used to detect
// the end of casting for Bard song clickies since the server doesn't send the OP_ManaChange
// that triggers the handle_stop_cast_callback() like normal clickies.
bool Melody::handle_opcode(int opcode, char *buffer, UINT len) {
  if (!is_active || use_item_ack_state == UseItemState::Idle) return false;

  if (opcode == Zeal::Packets::BeginCast && use_item_ack_state == UseItemState::CastRequested) {
    const auto *self = Zeal::Game::get_self();
    if (!self || !buffer || len < sizeof(Zeal::Packets::BeginCast_Struct)) return false;

    const auto *begin_cast = reinterpret_cast<const Zeal::Packets::BeginCast_Struct *>(buffer);
    if (begin_cast->caster_id == self->SpawnId) use_item_ack_state = UseItemState::CastStarted;

    if (is_verbose && begin_cast->caster_id == self->SpawnId && begin_cast->spell_id != casting_melody_spell_id)
      Zeal::Game::print_chat("Melody BeginCast mismatch: spell ID: %d vs %d", begin_cast->spell_id,
                             casting_melody_spell_id);
  } else if (opcode == Zeal::Packets::Action && use_item_ack_state == UseItemState::CastStarted) {
    if (!buffer || len < sizeof(Zeal::Packets::Action_Struct)) return false;

    // First check if this is a spell action with this client as the source. Note that we don't check
    // for the success flag (buff_unknown == 0x04) since we only care that the cast completed and
    // the second packet with success may not get sent if blocked for some reason.
    const auto *self = Zeal::Game::get_self();
    const auto *action = reinterpret_cast<const Zeal::Packets::Action_Struct *>(buffer);
    if (!self || action->source != self->SpawnId || action->type != 231) return false;  // Not our spell.

    // Then confirm the spell id matches the one we expect from our melody start of cast.
    if (action->spell != casting_melody_spell_id || casting_melody_spell_id == kInvalidSpellId) {
      if (is_verbose)
        Zeal::Game::print_chat("Melody Action mismatch: %d vs %d", action->spell, casting_melody_spell_id);
      return false;
    }

    // And finally check if this is a bard song clicky that needs a manual ack when casting is finished.
    const auto *spell = Zeal::Game::get_spell_from_id(casting_melody_spell_id);
    if (spell && spell->IsBardSong() && self->ActorInfo && self->ActorInfo->CastingSpellId == casting_melody_spell_id &&
        self->ActorInfo->CastingSpellGemNumber == 0) {
      self->ActorInfo->CastingSpellGemNumber = 0xff;  // Set the manual ack to avoid the timeout delay.
      casting_melody_spell_id = kInvalidSpellId;
      use_item_ack_state = UseItemState::Idle;  // Disable to prevent responding to rewinds or future action opcodes.

      if (is_verbose) Zeal::Game::print_chat("Melody: Cast finished for spell ID: %d", action->spell);
    }
  }

  return false;
}

// Development debug dump of current state.
void Melody::dump() const {
  Zeal::Game::print_chat("is_active: %d, current_index: %d, casting_id: %d", is_active, current_index,
                         casting_melody_spell_id);
  Zeal::Game::print_chat("retry_count: %d, retry_id: %d, deferred_id: %d", retry_count, retry_spell_id,
                         deferred_spell_id);
  Zeal::Game::print_chat("use_item: %d, ack_state: %d, item_timeout: %llu", use_item_index, use_item_ack_state,
                         use_item_timeout);
  Zeal::Game::print_chat("enter_zone_time: %llu, casting_visible: %llu, start_of_cast: %llu", enter_zone_time,
                         casting_visible_timestamp, start_of_cast_timestamp);
}

Melody::Melody(ZealService *zeal) {
  if (!Zeal::Game::is_new_ui()) return;  // Old UI not supported.

  zeal->callbacks->AddGeneric([this]() { tick(); });
  zeal->callbacks->AddGeneric([this]() { end(); }, callback_type::CharacterSelect);
  zeal->callbacks->AddGeneric([this]() { handle_deactivate_ui(); }, callback_type::DeactivateUI);
  zeal->callbacks->AddGeneric([this]() { enter_zone_time = GetTickCount64(); }, callback_type::EnterZone);
  zeal->callbacks->AddPacket([this](UINT opcode, char *buffer, UINT len) { return handle_opcode(opcode, buffer, len); },
                             callback_type::WorldMessage);
  zeal->hooks->Add("StopCast", 0x4cb510, StopCast, hook_type_detour);  // Hook in to end melody as well.
  zeal->commands_hook->Add(
      "/melody", {"/mel"},
      "Bard only, auto cycles up to 5 songs or bard song item clicks (0.10, 2.3, Case_sensitive_name_search).",
      [this](std::vector<std::string> &args) {
        if (args.size() > 1 && args[1] == "resume") {
          resume();  // Continues an interrupted melody.
          return true;
        }

        if (args.size() == 2 && args[1] == "dump") {
          dump();
          return true;
        }

        if (args.size() == 2 && args[1] == "verbose") {
          is_verbose = !is_verbose;  // Just toggle state.
          return true;
        }

        end(true, false);  // otherwise any active melodies are always terminated

        if (!Zeal::Game::get_char_info() || Zeal::Game::get_char_info()->Class != Zeal::GameEnums::ClassTypes::Bard) {
          Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, "Only bards can keep a melody.");
          return true;
        }

        if (args.size() > 6) {
          Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, "A melody can only consist of up to 5 songs.");
          return true;
        }

        std::vector<MelodyStep> new_songs;
        for (size_t i = 1; i < args.size(); i++)  // start at argument 1 because 0 is the command itself
        {
          MelodyStep step(args[i]);
          if (!step.is_valid(true)) {
            Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, "Melody example: /melody 1 2 3 0.10 Composers_Chestplate");
            Zeal::Game::print_chat(
                USERCOLOR_SPELL_FAILURE,
                "Specify songs with 1-8 for gems or use either bag#.slot# (bag 0 = equipped slots) or "
                "case sensitive start of item name with underscore replacing spaces.");
            return true;
          }
          new_songs.push_back(step);
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
