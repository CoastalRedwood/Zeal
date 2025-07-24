#pragma once
#include <windows.h>
#include <dbghelp.h>

#include <fstream>
#include <iostream>
#include <string>

class CrashHandler {
 public:
  CrashHandler();
  ~CrashHandler();

 private:
  PVOID exception_handler;
};
