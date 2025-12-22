#include "crash_handler.h"

#include <dbghelp.h>

#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "callbacks.h"
#include "game_functions.h"
#include "miniz.h"
#include "zeal.h"

std::vector<DWORD> nonCrashExceptionCodes = {
    DBG_PRINTEXCEPTION_C,          // 0x40010006, OutputDebugString exception for ASCII strings
    DBG_COMMAND_EXCEPTION,         // 0x40010009, Used for internal command execution during debugging
    DBG_PRINTEXCEPTION_WIDE_C,     // 0x4001000A, OutputDebugString exception for wide-character strings
    DBG_CONTROL_C,                 // 0x40010005, Control-C exception for console applications
    EXCEPTION_BREAKPOINT,          // 0x80000003, Used by debuggers to temporarily suspend execution
    EXCEPTION_SINGLE_STEP,         // 0x80000004, Used by debuggers for single-step tracing
    STATUS_GUARD_PAGE_VIOLATION,   // 0x80000001, Occurs on a stack overflow guard page hit
    STATUS_DATATYPE_MISALIGNMENT,  // 0x80000002, Memory access is misaligned
    STATUS_STACK_OVERFLOW,         // 0xC00000FD, Stack overflow occurred but might be recoverable
    0x406D1388,                    // Exception used to set thread names for debugging
    0x80000007,                    // Used to wake up the system debugger
    0xe06d7363                     // C++ exception, funny enough in ascii this code is 'MSC'
};

// Define a map to store exception codes and their descriptions
std::map<DWORD, std::string> exceptionCodeStrings = {
    {EXCEPTION_ACCESS_VIOLATION, "EXCEPTION_ACCESS_VIOLATION"},
    {EXCEPTION_ARRAY_BOUNDS_EXCEEDED, "EXCEPTION_ARRAY_BOUNDS_EXCEEDED"},
    {EXCEPTION_BREAKPOINT, "EXCEPTION_BREAKPOINT"},
    {EXCEPTION_DATATYPE_MISALIGNMENT, "EXCEPTION_DATATYPE_MISALIGNMENT"},
    {EXCEPTION_FLT_DENORMAL_OPERAND, "EXCEPTION_FLT_DENORMAL_OPERAND"},
    {EXCEPTION_FLT_DIVIDE_BY_ZERO, "EXCEPTION_FLT_DIVIDE_BY_ZERO"},
    {EXCEPTION_FLT_INEXACT_RESULT, "EXCEPTION_FLT_INEXACT_RESULT"},
    {EXCEPTION_FLT_INVALID_OPERATION, "EXCEPTION_FLT_INVALID_OPERATION"},
    {EXCEPTION_FLT_OVERFLOW, "EXCEPTION_FLT_OVERFLOW"},
    {EXCEPTION_FLT_STACK_CHECK, "EXCEPTION_FLT_STACK_CHECK"},
    {EXCEPTION_FLT_UNDERFLOW, "EXCEPTION_FLT_UNDERFLOW"},
    {EXCEPTION_ILLEGAL_INSTRUCTION, "EXCEPTION_ILLEGAL_INSTRUCTION"},
    {EXCEPTION_IN_PAGE_ERROR, "EXCEPTION_IN_PAGE_ERROR"},
    {EXCEPTION_INT_DIVIDE_BY_ZERO, "EXCEPTION_INT_DIVIDE_BY_ZERO"},
    {EXCEPTION_INT_OVERFLOW, "EXCEPTION_INT_OVERFLOW"},
    {EXCEPTION_INVALID_DISPOSITION, "EXCEPTION_INVALID_DISPOSITION"},
    {EXCEPTION_NONCONTINUABLE_EXCEPTION, "EXCEPTION_NONCONTINUABLE_EXCEPTION"},
    {EXCEPTION_PRIV_INSTRUCTION, "EXCEPTION_PRIV_INSTRUCTION"},
    {EXCEPTION_SINGLE_STEP, "EXCEPTION_SINGLE_STEP"},
    {EXCEPTION_STACK_OVERFLOW, "EXCEPTION_STACK_OVERFLOW"},
    // Add more exception codes as needed
};

void EnsureCrashesFolderExists() {
  std::filesystem::path crash_folder = Zeal::Game::get_game_path() / std::filesystem::path("crashes");
  if (std::filesystem::is_directory(crash_folder)) return;

  std::error_code ec;
  std::filesystem::create_directory(crash_folder, ec);
  if (ec) std::cerr << "Error creating directory: " << ec.message() << " (code: " << ec.value() << ")" << std::endl;
}

