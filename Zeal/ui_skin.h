#pragma once

#include <filesystem>

class UISkin {
 public:
  UISkin() = delete;  // Prevent object creation. Use initialize and static members.

  static void initialize_mode(class ZealService* zeal);  // Call to query ini settings and patch code if needed.

  static bool configuration_check();  // Returns false if missing files. Also shows warning dialog.

  static bool is_zeal_xml_file(const std::string& xml_file);  // Returns true if a zeal file.

  static std::vector<const char*> get_zeal_ui_xml_files();  // Returns file list to add to ui xml file.

  static std::filesystem::path get_zeal_xml_path() { return zeal_xml_path; }

  static std::filesystem::path get_zeal_resources_path() { return zeal_resources_path; }

  static std::string get_global_default_ui_skin_name();  // Returns the default UISkin in client.ini

  static bool is_big_fonts_mode() { return is_big_fonts; }  // Set at boot by default UISkin.

  static bool is_ui_skin_big_fonts_mode(const char* ui_skin);  // Checks if a specific UISkin is a big fonts version.

 private:
  static constexpr char kDefaultZealFileSubfolder[] = "zeal";             // In uifiles/zeal.
  static constexpr char kBigFontsXmlSubfolder[] = "big_xml";              // In uifiles/zeal/big_fonts.
  static constexpr char kBigFontsTriggerFilename[] = "zeal_ui_skin.ini";  // In ui skin folder.

  inline static std::filesystem::path zeal_xml_path;        // Default xml or to big_fonts subfolder.
  inline static std::filesystem::path zeal_resources_path;  // Same as default xml path.
  inline static bool is_big_fonts = false;                  // Set to true if the trigger filename exists.
};
