#include <Windows.h>

#include "zeal.h"

// Returns true if it detects that a version of Zeal has already been loaded.
static bool is_zeal_already_loaded() {
  // Simple hack check is to see if client sided mana ticking was disabled (since before 0.3.0).
  // The patch performs: mem::set(0x4C3F93, 0x90, 7);
  const uint8_t *ptr = reinterpret_cast<uint8_t *>(0x004C3F93);
  bool already_patched = (*ptr == 0x90);  // Already loaded if set to a nop.
  return already_patched;
}

// The zeal.asi is loaded by the mss which happens in-between the login screen and character select.
// This is unlike most other libraries that are loaded only once.  In order to avoid
// loading / unloading with multiple patching cycles, the zeal.asi dll pins itself into memory.

// There are restrictions on what can be done safely in the dll_attach process, so Zeal creates a replace call
// hook in the loadOptions() call that happens shortly after the SoundManager loads the dll.
static const int load_options_call_addr = 0x005282f8;
static int *const ptr_load_options_call_jump_value = reinterpret_cast<int *>(load_options_call_addr + 1);
static const int load_options_call_addr_jump_value_unpatched = 0x0000e9e3;

static void __fastcall initialize_zeal(void *this_game, int unused_edx) {
  // Pin the zeal DLL into memory until the process terminates, ignoring FreeLibrary calls.
  static HMODULE hModule = nullptr;
  GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                     (LPCSTR)&initialize_zeal,  // An address within this module.
                     &hModule);

  // Do not load Zeal if a version has already been loaded. This can happen normally on
  // subsequent world logins or it can happen if the user has multiple zeal asi files.
  if (!is_zeal_already_loaded()) {
    // Create the zeal super-class that installs patches, hooks, callbacks, and things like
    // the named pipe thread.
    ZealService::create();
  }

  // Call the original loadOptions.
  reinterpret_cast<void(_fastcall *)(void *, int)>(0x00536ce0)(this_game, unused_edx);
}

static void handle_process_attach() {
  // Bail out if already patched (either a previous login cycle or another zeal asi file).
  if (*ptr_load_options_call_jump_value != load_options_call_addr_jump_value_unpatched) return;

  // Install the patch by replacing the relative jump in a call to loadOptions with a jump
  // to the initialize_zeal() function.
  const int end_of_call_addr = load_options_call_addr + 5;
  const int jump_value = reinterpret_cast<int>(&initialize_zeal) - end_of_call_addr;

  DWORD old;
  VirtualProtect((LPVOID)ptr_load_options_call_jump_value, 4, PAGE_EXECUTE_READWRITE, &old);
  *ptr_load_options_call_jump_value = jump_value;
  VirtualProtect((LPVOID)ptr_load_options_call_jump_value, 4, old, NULL);
  FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<PVOID *>(ptr_load_options_call_jump_value), 4);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
  switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
      handle_process_attach();
      break;
    case DLL_PROCESS_DETACH:
      break;  // The DLL is pinned and is not unloaded until the process terminates.
    default:
      break;  // Otherwise ignore.
  }
  return TRUE;
}
