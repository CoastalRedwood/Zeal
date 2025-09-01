#include "commands.h"

#include <sstream>

// Do not add additional headers to this list. Add those commands in zeal.cpp.
#include "game_addresses.h"
#include "game_functions.h"
#include "game_packets.h"
#include "game_structures.h"
#include "hook_wrapper.h"
#include "memory.h"
#include "string_util.h"
#include "zeal.h"

void ChatCommands::print_commands() {
  std::stringstream ss;
  ss << "List of commands" << std::endl;
  ss << "-----------------------------------------------------" << std::endl;
  for (auto &[name, c] : CommandFunctions) {
    ss << name;
    if (c.aliases.size() > 0) ss << " [";
    for (auto it = c.aliases.begin(); it != c.aliases.end(); ++it) {
      auto &a = *it;
      ss << a;
      if (std::next(it) != c.aliases.end()) {
        ss << ", ";
      }
    }
    if (c.aliases.size() > 0) ss << "]";

    ss << " " << c.description;
    ss << std::endl;
  }
  Zeal::Game::print_chat(ss.str());
}

bool ChatCommands::handle_chat(std::string &str_cmd) const {
  if (!tell_callback) return false;
  auto name = tell_callback();
  if (name.empty()) return false;
  str_cmd = "/tell " + name + " " + str_cmd;
  return true;
}

void __fastcall InterpretCommand(int c, int unused, Zeal::GameStructures::Entity *player, const char *cmd) {
  ZealService *zeal = ZealService::get_instance();
  std::string str_cmd = Zeal::String::trim_and_reduce_spaces(cmd);
  if (str_cmd.length() == 0) return;

  // Support tell windows by optionally prepending the /tell target based on chat window.
  if (str_cmd.length() > 0 && str_cmd.front() != '/' && zeal->commands_hook->handle_chat(str_cmd))
    cmd = str_cmd.c_str();  // Updates the cmd going to the client call.

  std::vector<std::string> args = Zeal::String::split(str_cmd, " ");
  const std::string &cmd_name = args.front();
  if (!args.empty() && !cmd_name.empty() && Zeal::Game::is_in_game()) {
    bool cmd_handled = false;
    auto &command_functions = zeal->commands_hook->CommandFunctions;
    if (command_functions.count(cmd_name)) {
      cmd_handled = command_functions[args[0]].callback(args);
    } else {
      // attempt to find and call the function via an alias
      for (const auto &command_pair : command_functions) {
        const auto &command = command_pair.second;
        if (command.callback && !command.aliases.empty()) {
          for (const auto alias : command.aliases) {
            if (!alias.empty() && alias == cmd_name) {
              cmd_handled = command.callback(args);
              break;
            }
          }
        }
      }
    }
    if (cmd_handled) {
      return;
    }
  }
  zeal->hooks->hook_map["commands"]->original(InterpretCommand)(c, unused, player, cmd);
}

void ChatCommands::Add(std::string cmd, std::vector<std::string> aliases, std::string description,
                       std::function<bool(std::vector<std::string> &args)> callback) {
  CommandFunctions[cmd] = ZealCommand(aliases, description, callback);
}

ChatCommands::~ChatCommands() {}

// call interpret command without hitting the detour, useful for aliasing default commands
void ForwardCommand(std::string cmd) {
  ZealService::get_instance()->hooks->hook_map["commands"]->original(InterpretCommand)(
      (int)Zeal::Game::get_game(), 0, Zeal::Game::get_self(), cmd.c_str());
}

