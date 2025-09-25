#pragma once
#include <Windows.h>

#include <functional>
#include <string>

template <typename T>
class ZealSetting {
 public:
  ZealSetting() = delete;  // Prevent default construction.

  // Supports storing the setting in the kZealIniFilename file with options for per character
  // stored settings and a callback method when set. Note the callback method is executed when
  // exiting character select, so the callback must ensure it is compatible with GAMESTATE_ENTERWORLD.
  // The default_value is also copied back (if no persistent setting) when exiting character select.
  ZealSetting(T default_value, const std::string &ini_section, const std::string &ini_key,
              bool save_per_character = false, const std::function<void(const T &value)> &callback_on_set = nullptr);

  // Support a simple set/get/toggle memory-only mode (no persistent ini file storage or set call back).
  ZealSetting(T _default_value);

  // Sets the value in memory and optionally updates the ini file value.
  void set(T val, bool store = true);

  // Toggles the current value and optionally stores the new value to the ini file.
  void toggle(bool store = true)
    requires std::is_same_v<T, bool>
  {
    set(!value, store);
  }

  // Returns the current value.
  T get() const { return value; }

 private:
  // Initializes the setting by attempting to fetch the persistent value and then executing the set callback.
  void init();

  // Returns the ini section (category) name, optionally with a character specific suffix.
  std::string get_section_name() const;

  std::function<void(const T &value)> set_callback;
  T default_value;             // Default value (when persisted value isn't set).
  T value;                     // Live value.
  std::string section;         // Ini file section name.
  std::string key;             // Ini file key name within section.
  bool per_character = false;  // Whether persisted value is stored per character (vs global).
};