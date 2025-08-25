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
  Binds(class ZealService *zeal);
  ~Binds();

  static constexpr int kNumBinds = 256;
  static constexpr int kNameBufferSize = 64;
  char *KeyMapNames[kNumBinds] = {0};
  char KeyMapNamesBuffer[kNumBinds][kNameBufferSize] = {0};
  int KeyMapCategories[kNumBinds] = {0};
  std::unordered_map<int, std::function<void(int state)>> KeyMapFunctions;
  std::unordered_map<int, std::vector<std::function<bool(int state)>>> ReplacementFunctions;

  void read_ini();
  void initialize_options_with_keybinds(void *options_window);
  void add_bind(int index, const char *name, const char *short_name, key_category category,
                std::function<void(int state)> callback);
  void replace_cmd(int cmd, std::function<bool(int state)> callback);
  bool execute_cmd(unsigned int opcode, bool state);
};