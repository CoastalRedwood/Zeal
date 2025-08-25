#pragma once
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

struct ZealCommand {
  std::vector<std::string> aliases;
  std::string description;
  std::function<bool(std::vector<std::string> &args)> callback;

  ZealCommand(std::vector<std::string> _aliases, std::string _description,
              std::function<bool(std::vector<std::string> &args)> _callback) {
    callback = _callback;
    aliases = _aliases;
    description = _description;
  }

  ZealCommand(){};
};

class ChatCommands {
 public:
  void print_commands();

  // Register a callback that returns the target character of the active tell window. Empty if none.
  void add_get_tell_name_callback(std::function<std::string()> callback) { tell_callback = callback; };

  bool handle_chat(std::string &str_cmd) const;  // Updates str with a tell target if needed.

  ChatCommands(class ZealService *zeal);
  ~ChatCommands();
  void Add(std::string cmd, std::vector<std::string> aliases, std::string description,
           std::function<bool(std::vector<std::string> &args)> callback);
  std::unordered_map<std::string, ZealCommand> CommandFunctions;
  std::function<std::string()> tell_callback;
};