std::string GetModuleNameFromAddress(LPVOID address) {
  HMODULE hModule;
  DWORD_PTR dwOffset;
  char modulePath[MAX_PATH];

  // Get module handle from address
  if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                        reinterpret_cast<LPCWSTR>(address), &hModule) != 0) {
    // Get module file name
    if (GetModuleFileNameA(hModule, modulePath, MAX_PATH) != 0) {
      return modulePath;
    }
  }

  return "";
}

std::string ZipCrash(const std::string &folderName, const std::string &dumpFilePath,
                     const std::string &reasonFilePath) {
  // Zip the files
  std::time_t t = std::time(nullptr);
  std::tm tm;
  localtime_s(&tm, &t);
  std::ostringstream CrashFileName;
  CrashFileName << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S");

  std::string zipFilePath = Zeal::Game::get_game_path().string() + "\\crashes\\" + CrashFileName.str() + ".zip";
  mz_zip_archive zip_archive;
  memset(&zip_archive, 0, sizeof(zip_archive));

  if (!mz_zip_writer_init_file(&zip_archive, zipFilePath.c_str(), 0)) {
    std::cerr << "Failed to initialize zip archive." << std::endl;
    return "";
  }

  if (!mz_zip_writer_add_file(&zip_archive, "minidump.dmp", dumpFilePath.c_str(), NULL, 0, MZ_BEST_COMPRESSION)) {
    std::cerr << "Failed to add minidump to zip archive." << std::endl;
    mz_zip_writer_end(&zip_archive);
    return "";
  }

  if (!mz_zip_writer_add_file(&zip_archive, "crash_reason.txt", reasonFilePath.c_str(), NULL, 0, MZ_BEST_COMPRESSION)) {
    std::cerr << "Failed to add crash reason to zip archive." << std::endl;
    mz_zip_writer_end(&zip_archive);
    return "";
  }

  mz_zip_writer_finalize_archive(&zip_archive);
  mz_zip_writer_end(&zip_archive);

  // Clean up the original files
  DeleteFileA(dumpFilePath.c_str());
  DeleteFileA(reasonFilePath.c_str());
  RemoveDirectoryA(folderName.c_str());
  return CrashFileName.str();
}

static bool HandleCrashSender(EXCEPTION_POINTERS *pep, const std::string &CrashFileName, const std::string &reason) {
  std::string sender_filepath = Zeal::Game::get_game_path().string() + "\\crashes\\ZealCrashSender.exe";
  if (!std::filesystem::exists(sender_filepath)) {
    return false;
  }

  std::string moduleName = GetModuleNameFromAddress(pep->ExceptionRecord->ExceptionAddress);

  std::stringstream arguments;
  arguments << "\"";
  arguments << "Version: " << ZEAL_VERSION << std::endl;
  arguments << "Reason: " << reason << std::endl;
  arguments << "Exception: " << exceptionCodeStrings[pep->ExceptionRecord->ExceptionCode] << std::endl;
  arguments << "Address: 0x" << pep->ExceptionRecord->ExceptionAddress << std::endl;
  arguments << "Module Information: " << moduleName << std::endl;
  arguments << "Zipped Crash: " << CrashFileName << ".zip";
  arguments << "\" ";
  arguments << "\"" << CrashFileName << ".zip\" ";

  std::string cmdLine = sender_filepath + " " + arguments.str();

  // Convert to writable format
  size_t bufferSize = cmdLine.size() + 1;
  char *cmdLineWritable = new char[cmdLine.size() + 1];
  strcpy_s(cmdLineWritable, bufferSize, cmdLine.c_str());

  // Set up STARTUPINFO and PROCESS_INFORMATION structures
  STARTUPINFOA si;
  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);

  PROCESS_INFORMATION pi;
  ZeroMemory(&pi, sizeof(pi));

  // Start the process
  CreateProcessA(NULL,              // No module name (use command line)
                 cmdLineWritable,   // Command line
                 NULL,              // Process handle not inheritable
                 NULL,              // Thread handle not inheritable
                 FALSE,             // Set handle inheritance to FALSE
                 CREATE_NO_WINDOW,  // No window
                 NULL,              // Use parent's environment block
                 NULL,              // Use parent's starting directory
                 &si,               // Pointer to STARTUPINFO structure
                 &pi);              // Pointer to PROCESS_INFORMATION structure
  CloseHandle(pi.hProcess);         // Release to prevent leaks. It will still run standalone.
  CloseHandle(pi.hThread);
  return true;
}

