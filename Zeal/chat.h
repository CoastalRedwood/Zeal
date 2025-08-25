#pragma once

#include <functional>
#include <string>
#include <vector>

#include "zeal_settings.h"

class Chat {
 public:
  ZealSetting<bool> UseClassicClassNames = {false, "Zeal", "ClassicClasses", false, [this](bool val) { set_classes(); },
                                            true};
  ZealSetting<bool> UseBlueCon = {true, "Zeal", "Bluecon", false};
  ZealSetting<bool> UseZealInput = {true, "Zeal", "ZealInput", false};
  ZealSetting<bool> UseUniqueNames = {false, "Zeal", "UniqueNames", false};
  ZealSetting<int> TimeStampsStyle = {0, "Zeal", "ChatTimestamps", false};

  std::function<unsigned int(int)> get_color_callback;

  void set_classes();

  void add_get_color_callback(std::function<unsigned int(int index)> callback) { get_color_callback = callback; };

  void add_key_press_callback(std::function<bool(int key, bool down, int modifier)> callback) {
    key_press_callback = callback;
  }

  void add_print_chat_callback(std::function<void(const char *data, int color_index)> callback) {
    print_chat_callbacks.push_back(callback);
  }

  void handle_print_chat(const char *data, int color_index);

  bool handle_key_press(int key, bool down, int modifier);

  void DoPercentReplacements(std::string &str_data);
  Chat(class ZealService *pHookWrapper);
  ~Chat();

 private:
  void InitPercentReplacements();
  std::vector<std::function<void(std::string &str_data)>> percent_replacements;
  std::vector<std::function<void(const char *data, int color_index)>> print_chat_callbacks;
  std::function<bool(int key, bool down, int modifier)> key_press_callback;
};
