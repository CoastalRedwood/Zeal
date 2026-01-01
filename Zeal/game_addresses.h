#pragma once
#include "game_structures.h"
#include "game_ui.h"

namespace Zeal {
namespace Game {

template <typename T>
struct pPtr {
  T *ptr;
};

static HANDLE *Heap = Zeal::GameUI::Heap;
static GameStructures::Entity *Self = (Zeal::GameStructures::Entity *)0x7F94CC;
static GameStructures::Entity *Target = (Zeal::GameStructures::Entity *)0x7F94EC;
static GameStructures::Entity *Active_Corpse = (Zeal::GameStructures::Entity *)0x7f9500;
static GameStructures::Entity *EntListPtr = (Zeal::GameStructures::Entity *)0x7f9498;
static GameStructures::CameraInfo *CameraInfo = (Zeal::GameStructures::CameraInfo *)0x63B928;
static GameStructures::GroupInfo *GroupInfo = (Zeal::GameStructures::GroupInfo *)0x007912B0;
static GameStructures::RaidInfo *RaidInfo = (GameStructures::RaidInfo *)0x7914D0;
static GameStructures::ViewActor *ViewActor = (GameStructures::ViewActor *)0x63D6C0;
static GameStructures::KeyboardModifiers *KeyMods = (GameStructures::KeyboardModifiers *)0x799738;
static GameUI::pInstWindows *Windows = (GameUI::pInstWindows *)0x63D5CC;
static GameUI::CXWndManager **WndManager = (GameUI::CXWndManager **)0x809DB4;

// The client maintains a 5000 entry spawn_id to entity (GamePlayer) LUT for faster access to the linked list.
static constexpr int kEntityIdArraySize = 5000;
static GameStructures::Entity **EntityIdArray = (Zeal::GameStructures::Entity **)0x0078c47c;  // IDArray
static GameStructures::GAMEZONEINFO *ZoneInfo = (GameStructures::GAMEZONEINFO *)0x00798784;
// static GameStructures::SPELLMGR* SpellsMgr = (GameStructures::SPELLMGR*)0x805CB0;

// static DWORD* ptr_LocalPC = (DWORD*)0x7F94E8;
static GameUI::ContainerMgr **ptr_ContainerMgr = reinterpret_cast<GameUI::ContainerMgr **>(0x0063d6b8);
static int *ptr_PrimaryKeyMap = (int *)0x7CD84C;
static int *ptr_AlternateKeyMap = (int *)0x7CDC4C;
static BYTE *strafe_direction = (BYTE *)0x7985EB;
static float *strafe_speed = (float *)0x799780;
static GameStructures::Entity *_ControlledPlayer = (GameStructures::Entity *)0x7f94e0;
static GameStructures::Cam *camera = (GameStructures::Cam *)0x799688;       // 0x7996C0;
static int16_t *const mouse_client_x = (int16_t *)0x00798580;               // Mouse coordinates in client rect.
static int16_t *const mouse_client_y = (int16_t *)0x00798582;               // Set to 32767 when invalid.
static int32_t *const mouse_client_x_dinput_state = (int32_t *)0x008090a8;  // Internal dinput state update.
static int32_t *const mouse_client_y_dinput_state = (int32_t *)0x008090ac;
static int32_t *const mouse_client_x_dinput_accum = (int32_t *)0x008092e8;  // Internal dinput accumulators.
static int32_t *const mouse_client_y_dinput_accum = (int32_t *)0x008092ec;
static bool *const is_right_mouse_look_down = (bool *)0x007985ea;  // Set true when in rmb mouse look mode.
static bool *const is_left_mouse_down = (bool *)0x00798614;        // Copied dinput lmb value in ProcessMouseEvent.
static bool *const is_right_mouse_down = (bool *)0x00798615;       // Copied dinput lmb value in ProcessMouseEvent.
static bool *const is_right_mouse_down_from_dinput = (bool *)0x00798616;
static bool *const is_left_mouse_down_from_dinput = (bool *)0x00798617;
static int *mouse_hover_window = (int *)0x809DD8;  // unsure
static int *camera_view = (int *)0x63BE68;
static int max_pitch = 0x5e86d0;
static GameStructures::KeyboardInput *KeyInput = (GameStructures::KeyboardInput *)0x7ce058;
static char *in_game = (char *)0x798550;
static int *attack_on_assist = (int *)0x007cf204;    // 1 = attack on assist
static int *is_logging_enabled = (int *)0x007cf1e0;  // 0 = disabled, 1 = enabled

// Vec3* camera_position = *(Vec3**)0x9c08128;
}  // namespace Game
}  // namespace Zeal
   // int pWorld = 0x7F9494;