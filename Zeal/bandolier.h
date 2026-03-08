#pragma once

#include <Windows.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "game_ui.h"
#include "io_ini.h"
#include "zeal_settings.h"

class Bandolier {
 public:
  Bandolier(class ZealService *zeal);
  ~Bandolier();

 private:

  // File system storage of spell sets.
  void initialize_ini_filename();
  void save(const std::string &name);
  void load(const std::string &name);
  void remove(const std::string &name);

  IO_ini ini = IO_ini(".\\bandolier.ini");      // Filename updated later to per character.
};
