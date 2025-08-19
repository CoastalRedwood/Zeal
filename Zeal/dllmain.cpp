#include <Windows.h>

#include "zeal.h"

static bool check_if_already_loaded() {
  // Simple hack check is to see if client sided mana ticking was disabled (since before 0.3.0).
  // This only works if this newer Zeal is loaded second, but a 50% detection is better than none.
  //     mem::set(0x4C3F93, 0x90, 7);
  const uint8_t *ptr = reinterpret_cast<uint8_t *>(0x004C3F93);
  bool already_patched = (*ptr == 0x90);                             // Already loaded if set to a nop.
  return already_patched || ZealService::get_instance() != nullptr;  // Also should be a singleton.
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
  static std::unique_ptr<ZealService> zeal = nullptr;

  switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH: {
      if (check_if_already_loaded())
        MessageBoxA(NULL,
                    "Error: An extra zeal .asi file is loading. This is bad and could crash. Remove extra .asi files "
                    "from eq root directory.",
                    "Zeal installation error", MB_OK | MB_ICONERROR);
      else {
        DisableThreadLibraryCalls(hModule);
        // Experimental use of critical section to try and reduce HeapValidate() failures.
        // Copy critical section protection used in ProcessMbox() and DoMainLoop().
        int critical_section = *(int *)0x007914b8;
        if (critical_section) EnterCriticalSection((LPCRITICAL_SECTION)(0x007914b8));
        zeal = std::make_unique<ZealService>();  // Construct before game thread is started.
        if (critical_section) LeaveCriticalSection((LPCRITICAL_SECTION)(0x007914b8));
      }
      break;
    }
    case DLL_PROCESS_DETACH:
      // Only perform cleanup if the process is not ending.
      if (lpReserved == nullptr) {
        zeal.reset();  // Release zeal which will invoke the destructors.
      }
      break;
    default:
      break;  // Otherwise ignore.
  }
  return TRUE;
}
