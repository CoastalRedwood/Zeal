#pragma once
#include <Windows.h>

class CrashHandler {
 public:
  CrashHandler();
  ~CrashHandler();

 private:
  PVOID exception_handler;
};
