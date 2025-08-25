#pragma once
#include <Windows.h>

class CrashHandler {
 public:
  CrashHandler();
  ~CrashHandler();

  void increment_xml_error_count() { xml_error_count++; };

  int get_xml_error_count() const { return xml_error_count; };

 private:
  PVOID exception_handler;
  int xml_error_count = 0;
};
