#include "binds.h"

#include "callbacks.h"
#include "game_addresses.h"
#include "game_functions.h"
#include "game_structures.h"
#include "game_ui.h"
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

// Called in OptionsWnd::OptionsWnd() which happens upon creation of the new UI. This will handle updating
// the keybinds from the ini if they are per character.
void __fastcall InitKeyboardAssignments(void *options_window, int unused) {
  ZealService *zeal = ZealService::get_instance();
  zeal->binds_hook->handle_init_keyboard_assignments(options_window);
  zeal->hooks->hook_map["InitKeyboardAssignments"]->original(InitKeyboardAssignments)(options_window, unused);
}

// Sets the name used in the ini file to allow per character support.
void Binds::update_ini_section_name() {
  strcpy_s(ini_section_name, sizeof(ini_section_name), "KeyMaps");  // Start with default section name.
  auto self = Zeal::Game::get_self();
  if (self && per_character_mode) {
    std::string name = std::string("KeyMaps_") + self->Name;
    if (name.length() < sizeof(ini_section_name)) strcpy_s(ini_section_name, sizeof(ini_section_name), name.c_str());
  }
}

// The ini keybinds are normally initialized from the ini in loadOptions() as part of the game constructor.
// This happens before Zeal is loaded in the first boot, so we need to perform the loads for the Zeal
// custom binds here. In order to support per character keybinds, this was expanded to completely redo all
// keys so it repeats the default initialization and reloads all ini key values.
void Binds::read_ini() {
  // First call default_key_bindings() to reset state. For some reason that code also resets the
  // mouse y invert global and sensitivity, so we cache and restore them.
  BYTE *const g_mouse_y_invert = reinterpret_cast<BYTE *>(0x007985e8);
  DWORD *const g_mouse_sensitivity = reinterpret_cast<DWORD *>(0x0079858a);
  BYTE invert = *g_mouse_y_invert;
  DWORD sensitivity = *g_mouse_sensitivity;
  const int kDefaultKeyBindingsAddr = 0x0055a83b;
  reinterpret_cast<void (*)()>(kDefaultKeyBindingsAddr)();  // Restore to defaults.
  *g_mouse_y_invert = invert;
  *g_mouse_sensitivity = sensitivity;

  // Update all bound keycodes after possibly updating to point to per character ini section.
  update_ini_section_name();
  for (int i = 1; i < kNumBinds; i++) {
    if (KeyMapNames[i])  // check if its not nullptr
    {
      int keycode = Zeal::Game::GameInternal::readKeyMapFromIni(i, 0);
      int keycode_alt = Zeal::Game::GameInternal::readKeyMapFromIni(i, 1);
      if (keycode != -0x2) {
        Zeal::Game::ptr_PrimaryKeyMap[i] = keycode;
      }
      if (keycode_alt != -0x2) {
        Zeal::Game::ptr_AlternateKeyMap[i] = keycode_alt;
      }
    }
  }
}

void Binds::set_per_character_mode(bool enable) {
  if (per_character_mode == enable) return;  // Nothing to do.

  per_character_mode = enable;

  if (Zeal::Game::get_gamestate() != GAMESTATE_INGAME) return;

  read_ini();  // Synchronizes the current keybinds based on character (if needed).

  // Update UI elements (options wnd lists, other special wnd cases).
  auto options = Zeal::Game::Windows->Options;
  if (options) options->UpdateKeyboardAssignmentList();
  auto display = Zeal::Game::get_display();
  if (display) display->KeyMapUpdated();
}

// Returns true if the bind is an existing game_bind.
static bool is_existing_game_bind(int cmd) {
  if (cmd <= 0x82) return true;                 // Binds exist all the way up to 0x82 with a few exceptions.
  if (cmd >= 0xc6 && cmd <= 0xd2) return true;  // Hidden binds in this region.
  return false;
}

// Loads the internal cache with the custom keybind for later initialization.
void Binds::add_bind(int cmd, const char *name, const char *short_name, key_category category,
                     std::function<void(int state)> callback) {
  if (cmd < 0 || cmd >= kNumBinds) return;

  // Add a second layer of protection against replacing existing binds.
  if (is_existing_game_bind(cmd)) return;

  strcpy_s(KeyMapNamesBuffer[cmd], kNameBufferSize, short_name);
  KeyMapNames[cmd] = KeyMapNamesBuffer[cmd];
  KeyMapCategories[cmd] = static_cast<int>(category);
  KeyMapFunctions[cmd] = callback;
}

// Handle the custom initialization of the keybinds.
void Binds::handle_init_keyboard_assignments(void *options_window) {
  // First update the OptionsWnd keymaps with the new custom values so they show up as configurable.
  // Note that the default available binds (1 to 0x74) are taken care of by the default
  // InitKeyboardAssignments() since the corresponding KeyMapFunctions won't exist since we
  // block adding binds in that region.
  auto options = reinterpret_cast<Zeal::GameUI::OptionsWnd *>(options_window);
  for (int cmd = 0; cmd < kNumBinds; ++cmd) {
    if (KeyMapNames[cmd] == nullptr || !KeyMapFunctions.count(cmd)) continue;  // Empty or default, skip.
    options->KeyMaps[cmd].name.Set(KeyMapNames[cmd]);
    options->KeyMaps[cmd].category = KeyMapCategories[cmd];
  }

  // Take advantage of this call to handle ini synchronization to support custom keybinds and
  // also the per character option.
  read_ini();
}

void Binds::replace_cmd(int cmd, std::function<bool(int state)> callback) {
  ReplacementFunctions[cmd].push_back(callback);
}

void Binds::print_keybinds() const {
  for (int i = 0; i < kNumBinds; ++i) {
    const char *name = KeyMapNames[i];
    name = name ? name : (is_existing_game_bind(i) ? "HARDCODED" : "");
    Zeal::Game::print_chat("[%d]: %s", i, name);
  }
}

Binds::Binds(ZealService *zeal) {
  zeal->callbacks->AddCommand([this](UINT opcode, int state) { return execute_cmd(opcode, state); },
                              callback_type::ExecuteCmd);
  update_ini_section_name();               // Initializes to default.
  const int kNumOriginalShortNames = 128;  // Length of initialized pointer array in the client.
  char **const kOriginalShortNames = reinterpret_cast<char **const>(0x611220);
  for (int i = 0; i < kNumOriginalShortNames; i++)
    KeyMapNames[i] = kOriginalShortNames[i];     // copy the original short names to the new array
  mem::write(0x52507A, (int)KeyMapNames);        // write ini keymap
  mem::write(0x525477, (int)&ini_section_name);  // write ini section name
  mem::write(0x5254D9, (int)KeyMapNames);        // clear ini keymap
  mem::write(0x525514, (int)&ini_section_name);  // clear ini section name
  mem::write(0x525544, (int)KeyMapNames);        // read ini keymap
  mem::write(0x525596, (int)&ini_section_name);  // read ini section name
  mem::write(0x42C52F, (BYTE)0xEB);              // remove the check for max index of 116 being stored in client ini
  mem::write(0x52485A, (int)kNumBinds);          // increase this for loop to look through all 256
  mem::write(0x52591C, (int)(&Zeal::Game::ptr_AlternateKeyMap[kNumBinds]));  // also loop through 256
  zeal->hooks->Add("InitKeyboardAssignments", Zeal::Game::GameInternal::fn_initkeyboardassignments,
                   InitKeyboardAssignments, hook_type_detour);
}
