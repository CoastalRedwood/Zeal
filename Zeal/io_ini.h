#pragma once
#include <Windows.h>

#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// Declare a single function from game_functions.h to avoid pulling in too many headers.
namespace Zeal::Game {
void print_chat(const char *format, ...);
}

class IO_ini {
 private:
  std::string filename;

 public:
  static constexpr char kClientFilename[] = ".\\eqclient.ini";
  static constexpr char kZealIniFilename[] = ".\\zeal.ini";

  IO_ini(const std::string &filename) : filename(filename){};

  void set(std::string path) { filename = path; }

  bool exists(const std::string &section, const std::string &key) const {
    char buffer[256];
    DWORD bytesRead =
        GetPrivateProfileStringA(section.c_str(), key.c_str(), "", buffer, sizeof(buffer), filename.c_str());

    if (bytesRead == 0) {
      return false;
    }
    return true;
  }

  std::vector<std::string> getSectionNames() {
    std::vector<std::string> sectionNames;
    const DWORD bufferSize = 4096;  // Adjust buffer size as needed
    char buffer[bufferSize];

    DWORD result = GetPrivateProfileSectionNamesA(buffer, bufferSize, filename.c_str());
    if (result == 0) {
      std::cerr << "Failed to read INI file: " << filename << std::endl;
      return sectionNames;
    }

    for (char *p = buffer; *p != '\0'; p += strlen(p) + 1) {
      sectionNames.push_back(p);
    }

    return sectionNames;
  }

  bool deleteSection(const std::string &sectionName) {
    // Delete the section and its contents by writing an empty string to it
    if (!WritePrivateProfileSectionA(sectionName.c_str(), nullptr, filename.c_str())) {
      return false;
    } else {
      return true;
    }
  }

  template <typename T>
  T getValue(std::string section, std::string key) const {
    // Grows until the value fits. GetPrivateProfileStringA does not report that it truncated - it
    // just returns nSize-1 and hands back a short string - so a fixed buffer silently corrupts any
    // setting longer than it. The 256-byte one here turned a 300-character sealed OTLP token into
    // 255 characters of unparseable base64, and the token read back as "not set" with nothing
    // logged anywhere.
    std::vector<char> buffer(256);
    DWORD bytesRead = 0;
    for (;;) {
      bytesRead = GetPrivateProfileStringA(section.c_str(), key.c_str(), "", buffer.data(),
                                           static_cast<DWORD>(buffer.size()), filename.c_str());
      // Short of the cap means the whole value came back.
      if (bytesRead < buffer.size() - 1) break;
      if (buffer.size() >= (1u << 16)) break;  // 64 KB: no ini setting is this long, stop growing
      buffer.resize(buffer.size() * 2);
    }

    if (bytesRead == 0) {
      return T{};
    }
    if constexpr (std::is_same_v<T, std::string>) return std::string(buffer.data(), bytesRead);
    return convertFromString<T>(std::string(buffer.data(), bytesRead));
  }

  template <typename T>
  void setValue(const std::string &section, const std::string &key, const T &value) {
    std::string valueStr;
    if constexpr (std::is_same_v<T, bool>) {
      valueStr = value ? "TRUE" : "FALSE";
    } else if constexpr (!std::is_same_v<T, std::string>) {
      valueStr = std::to_string(value);
    } else {
      valueStr = value;
    }
    BOOL result = WritePrivateProfileStringA(section.c_str(), key.c_str(), valueStr.c_str(), filename.c_str());
    if (!result) {
      Zeal::Game::print_chat("Error writing value to INI file.");
    }
  }

 private:
  template <typename T>
  T convertFromString(const std::string &str) const {
    if constexpr (std::is_same_v<T, bool>) {
      if (str == "TRUE")
        return true;
      else
        return false;
    }
    std::istringstream iss(str);
    T value;
    iss >> std::boolalpha >> value;
    return value;
  }
};
