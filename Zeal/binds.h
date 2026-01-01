#pragma once
#include <functional>
#include <unordered_map>

#include "operator_overloads.h"

enum class key_category : int  // this is bitwise so you can do multiple categorys by doing Movement | Target for
                               // example
{
  Movement = 1,
  Commands = 2,
  Spell = 4,
  Target = 8,
  Camera = 16,
  Chat = 32,
  UI = 64,
  Macros = 128
};

ENUM_BITMASK_OPERATORS(key_category)

class Binds {
 public:
  static constexpr int kNumBinds = 256;  // Limited by OptionsWnd::KeyMaps size.
  static constexpr int kNameBufferSize = 64;

  Binds(class ZealService *zeal);
  ~Binds();

  // Adds a new mappable bind in the command index slot. Blocks replacing default client indices.
  void add_bind(int index, const char *name, const char *short_name, key_category category,
                std::function<void(int state)> callback);

  // Adds a callback that happens prior to the primary bind (including client binds) and
  // can return true to block execution of the primary bind.
  void replace_cmd(int cmd, std::function<bool(int state)> callback);

  // Toggles whether to use the default global ini section or per character section name.
  void set_per_character_mode(bool enable);

  void print_keybinds() const;

  void handle_init_keyboard_assignments(void *options_window);  // Internal callback.

 private:
  bool execute_cmd(unsigned int opcode, int state);
  void update_ini_section_name();
  void read_ini();

  char *KeyMapNames[kNumBinds] = {0};
  char KeyMapNamesBuffer[kNumBinds][kNameBufferSize] = {0};
  int KeyMapCategories[kNumBinds] = {0};
  std::unordered_map<int, std::function<void(int state)>> KeyMapFunctions;
  std::unordered_map<int, std::vector<std::function<bool(int state)>>> ReplacementFunctions;
  bool per_character_mode = false;
  char ini_section_name[8 + 30 + 2];  // Space for "Keymaps_" + self->name + null.
};
