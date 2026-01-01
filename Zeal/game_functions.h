#pragma once
#include <filesystem>

#include "game_structures.h"
#include "game_ui.h"

enum Stance {
  Stand = 0x64,
  NotInControl = 0x66,  // Fear, Charm, etc
  Bind = 0x69,
  Sit = 0x6E,
  Duck = 0x6F,
  Feign = 0x73,
  Dead = 0x78
};

namespace Zeal {
namespace Game {
namespace GameInternal {
static int fn_GetLabelFromGame = 0x436680;
static int fn_GetGaugeLabelFromGame = 0x43600d;
static int fn_finalizeloot = 0x47f2bd;
static int fn_releaseloot = 0x426576;
static int fn_targetnearestnpc = 0x4ac17f;
static int *fn_s_t3dGetActorLocation_ptr = (int *)0x7f99c8;
static int *fn_s_s3dGetWorldVisibleActorList = (int *)0x7f9850;
static int *fn_s_s3dGetCameraLocation = (int *)0x7f99D4;
static int *fn_s_t3dSetCameraLocation = (int *)0x7f9ae4;
static int fn_handle_mouselook = 0x4DB384;
static int fn_initkeyboardassignments = 0x42A9D7;
static int fn_executecmd = 0x54050c;
static int fn_interpretcmd = 0x54572f;
// static int fn_main_loop = 0x5473c3;
/*inline int fn_loadoptions = 0x536CE0;*/
static int fn_KeyboardPageHandleKeyboardMsg = 0x42c4fb;

static mem::function<void __fastcall(int t, int unused, char *data, int unknown)> DoPercentConvert = 0x00538110;
static mem::function<void __cdecl(const char *data)> gamelog = 0x005240dc;
static mem::function<char __fastcall(void *this_display, int unused_edx, Zeal::GameStructures::Entity *viewer,
                                     Zeal::GameStructures::Entity *target)>
    is_invisible = 0x4afa90;  // can_target
static mem::function<char __fastcall(int, int, int, int, float *, float, UINT32)> get_world_visible_actor_list =
    0x7f9850;
static mem::function<short __fastcall(int, int)> get_max_mana = 0x4B9483;
static mem::function<short __fastcall(int, int)> get_cur_mana = 0x4b9450;
static mem::function<int __cdecl(int, Vec3 *)> t3dGetRegionNumberFromWorldAndXYZ = 0x0;
static mem::function<void __fastcall(DWORD, int unused, DWORD)> ui_something = 0x536bae;
static mem::function<float __fastcall(void *this_display, int unused_edx, float input_heading)> fix_heading = 0x4a2eed;
static mem::function<void __cdecl()> ProcessMouseEvent = 0x525db4;
static mem::function<void __fastcall(int, int, float)> MouseLook = 0x4db384;
static mem::function<void __fastcall(DWORD, int unused, DWORD)> proc_mouse = 0x537707;
static mem::function<void __fastcall(DWORD, int unused, int cmd, int str_id, int category)> InitKeyBind =
    0x42B21D;  // arguments coptionswnd ptr, cmd, string_id, category
static mem::function<int __cdecl(Zeal::GameUI::CXSTR *, const char *format)> CXStr_PrintString = 0x578110;
static mem::function<int __fastcall(void *this_game, int unused_edx)> LoadOptions = 0x536CE0;  // Game ::loadOptions()
static mem::function<int __cdecl(int key, int type)> readKeyMapFromIni = 0x525520;
static mem::function<void __cdecl(Zeal::GameStructures::GAMECHARINFO *_char, Zeal::GameStructures::_GAMEITEMINFO **Item,
                                  int)>
    auto_inventory = 0x4F0EEB;
static mem::function<int __cdecl()> UI_ChatInputCheck = 0x54042d;
static mem::function<bool __cdecl()> IsNoSlashWndActive =
    0x00545bbd;  // Returns false if merchant, loot, trade, and bank windows are not activated.
static mem::function<bool __cdecl()> IsPlayerABardAndSingingASong = 0x0040a74a;  // Does internal pointer checking.
static mem::function<int __fastcall(void *this_raid, int unused_edx, const char *)> send_raid_chat =
    0x0049e2e8;  // CRaid::SendRaidChat()
static mem::function<int __cdecl(Zeal::GameStructures::Entity *, const char *)> do_say = 0x4f8172;
static mem::function<int __cdecl(Zeal::GameStructures::Entity *, const char *)> do_tell = 0x004f8367;
static mem::function<int __cdecl(Zeal::GameStructures::Entity *, const char *)> do_guildsay = 0x004f4bd1;
static mem::function<int __cdecl(Zeal::GameStructures::Entity *, const char *)> do_gsay = 0x004f82a8;
static mem::function<int __cdecl(Zeal::GameStructures::Entity *, const char *)> do_auction = 0x004f8325;
static mem::function<int __cdecl(Zeal::GameStructures::Entity *, const char *)> do_ooc = 0x004f8346;
static mem::function<float __fastcall(int, int)> encum_factor = 0x4bb9c7;
static mem::function<float __fastcall(int, int, int, int)> OpenContainer = 0x4168bd;
static mem::function<float __fastcall(int, int)> CloseAllContainers = 0x416a43;
static mem::function<int __fastcall(int, int)> GetFocusWnd = 0x5a07c0;
static mem::function<int __fastcall(int, int, int)> CXWndIsType = 0x571300;
static mem::function<int __fastcall(int, int, int, int)> CXWndShowContextMenu = 0x5A02F0;
static mem::function<char __fastcall(Zeal::GameUI::SidlWnd *wnd, int unusedEDX, int left, int top, int right,
                                     int bottom)>
    CXWndMoveAndInvalidate = 0x00573600;
static mem::function<Zeal::GameUI::CXRect *__fastcall(Zeal::GameUI::SidlWnd *wnd, int unusedEDX,
                                                      Zeal::GameUI::CXRect *rect)>
    CXWndGetMinimizedRect = 0x00573730;
static mem::function<int __fastcall(Zeal::GameUI::SidlWnd *wnd, int unusedEDX, int32_t mouse_x, int32_t mouse_y,
                                    uint32_t unknown3)>
    CSidlScreenWndHandleRButtonDown = 0x005703f0;
static mem::function<void __fastcall(Zeal::GameUI::SidlWnd *wnd, int unusedEDX)> CXWndDeactivate =
    0x0056e0b0;  // Does nothing.
static mem::function<char __fastcall(Zeal::GameUI::SidlWnd *wnd, int unusedEDX, Zeal::GameUI::CXRect rect)>
    CXWndOnMove = 0x00402301;
static mem::function<int __fastcall(Zeal::GameUI::SidlWnd *wnd, int unusedEDX, int width, int height)> CXWndOnResize =
    0x0056eb00;
static mem::function<void __fastcall(Zeal::GameUI::SidlWnd *wnd, int unusedEDX)> CXWndOnMinimizeBox = 0x00402306;
static mem::function<int __fastcall(Zeal::GameUI::SidlWnd *wnd, int unusedEDX)> CXWndOnActivate =
    0x00402317;  // Just returns zero. Shared call.
static mem::function<int __fastcall(Zeal::GameUI::SidlWnd *wnd, int unusedEDX)> CXWndAboutToDeleteWnd =
    0x00402317;  // Just returns zero. Shared call.
static mem::function<Zeal::GameUI::CXRect *__fastcall(Zeal::GameUI::SidlWnd *wnd, int unusedEDX,
                                                      Zeal::GameUI::CXRect *rect, int region)>
    CXWndGetHitTestRect = 0x00571540;
static mem::function<Zeal::GameUI::CXRect *__fastcall(Zeal::GameUI::SidlWnd *wnd, int unusedEDX,
                                                      Zeal::GameUI::CXRect *rect)>
    CXWndGetInnerRect = 0x005714b0;
static mem::function<Zeal::GameUI::CXRect *__fastcall(Zeal::GameUI::SidlWnd *wnd, int unusedEDX,
                                                      Zeal::GameUI::CXRect *rect)>
    CXWndGetClientRect = 0x00572390;
static mem::function<int __fastcall(Zeal::GameUI::SidlWnd *wnd, int unusedEDX)> CSidlScreenWndOnShow = 0x0056f590;
static mem::function<void __fastcall(Zeal::GameUI::SidlWnd *wnd, int unusedEDX)> CSidlScreenWndLoadIniInfo = 0x0056f5a0;
static mem::function<void __fastcall(Zeal::GameUI::SidlWnd *wnd, int unusedEDX)> CSidlScreenWndStoreIniInfo =
    0x0056fe10;
static mem::function<int *__fastcall(Zeal::GameUI::SidlWnd *, int unusedEDX, Zeal::GameUI::BasicWnd *,
                                     Zeal::GameUI::CXSTR name)>
    CSidlScreenWndConstructor = 0x0056e2b0;
static mem::function<int *__fastcall(Zeal::GameUI::SidlWnd *, int unusedEDX, bool delete_me)> CSidlScreenWndDestructor =
    0x0056e0c0;
static mem::function<Zeal::GameUI::ItemDisplayWnd *__fastcall(const Zeal::GameUI::ItemDisplayWnd *, int unusedEDX,
                                                              int unk)>
    CItemDisplayWndConstructor = 0x00423331;
static mem::function<int *__fastcall(Zeal::GameUI::ItemDisplayWnd *, int unusedEDX, bool delete_me)>
    CItemDisplayWndDestructor = 0x004234e8;
static mem::function<int __fastcall(int, int)> CLootWndDeactivate = 0x42651f;
static mem::function<int __cdecl()> MessageEvent = 0x52437F;
static mem::function<int __fastcall(int, int)> ProcessControls = 0x53F337;
static mem::function<int __cdecl(Zeal::GameStructures::Entity *, const char *)> ReplyTarget = 0x4ff62d;
static mem::function<Zeal::GameStructures::Entity *__cdecl(const char *name)> GetPlayerFromName =
    0x005081db;  // Entity list traversal with name stricmp.
}  // namespace GameInternal

namespace Spells  // some wrappers for simplifying
{
void OpenBook();
void StopSpellBookAction();
void Memorize(int book_index, int gem_index);
void Forget(int index);
void UpdateGems(int index);
bool IsValidSpellIndex(int spellid);
}  // namespace Spells

namespace OldUI {
bool spellbook_window_open();
}

int GetSpellCastingTime();  // Used by CCastingWnd. Returns -1 if done otherwise in units of 0.1% of time left.
DWORD GetLevelCon(Zeal::GameStructures::Entity *ent);
bool IsPlayableRace(WORD race);
const char *get_aa_title_name(BYTE class_id, int aa_rank, BYTE gender_id);
float CalcCombatRange(Zeal::GameStructures::Entity *entity1, Zeal::GameStructures::Entity *entity2);
float CalcZOffset(Zeal::GameStructures::Entity *ent);
float CalcBoundingRadius(Zeal::GameStructures::Entity *ent);
void DoPercentConvert(std::string &str);
Zeal::GameStructures::Entity *get_player_partial_name(const char *name);
void move_item(int a1, int slot, int a2, int a3);
bool can_inventory_item(Zeal::GameStructures::GAMEITEMINFO *item);
// Checks if the race/class/deity etc can equip this item
bool can_use_item(Zeal::GameStructures::GAMECHARINFO *c, Zeal::GameStructures::GAMEITEMINFO *item);
// Checks if the item can currently be equipped in this inventory slot.
// This also prevents equipping an offhand while holding a 2Hander. Or equipping a mainhand with an instrument, etc.
// Use [1..21] for slot number.
bool can_item_equip_in_slot(Zeal::GameStructures::GAMECHARINFO *c, const Zeal::GameStructures::GAMEITEMINFO *item,
                            int slot);
bool can_equip_item(Zeal::GameStructures::GAMEITEMINFO *item);
void print_debug(const char *format, ...);
UINT get_game_time();
int get_game_main();
void do_autoattack(bool enabled);
bool CanIHitTarget(float dist);
bool do_attack(uint8_t type, uint8_t p2);
void do_raidaccept();
void do_raiddecline();
void do_inspect(Zeal::GameStructures::Entity *player);
void do_join(Zeal::GameStructures::Entity *player, const char *name);
void send_to_channel(int chat_channel_zero_based, const char *message);
void execute_cmd(UINT cmd, int isdown, int unk2);
GameStructures::GameClass *get_game();
GameStructures::Display *get_display();
const char *get_ui_skin();
std::string get_ui_ini_filename();
std::string get_host_tag();  // Returns the short host description tag at end of ui filenames.
std::filesystem::path get_game_path();
std::filesystem::path get_default_ui_skin_path();
int get_gamestate();
int get_channel_number(const char *name);  // Zero-based channel number.
void SetMusicSelection(int number, bool enabled);
void WavePlay(int index);
bool is_new_ui();
HWND get_game_window();
bool show_context_menu();
bool game_wants_input();  // returns true if the game wants text input so it doesn't run binds
void CXStr_PrintString(Zeal::GameUI::CXSTR *str, const char *format, ...);
Vec3 get_player_head_pos();
Vec3 get_view_actor_head_pos();
void pet_command(int cmd, short spawn_id);
float encum_factor();
const Zeal::GameStructures::GameCommand *get_command_struct(const std::string &command);
int get_command_function(const std::string &command);  // Returns function pointer of command.
std::vector<std::string> get_command_matches(const std::string &start_of_command);
Zeal::GameStructures::Entity *get_view_actor_entity();
inline Zeal::GameStructures::GuildName *guild_names = (Zeal::GameStructures::GuildName *)0x7F9C94;
bool collide_with_world(Vec3 start, Vec3 end, Vec3 &result, char collision_type = 0x3, bool debug = false);
bool is_view_actor_invisible(Zeal::GameStructures::Entity *entity);
std::vector<Zeal::GameStructures::Entity *> get_world_visible_actor_list(float max_dist, bool only_targetable = true);
Zeal::GameStructures::ActorLocation get_actor_location(int actor);
float get_target_blink_fade_factor(float speed_factor, bool auto_attack_only);  // Returns 0 to 1.0f.
bool is_view_actor_me();
void print_chat(const std::string& data);
void print_chat(const char *format, ...);
void print_chat(short color, const char *format, ...);
void print_chat_wnd(Zeal::GameUI::ChatWnd *, short color, const char *format, ...);
long get_user_color(int index);
void set_target(Zeal::GameStructures::Entity *target);  // this will target an entity without question
void do_target(const char *name);  // this function is used by /target and /rt with range limitations
void do_consider(Zeal::GameStructures::Entity *entity, const char *str);
bool get_attack_on_assist();
void set_attack_on_assist(bool enable);
bool can_move();
bool is_on_ground(Zeal::GameStructures::Entity *ent);
void log(const char *data);
void log(std::string &data);
bool is_autoattacking();
Zeal::GameStructures::GAMECHARINFO *get_char_info();
Zeal::GameStructures::Entity *get_active_corpse();
Zeal::GameStructures::Entity *get_target();
Zeal::GameStructures::Entity *get_entity_list();
Zeal::GameStructures::Entity *get_self();
Zeal::GameStructures::Entity *get_pet();
Zeal::GameStructures::SPELLMGR *get_spell_mgr();
int get_spell_level(int spell_id);
const char *get_spell_name(int spell_id);
void dump_spell_info(int spell_id);
Zeal::GameStructures::Entity *get_controlled();
Zeal::GameStructures::ViewActor *get_view_actor();
Zeal::GameStructures::CameraInfo *get_camera();
Zeal::GameStructures::Entity *get_entity_by_id(short id);  // Returns nullptr if invalid id.
Zeal::GameStructures::Entity *get_entity_by_parent_id(short parent_id);
void send_message(UINT opcode, int *buffer, UINT size, int unknown);
// trim_name() and strip_name() both generate temporary modified string copies in global buffers that will get
// re-used, so the result should be consumed immediately (copy to std::string or compare immediately).
const char *trim_name(const char *name);   // Cleans but leaves suffixes. Use for name's corpse123 => name's corpse.
const char *strip_name(const char *name);  // Strips numbers and any text past an apostrophe ('s corpse).
char *get_string(UINT id);
// void set_camera_position(Vec3* pos);
float heading_to_yaw(float heading);
bool is_mouse_hovering_window();
int get_showname();        // Holds value of /showname command.
int get_show_pc_names();   // Holds value of Options -> Display -> Show PC Names.
int get_show_npc_names();  // Holds value of Options -> Display -> Show NPC Names.
std::string class_name_short(int class_id);
std::string class_name(int class_id);
static constexpr int kInvalidZoneId = 0;                // get_index_from_zone_name() returns 0 if no matches.
static constexpr int kNumZoneIds = 1000;                // 0 = invalid, 999 = last reliable entry in GameWorldData
std::string get_full_zone_name(int zone_id);            // GameWorldData::GetFullZoneName()
std::string get_zone_name_from_index(int zone_id);      // GameWorldData::GetZoneNameFromIndex()
int get_index_from_zone_name(const std::string &name);  // GameWorldData::GetIndexFromZoneName()
std::string get_class_desc(int class_id);               // Game ::GetClassDesc()
std::string get_title_desc(int class_id, int aa_rank, int gender);  // Game ::GetTitleDesc()
std::string get_player_guild_name(short guild_id);                  // GetPlayerGuildName()
bool is_gui_visible();                                              // Returns true if DrawWindows() will be called.
bool is_game_ui_window_hovered();
bool is_targetable(Zeal::GameStructures::Entity *ent);
bool is_in_game();
bool is_in_char_select();
void do_consent(const char *name);
void do_say(bool hide_local, const char *format, ...);
void do_say(bool hide_local, std::string data);
void do_tell(const char *target_name_and_message);
void do_gsay(std::string data);
void do_guildsay(std::string data);
void do_auction(std::string data);
void do_ooc(std::string data);
void send_raid_chat(std::string data);
void destroy_held();
int get_region_from_pos(Vec3 *pos);
GameUI::CXWndManager *get_wnd_manager();
bool is_player_pet(const Zeal::GameStructures::Entity &entity);
std::vector<Zeal::GameStructures::RaidMember *> get_raid_list();
DWORD get_raid_group_number();
int get_raid_group_count(DWORD group_number);
DWORD get_raid_class_color(BYTE class_id);  // Returns ARGB color for the class.
bool is_raid_pet(const Zeal::GameStructures::Entity &entity);
std::string get_raid_group_leader(DWORD group_number);
bool update_group_window_colors(bool use_raid_colors, bool store_defaults = false);
void print_group_leader();
void print_raid_leaders(bool show_all_groups = false, bool show_open_groups = false);
void print_raid_ungrouped();
void dump_raid_state();
std::string generateTimestamp();
int get_effect_required_level(Zeal::GameStructures::GAMEITEMINFO *item);
bool use_item(int item_index, bool quiet = false);
enum class SortType { Ascending, Descending, Toggle };
void sort_list_wnd(Zeal::GameUI::ListWnd *list_wnd, int sort_column, SortType sort_type = SortType::Ascending);
short total_spell_affects(Zeal::GameStructures::GAMECHARINFO *char_info, BYTE affect_type, BYTE a3,
                          int *per_buff_values);
void sit();
void dismount();
bool is_mounted();
bool is_a_mount(const Zeal::GameStructures::Entity *entity);

// game.dll patch support that expanded the number of available bank slots.
int get_num_personal_bank_slots();
int get_num_shared_bank_slots();
int get_num_total_bank_slots();

int get_num_empty_inventory_slots();  // Not game.dll related (label support).
int get_num_inventory_slots();        // Not game.dll related (label support).

// mystats details.
enum Era { Classic = 0, Kunark, Velious, Luclin, PlanesOfPower };

Era get_era();
int get_avoidance(bool include_combat_agility = false);
int get_mitigation(bool include_cap = false);
int get_mitigation_softcap();
int get_display_AC();
Zeal::GameEnums::SkillType get_weapon_skill(const Zeal::GameStructures::GAMEITEMINFO *weapon);
int get_hand_to_hand_delay();
void print_melee_attack_stats(bool primary, const Zeal::GameStructures::GAMEITEMINFO *weapon = nullptr,
                              short color = USERCOLOR_DEFAULT);
}  // namespace Game
}  // namespace Zeal