ChatCommands::ChatCommands(ZealService *zeal) {
  Add("/crash", {}, "Tests a crash", [](std::vector<std::string> &args) {
    int *x = 0;
#pragma warning(suppress : 6011)
    *x = 0;  // nullptr exception
    return false;
  });
  Add("o", {}, "Removes the o command that is switching ui from new to old.", [](std::vector<std::string> &args) {
    if (Zeal::String::compare_insensitive(args[0], "o")) return true;
    return false;
  });
  Add("/target", {"/cleartarget"},
      "Adds clear target functionality to the /target command if you give it no arguments.",
      [](std::vector<std::string> &args) {
        if (args.size() == 1) {
          Zeal::Game::set_target(0);
          return true;  // return true to stop the game from processing any further on this command, false if you want
                        // to just add features to an existing cmd
        }
        return false;
      });
  Add("/cls", {}, "Adds cls alias for clearchat.", [](std::vector<std::string> &args) {
    ForwardCommand("/clearchat");
    return true;
  });
  Add("/autoinventory", {"/autoinv", "/ai"}, "Puts whatever is on your cursor into your inventory.",
      [](std::vector<std::string> &args) {
        if (Zeal::Game::can_inventory_item(Zeal::Game::get_char_info()->CursorItem)) {
          Zeal::Game::GameInternal::auto_inventory(Zeal::Game::get_char_info(),
                                                   &Zeal::Game::get_char_info()->CursorItem, 0);
        } else {
          if (Zeal::Game::get_char_info()->CursorItem)
            Zeal::Game::print_chat(USERCOLOR_LOOT, "Cannot auto inventory %s",
                                   Zeal::Game::get_char_info()->CursorItem->Name);
        }
        return true;  // return true to stop the game from processing any further on this command, false if you want to
                      // just add features to an existing cmd
      });
  Add("/testinventory", {}, "Test cursor item to see if it can be inventoried", [](std::vector<std::string> &args) {
    Zeal::Game::print_chat("You %s inventory that item",
                           Zeal::Game::can_inventory_item(Zeal::Game::get_char_info()->CursorItem) ? "can" : "cannot");
    return true;  // return true to stop the game from processing any further on this command, false if you want to
                  // just add features to an existing cmd
  });
  Add("/aspectratio", {"/ar"}, "Change your aspect ratio.", [](std::vector<std::string> &args) {
    Zeal::GameStructures::CameraInfo *ci = Zeal::Game::get_camera();
    if (ci) {
      float ar = 0;
      if (args.size() > 1 && Zeal::String::tryParse(args[1], &ar)) {
        ci->AspectRatio = ar;
      } else {
        Zeal::Game::print_chat("Current Aspect Ratio [%f]", ci->AspectRatio);
      }
    }

    return true;  // return true to stop the game from processing any further on this command, false if you want to
                  // just add features to an existing cmd
  });
  Add("/clearchat", {}, "Adds clearchat functionality to oldui.", [](std::vector<std::string> &args) {
    // each line can hold up to 256 characters but typically doesnt reach that far.
    // each unused line is set with '@\0', so lets clear to that
    if (!Zeal::Game::is_new_ui()) {
      int start = 0x799D8D;
      int end = 0x7B908D;
      while (start != end) {
        mem::write<WORD>(start, 0x4000);
        start += 256;
      }
      // max scrollbar height doesn't reset properly here. Need to figure out why still.
      return true;
    }

    return false;
  });
  Add("/sit", {}, "Adds arguments on/down to force you to sit down instead of just toggling.",
      [](std::vector<std::string> &args) {
        if (Zeal::Game::is_mounted())
          Zeal::Game::print_chat("Must dismount to /sit");  // Toss out a warning. Let downstream fail.
        if (args.size() > 1 && args.size() < 3) {
          if (Zeal::String::compare_insensitive(args[1], "on") || Zeal::String::compare_insensitive(args[1], "down")) {
            Zeal::Game::sit();
            return true;
          }
        }
        return false;
      });
  Add("/run", {}, "Sets run mode (toggle, on, off (walk)).", [](const std::vector<std::string> &args) {
    BYTE *run_mode = reinterpret_cast<BYTE *>(0x0079856d);
    if (args.size() == 2 && Zeal::String::compare_insensitive(args[1], "on"))
      *run_mode = 1;
    else if (args.size() == 2 && Zeal::String::compare_insensitive(args[1], "off"))
      *run_mode = 0;
    else if (args.size() == 1) {
      *run_mode = (*run_mode == 0);
      Zeal::Game::print_chat("%s", *run_mode ? "Run on" : "Walk on");
    } else
      Zeal::Game::print_chat("Usage: /run (toggles), /run on, /run off");
    return true;
  });
  Add("/showlootlockouts", {"/sll", "/showlootlockout", "/showlockouts"}, "Sends #showlootlockouts to server.",
      [](std::vector<std::string> &args) {
        Zeal::Game::do_say(true, "#showlootlockouts");
        return true;
      });

  Add("/clienthptick", {"/cht"}, "Toggle client health tick (disabled by default in this client).",
      [this](std::vector<std::string> &args) {
        BYTE orig1[9] = {0x55, 0x50, 0x8B, 0xCE, 0xE8, 0x10, 0x65, 0xFF, 0xFF};
        BYTE orig2[2] = {0x0F, 0x89};
        BYTE orig3a[2] = {0x55, 0x57};
        BYTE orig3b[5] = {0xE8, 0x38, 0x64, 0xFF, 0xFF};
        BYTE orig4[9] = {0x55, 0x57, 0x8B, 0xCE, 0xE8, 0x11, 0x62, 0xFF, 0xFF};
        if (*(BYTE *)0x4C28B5 == 0x90) {
          mem::copy(0x4C28B5, (int)orig1, sizeof(orig1));
          mem::copy(0x4C28EF, (int)orig2, sizeof(orig2));
          mem::copy(0x4C298B, (int)orig3a, sizeof(orig3a));
          mem::copy(0x4C2991, (int)orig3b, sizeof(orig3b));
          mem::copy(0x4C2BB4, (int)orig4, sizeof(orig4));
          Zeal::Game::print_chat("Client sided heath tick re-enabled");
        } else {
          // (1) ModifyCurHP -> nop
          mem::set(0x4C28B5, 0x90, 9);
          // (2) skip negative hp check for mez break
          mem::set(0x4C28EF, 0x90, 1);
          mem::set(0x4C28EF + 1, 0xE9, 1);
          // (3) ModifyCurHP -> nop
          mem::set(0x4C298B, 0x90, 2);
          mem::set(0x4C2991, 0x90, 5);
          // (4) ModifyCurHP -> nop
          mem::set(0x4C2BB4, 0x90, 9);
          Zeal::Game::print_chat("Client sided health tick disabled");
        }
        return true;
      });
  Add("/clientmanatick", {"/cmt"}, "Toggle client mana tick (disabled by default in this client).",
      [this](std::vector<std::string> &args) {
        BYTE orig1[7] = {0x66, 0x01, 0xBE, 0x9A, 0x0, 0x0, 0x0};  // 0x4C3F93
        BYTE orig2[7] = {0x66, 0x01, 0x87, 0x9A, 0x0, 0x0, 0x0};  // 0x4c7642
        if (*(BYTE *)0x4C3F93 == 0x90) {
          mem::copy(0x4C3F93, (int)orig1, sizeof(orig1));
          mem::copy(0x4C7642, (int)orig2, sizeof(orig2));
          Zeal::Game::print_chat("Client sided mana tick re-enabled");
        } else {
          mem::set(0x4C3F93, 0x90, sizeof(orig1));
          mem::set(0x4C7642, 0x90, sizeof(orig2));
          Zeal::Game::print_chat("Client sided mana tick disabled");
        }
        return true;
      });

  Add("/reloadskin", {}, "Reload your current ui with ini.", [this](std::vector<std::string> &args) {
    mem::write<BYTE>(0x8092d9, 1);  // reload skin
    mem::write<BYTE>(0x8092da, 1);  // reload with ui
    return true;
  });
  Add("/inspect", {}, "Inspect your current target.", [this, zeal](std::vector<std::string> &args) {
    if (args.size() > 1 && args[1] == "target") {
      Zeal::GameStructures::Entity *t = Zeal::Game::get_target();
      if (!t || t->Type != 0) {
        Zeal::Game::print_chat("That is not a valid target to inspect!");
        return true;
      } else {
        Zeal::Game::do_inspect(t);
        return true;
      }
    }
    return false;
  });
  Add("/lead", {}, "Print current group and raid leaders", [this, zeal](std::vector<std::string> &args) {
    Zeal::GameStructures::RaidInfo *raid_info = Zeal::Game::RaidInfo;

    if (raid_info->is_in_raid()) {
      bool show_all = (args.size() == 2 && args[1] == "all");
      bool show_open = (args.size() == 2 && args[1] == "open");
      Zeal::Game::print_raid_leaders(show_all, show_open);
    } else
      Zeal::Game::print_group_leader();

    if (args.size() == 2 && args[1] == "dump") Zeal::Game::dump_raid_state();

    return true;
  });
  Add("/help", {"/hel"}, "Adds zeal to the help list.", [this](std::vector<std::string> &args) {
    if (args.size() == 1) {
      std::stringstream ss;
      ss << "Format: /help <class> Where class is one of normal, emote, guild, ect.." << std::endl;
      ss << "Normal will display a list of all commands." << std::endl;
      ss << "Emote will display a list of all player emotes." << std::endl;
      ss << "Guild will display a list of guild commands." << std::endl;
      ss << "Voice will display a list of voice control commands." << std::endl;
      ss << "Chat will display a list of chat channel commands." << std::endl;
      ss << "Zeal will display a list of custom commands." << std::endl;
      Zeal::Game::print_chat(ss.str());
      return true;
    }
    if (args.size() > 1 && Zeal::String::compare_insensitive(args[1], "zeal")) {
      print_commands();
      return true;
    }
    return false;
  });
  zeal->hooks->Add("commands", Zeal::Game::GameInternal::fn_interpretcmd, InterpretCommand, hook_type_detour);
}
