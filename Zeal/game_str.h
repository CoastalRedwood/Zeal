#pragma once
#include "hook_wrapper.h"
#include "memory.h"

class GameStr {
 public:
  std::unordered_map<int, const char *> str_replacements;
  std::unordered_map<int, bool> str_noprint;
  GameStr(class ZealService *zeal);
  ~GameStr();

 private:
};
