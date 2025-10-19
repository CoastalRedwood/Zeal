#include "binds.h"

#include "callbacks.h"
#include "game_addresses.h"
#include "game_functions.h"
#include "game_structures.h"
#include "hook_wrapper.h"
#include "zeal.h"

Binds::~Binds() {}

bool Binds::execute_cmd(unsigned int cmd, int isdown) {
  ZealService *zeal = ZealService::get_instance();
  // Don't call our binds on keydown when the game wants input except for reply cycling and auto-run.
  bool reply_cycle = (cmd == 0x3c || cmd == 0x3d);
  bool auto_run = (cmd == 1);  // ProcessKeyDown() already filters normal keys during chat. Fixes numlock.
  if (!Zeal::Game::game_wants_input() || !isdown || reply_cycle || auto_run) {
    if (ReplacementFunctions.count(cmd) > 0) {
      for (auto &fn : ReplacementFunctions[cmd])
        if (fn(isdown))  // if the replacement function returns true, end here otherwise its really just adding more to
                         // the command
          return true;
    }

    if (KeyMapFunctions.count(cmd) > 0)
      KeyMapFunctions[cmd](isdown);
    else
      return false;
  }

  return false;
}

void __fastcall InitKeyboardAssignments(void *options_window, int unused) {
  ZealService *zeal = ZealService::get_instance();
  zeal->binds_hook->initialize_options_with_keybinds(options_window);
  zeal->binds_hook->read_ini();
  zeal->hooks->hook_map["InitKeyboardAssignments"]->original(InitKeyboardAssignments)(options_window, unused);
}

UINT32 read_internal_from_ini(int index, int key_type) {
  int fn = 0x525520;
  __asm
  {
		push key_type
		push index
		call fn
		pop ecx
		pop ecx
  }
}

void Binds::read_ini() {
  int size = sizeof(KeyMapNames) / sizeof(KeyMapNames[0]);
  for (int i = 128; i < size; i++)  // the game will load its own properly
  {
    if (KeyMapNames[i])  // check if its not nullptr
    {
      int keycode = read_internal_from_ini(i, 0);
      int keycode_alt = read_internal_from_ini(i, 1);
      if (keycode != -0x2) {
        Zeal::Game::ptr_PrimaryKeyMap[i] = keycode;
      }
      if (keycode_alt != -0x2) {
        Zeal::Game::ptr_AlternateKeyMap[i] = keycode_alt;
      }
    }
  }
}

// Loads the internal cache with the custom keybind for later initialization.
void Binds::add_bind(int cmd, const char *name, const char *short_name, key_category category,
                     std::function<void(int state)> callback) {
  if (cmd < 0 || cmd >= kNumBinds) return;
  strcpy_s(KeyMapNamesBuffer[cmd], kNameBufferSize, short_name);
  KeyMapNames[cmd] = KeyMapNamesBuffer[cmd];
  KeyMapCategories[cmd] = static_cast<int>(category);
  KeyMapFunctions[cmd] = callback;
}

void Binds::initialize_options_with_keybinds(void *options_window) {
  int options = reinterpret_cast<int>(options_window);  // TODO: Add an OptionsWindow class.
  for (int cmd = 0; cmd < kNumBinds; ++cmd) {
    if (KeyMapNames[cmd] == nullptr || !KeyMapFunctions.count(cmd)) continue;  // Empty, skip.
    Zeal::Game::GameInternal::InitKeyBindStr((options + cmd * 0x8 + 0x20c), 0, KeyMapNames[cmd]);
    *(int *)((options + cmd * 0x8 + 0x210)) = KeyMapCategories[cmd];
  }
}

void Binds::replace_cmd(int cmd, std::function<bool(int state)> callback) {
  ReplacementFunctions[cmd].push_back(callback);
}

Binds::Binds(ZealService *zeal) {
  zeal->callbacks->AddCommand([this](UINT opcode, int state) { return execute_cmd(opcode, state); },
                              callback_type::ExecuteCmd);
  for (int i = 0; i < 128; i++)
    KeyMapNames[i] = *(char **)(0x611220 + (i * 4));  // copy the original short names to the new array
  mem::write(0x52507A, (int)KeyMapNames);             // write ini keymap
  mem::write(0x5254D9, (int)KeyMapNames);             // clear ini keymap
  mem::write(0x525544, (int)KeyMapNames);             // read ini keymap
  mem::write(0x42C52F, (BYTE)0xEB);  // remove the check for max index of 116 being stored in client ini
  mem::write(0x52485A, (int)256);    // increase this for loop to look through all 256
  mem::write(0x52591C,
             (int)(Zeal::Game::ptr_AlternateKeyMap + (256 * 4)));  // fix another for loop to loop through all 256
  zeal->hooks->Add("InitKeyboardAssignments", Zeal::Game::GameInternal::fn_initkeyboardassignments,
                   InitKeyboardAssignments, hook_type_detour);
}