static std::string GetCrashMessage(EXCEPTION_POINTERS *pep, bool extra_data) {
  std::stringstream reasonStream;
  if (pep == nullptr || pep->ExceptionRecord == nullptr) {
    reasonStream << "No exception information." << std::endl;
    reasonStream << "Zeal Version: " << ZEAL_VERSION << " (" << ZEAL_BUILD_VERSION << ")" << std::endl;
    return reasonStream.str();
  }

  reasonStream << "Exception Code: 0x" << std::hex << pep->ExceptionRecord->ExceptionCode << std::endl;
  if (exceptionCodeStrings.count(pep->ExceptionRecord->ExceptionCode))
    reasonStream << "Exception String: " << std::hex << exceptionCodeStrings[pep->ExceptionRecord->ExceptionCode]
                 << std::endl;
  reasonStream << "Exception Address: 0x" << std::hex << pep->ExceptionRecord->ExceptionAddress << std::endl;
  // Get and write module information
  std::string moduleName = GetModuleNameFromAddress(pep->ExceptionRecord->ExceptionAddress);
  if (!moduleName.empty()) {
    reasonStream << "Exception occurred in module: " << moduleName << std::endl;
  } else {
    reasonStream << "Module information not available." << std::endl;
  }
  reasonStream << "Zeal Version: " << ZEAL_VERSION << " (" << ZEAL_BUILD_VERSION << ")" << std::endl;
  // Add more details as needed from pep->ExceptionRecord and pep->ContextRecord
  if (extra_data) {
    Zeal::GameStructures::GAMECHARINFO *char_info = Zeal::Game::get_char_info();
    Zeal::GameStructures::Entity *spawn_info = (char_info ? char_info->SpawnInfo : nullptr);
    Zeal::GameStructures::Entity *self = Zeal::Game::get_self();
    reasonStream << "Character: " << (char_info ? char_info->Name : "Unknown") << std::endl;
    reasonStream << "UI Skin: " << Zeal::Game::get_ui_skin() << std::endl;
    int zone_id = self ? self->ZoneId : -1;
    reasonStream << "Zone ID: " << zone_id << std::endl;
    reasonStream << "Game state: " << Zeal::Game::get_gamestate() << std::endl;
    if (ZealService::get_instance() && ZealService::get_instance()->callbacks)
      reasonStream << "Callbacks: " << ZealService::get_instance()->callbacks->get_trace() << std::endl;
    if (!char_info) reasonStream << "GAMECHARINFO: 0x" << std::hex << (uint32_t)(char_info) << std::endl;
    if (!self || !spawn_info || self != spawn_info) {
      reasonStream << "SpawnInfo: 0x" << std::hex << (uint32_t)(spawn_info) << std::endl;
      reasonStream << "Self: 0x" << std::hex << (uint32_t)(self) << std::dec << std::endl;
    }
    int show_spell_effects = *reinterpret_cast<unsigned int *>(0x007cf290);
    const BYTE kOpcodeNop = 0x90;
    const int kDoSpriteEffectAddr = 0x0052cbb1;
    bool sprites_disabled = (*reinterpret_cast<BYTE *>(kDoSpriteEffectAddr) == kOpcodeNop);
    if (show_spell_effects)
      reasonStream << "ShowSpellEffects: " << show_spell_effects << " NoSprites: " << sprites_disabled << std::endl;
    if (ZealService::get_heap_failed_line())
      reasonStream << "BootHeapCheck: " << ZealService::get_heap_failed_line() << std::endl;
    int error_count = ZealService::get_instance()->crash_handler->get_xml_error_count();
    if (error_count) reasonStream << "Unknown uierrors.txt error count: " << error_count << std::endl;
  }
  return reasonStream.str();
}

