#include "zeal_settings.h"

#include "callbacks.h"
#include "game_addresses.h"
#include "game_functions.h"
#include "game_ui.h"
#include "io_ini.h"
#include "zeal.h"

template <typename T>
ZealSetting<T>::ZealSetting(T default_value_in, const std::string &ini_section, const std::string &ini_key,
                            bool save_per_character, const std::function<void(const T &value)> &callback_on_set) {
  set_callback = callback_on_set;
  default_value = default_value_in;
  value = default_value;
  section = ini_section;
  key = ini_key;
  per_character = save_per_character;
  init();
  // Add a callback that executes when exiting CharacterSelect to trigger a refresh init of settings.
  ZealService::get_instance()->callbacks->AddGeneric([this]() { init(); }, callback_type::CleanCharSelectUI);
}

// The memory only setting just sets the section and key names blank to avoid use of ini io. It
// will still get reset to the default by the init callback.
template <typename T>
ZealSetting<T>::ZealSetting(T default_value_in) : ZealSetting(default_value_in, "", "", false, nullptr) {}

template <typename T>
void ZealSetting<T>::init() {
  // Resets to the default value and then tries to retrieve the persistent value. The per_character ones
  // will stay at the default initially until a character is known.
  value = default_value;
  if (section.length() && key.length()) {
    IO_ini ini(IO_ini::kZealIniFilename);
    std::string section_name = get_section_name();
    if (section_name.length() && ini.exists(section_name, key)) value = ini.getValue<T>(section_name, key);
  }
  if (set_callback) set_callback(value);
}

template <typename T>
void ZealSetting<T>::set(T val, bool store) {
  if (store && section.length() && key.length()) {
    std::string section_name = get_section_name();
    if (section_name.length()) {
      IO_ini ini(IO_ini::kZealIniFilename);
      ini.setValue<T>(section_name, key, val);
    }
  }
  if (value != val) {
    value = val;
    if (set_callback) set_callback(value);
  }
}

static const char *get_character_name() {
  // In order to properly reset everything to the defaults and reload the per character settings,
  // this method may be accessed in GAMESTATE_ENTERWORLD where charinfo has not yet been updated.
  // We peek at the character select results to retrieve the name, which is similar to what
  // StartNetworkGame() does to set g_next_player at 0x00795274.
  const char *name = nullptr;
  if (Zeal::Game::get_gamestate() == GAMESTATE_INGAME) {
    Zeal::GameStructures::GAMECHARINFO *c = Zeal::Game::get_char_info();
    name = (c) ? c->Name : nullptr;
  } else if (Zeal::Game::get_gamestate() == GAMESTATE_ENTERWORLD) {
    int index = -1;
    if (Zeal::Game::is_new_ui()) {
      if (Zeal::Game::Windows && Zeal::Game::Windows->CharacterSelect) {
        index = Zeal::Game::Windows->CharacterSelect->SelectIndex;
      }
    } else {
      int old_char_select = *reinterpret_cast<int *>(0x007f959c);
      if (old_char_select) index = *reinterpret_cast<short *>(old_char_select + 0x80);
    }
    if (index >= 0 && index < 8) {
      name = index * 0x40 + 0x38e54 + reinterpret_cast<const char *>(Zeal::Game::get_game());
    }
  }
  return name;
}

template <typename T>
std::string ZealSetting<T>::get_section_name() const {
  if (!per_character) return section;

  const char *name = get_character_name();
  if (!name) return "";
  return section + "_" + std::string(name);
  return std::string(name);
}

// Perform explicit instatiation of the supported template types.
template class ZealSetting<bool>;
template class ZealSetting<int>;
template class ZealSetting<float>;
template class ZealSetting<std::string>;