void WriteMiniDump(EXCEPTION_POINTERS *pep, const std::string &reason, const std::string &extra_reason) {
  // Get the current time for a unique folder name
  EnsureCrashesFolderExists();

  // Create the unique temporary folder for zipping.
  std::time_t t = std::time(nullptr);
  std::tm tm;
  localtime_s(&tm, &t);
  std::ostringstream folderNameStream;
  folderNameStream << Zeal::Game::get_game_path().string() << "\\crashes\\" << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S");
  std::string folderName = folderNameStream.str();

  if (!CreateDirectoryA(folderName.c_str(), NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
    std::cerr << "Could not create temporary dump folder." << std::endl;
    return;
  }

  // Create the mini-dump file handle.
  std::string dumpFilePath = folderName + "\\minidump.dmp";
  HANDLE hFile = CreateFileA(dumpFilePath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

  if (hFile == INVALID_HANDLE_VALUE) {
    std::cerr << "Could not create minidump file." << std::endl;
    RemoveDirectoryA(folderName.c_str());
    return;
  }

  // Create the mini-dump output file.
  MINIDUMP_EXCEPTION_INFORMATION mdei;
  mdei.ThreadId = GetCurrentThreadId();
  mdei.ExceptionPointers = pep;
  mdei.ClientPointers = FALSE;
  // MiniDumpWithPrivateReadWriteMemory
  MINIDUMP_TYPE mdt = (MINIDUMP_TYPE)(MiniDumpWithHandleData | MiniDumpWithProcessThreadData | MiniDumpWithThreadInfo |
                                      MiniDumpWithUnloadedModules);
  BOOL result = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, mdt, (pep != 0) ? &mdei : 0, 0, 0);
  CloseHandle(hFile);
  if (!result) {
    std::cerr << "Failed to write dump file." << std::endl;
    RemoveDirectoryA(folderName.c_str());
    return;
  }

  // Create the reason file.
  std::string reasonFilePath = folderName + "\\crash_reason.txt";
  std::ofstream reasonFile(reasonFilePath);
  std::stringstream reasonStream;
  if (reasonFile.is_open()) {
    std::string moduleName = "";
    reasonStream << "Unhandled exception occurred: " << extra_reason << std::endl << std::endl << reason;
    reasonFile << reasonStream.str();
    reasonFile.close();
  }

  // Zip up the files (also deletes the temporary folder and files).
  std::string CrashFileName = ZipCrash(folderName, dumpFilePath, reasonFilePath);

  // Trigger optional crash sender.
  // Note: Disabled to avoid using crashsender.exe versions installed by earlier Zeal versions.
  // HandleCrashSender(pep, CrashFileName, reasonStream.str());
}

void ShowCrashLoopDialog(PEXCEPTION_POINTERS pep) {
  std::string message = "Fatal crash loop. Unable to create a crash zip.";
  if (pep != nullptr && pep->ExceptionRecord != nullptr) {
    const auto &record = pep->ExceptionRecord;
    message += std::format(" Exception code: {:#x}, Address: {:#x}, Module: {}", record->ExceptionCode,
                           reinterpret_cast<DWORD>(record->ExceptionAddress),
                           GetModuleNameFromAddress(record->ExceptionAddress));
  }
  MessageBoxA(NULL, message.c_str(), "D'oh! Crash loop", MB_OK | MB_ICONERROR);
}

LONG CALLBACK VectoredExceptionHandler(PEXCEPTION_POINTERS pExceptionInfo) {
  static int crashes = 0;

  // Check for non-crash exceptions and return early if detected
  if (pExceptionInfo != nullptr && pExceptionInfo->ExceptionRecord != nullptr) {
    DWORD exceptionCode = pExceptionInfo->ExceptionRecord->ExceptionCode;
    for (DWORD nonCrashCode : nonCrashExceptionCodes) {
      if (exceptionCode == nonCrashCode) return EXCEPTION_CONTINUE_SEARCH;  // Continue searching for other handlers.
    }
  }

  // Count crashes to avoid infinite crash looping.  Show dialog on the first.
  crashes++;
  if (crashes == 1) {
    std::string reason = GetCrashMessage(pExceptionInfo, true);
    std::wstring message = std::wstring(reason.begin(), reason.end());
    MessageBox(NULL, message.c_str(), L"D'oh! Client crash", MB_OK | MB_SYSTEMMODAL | MB_ICONERROR);
    WriteMiniDump(pExceptionInfo, reason, "Initial Handler");
  } else if (crashes == 2) {
    std::string reason = GetCrashMessage(pExceptionInfo, false);
    WriteMiniDump(pExceptionInfo, reason, "Multiple Crashes");
  } else {
    std::cerr << "Crash loop detected exiting process" << std::endl;
    if (crashes == 3) ShowCrashLoopDialog(pExceptionInfo);
    ExitProcess(1);
  }
  return EXCEPTION_CONTINUE_SEARCH;  // Continue searching for other handlers
}

CrashHandler::CrashHandler() { exception_handler = AddVectoredExceptionHandler(0, VectoredExceptionHandler); }

CrashHandler::~CrashHandler() { RemoveVectoredExceptionHandler(exception_handler); }
