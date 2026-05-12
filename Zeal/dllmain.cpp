#include <Windows.h>

#include <cstdint>

#include "zeal.h"

// Layer 0 addresses ported from 2002 client to RoF2 (Session 9 Ghidra work,
// see memory/project_zeal_rof2_addresses.md). The pin + patch + chain pattern
// is unchanged from upstream Zeal.
//
// CRITICAL: RoF2's eqgame.exe has DYNAMIC_BASE (ASLR) set in its PE header
// (the 2002 client did not). Hardcoded Ghidra addresses are relative to the
// PE preferred base 0x00400000 and must be ADJUSTED by the runtime ASLR delta
// before dereferencing: runtime = ghidra + (runtime_base - 0x00400000). The
// delta is computed once in handle_process_attach() and stored in aslr_delta.
// Our own DLL's function pointers (e.g. &initialize_zeal) are already runtime-
// correct via the linker, so no adjustment is needed for them.

static const uintptr_t EQGAME_PREFERRED_BASE = 0x00400000;
static uintptr_t aslr_delta = 0;  // Populated in handle_process_attach().

// Addresses below are Ghidra's static view (preferred base 0x00400000).
// Add aslr_delta before dereferencing.
static const uintptr_t load_options_call_addr_ghidra = 0x0053621a;
static const uintptr_t load_options_func_addr_ghidra = 0x00544be0;
static const int load_options_call_jump_value_unpatched = 0x0000e9c1;

// Set to 1 during Layer 0 verification: prove the patched CALL reaches
// initialize_zeal() without booting any submodule that still uses 2002-client
// addresses from game_addresses.h. Flip to 0 in R3 when modules come online.
#define ZEAL_ROF2_LAYER0_VALIDATION 1

// Set to 1 to pop a DllMain-time MessageBox showing runtime base, ASLR delta,
// and the byte values at the (adjusted) CALL site. Triage tool — flip off in R3.
#define ZEAL_ROF2_LAYER0_DIAGNOSE 1

static void __fastcall initialize_zeal(void *this_game, int unused_edx) {
  // Pin the DLL: Miles otherwise unloads/reloads on each char-select transition.
  static HMODULE hModule = nullptr;
  GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                     (LPCSTR)&initialize_zeal, &hModule);

  // Skip re-init on subsequent world logins (each refires the patched CALL).
  // ZealService::create() is internally singleton-guarded as a backstop.
  static bool zeal_service_created = false;
  if (!zeal_service_created) {
#if ZEAL_ROF2_LAYER0_VALIDATION
    MessageBoxA(NULL,
                "Patched CALL reached initialize_zeal() successfully.\n"
                "ZealService::create() is gated off for Layer 0 validation.\n"
                "Chaining to original loadOptions next.",
                "Zeal-RoF2 Layer 0 [2/2] initialize_zeal",
                MB_OK | MB_ICONINFORMATION);
#else
    ZealService::create();
#endif
    zeal_service_created = true;
  }

  // Chain to the original loadOptions so the client gets its INI loaded.
  const uintptr_t load_options_runtime = load_options_func_addr_ghidra + aslr_delta;
  reinterpret_cast<void(__fastcall *)(void *, int)>(load_options_runtime)(this_game, unused_edx);
}

static void handle_process_attach() {
  // GetModuleHandle(NULL) returns the main exe's HMODULE, which equals its
  // runtime base address. Compute ASLR delta from that.
  const HMODULE eqgame_base = GetModuleHandleA(NULL);
  aslr_delta = reinterpret_cast<uintptr_t>(eqgame_base) - EQGAME_PREFERRED_BASE;

  const uintptr_t call_site = load_options_call_addr_ghidra + aslr_delta;
  int *const ptr_jump_value = reinterpret_cast<int *>(call_site + 1);

  const uint8_t opcode_byte = *reinterpret_cast<uint8_t *>(call_site);
  const int current_displacement = *ptr_jump_value;

  const char *decision_str;
  bool install = false;
  if (opcode_byte != 0xe8) {
    decision_str = "SKIP - opcode at CALL site is not 0xe8 (wrong address?)";
  } else if (current_displacement != load_options_call_jump_value_unpatched) {
    decision_str = "SKIP - displacement mismatch (already patched OR wrong address)";
  } else {
    decision_str = "INSTALL - signature matches, will patch";
    install = true;
  }

#if ZEAL_ROF2_LAYER0_DIAGNOSE
  char module_path[MAX_PATH] = {0};
  HMODULE self = nullptr;
  GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                     (LPCSTR)&handle_process_attach, &self);
  GetModuleFileNameA(self, module_path, sizeof(module_path));

  char msg[1024];
  wsprintfA(msg,
            "Module loaded: %s\r\n"
            "\r\n"
            "eqgame.exe runtime base: 0x%08x (preferred: 0x%08x)\r\n"
            "ASLR delta: 0x%08x\r\n"
            "\r\n"
            "CALL site (ghidra 0x%08x -> runtime 0x%08x):\r\n"
            "  Opcode byte: 0x%02x (expected 0xe8 = near CALL)\r\n"
            "  Displacement: 0x%08x (expected unpatched: 0x%08x)\r\n"
            "\r\n"
            "Decision: %s",
            module_path,
            (uintptr_t)eqgame_base,
            EQGAME_PREFERRED_BASE,
            aslr_delta,
            load_options_call_addr_ghidra,
            call_site,
            opcode_byte,
            current_displacement,
            load_options_call_jump_value_unpatched,
            decision_str);
  MessageBoxA(NULL, msg, "Zeal-RoF2 Layer 0 [1/2] DllMain", MB_OK | MB_ICONINFORMATION);
#endif

  if (!install) return;

  const uintptr_t end_of_call_addr = call_site + 5;
  const int jump_value = reinterpret_cast<int>(&initialize_zeal) - static_cast<int>(end_of_call_addr);

  DWORD old;
  VirtualProtect((LPVOID)ptr_jump_value, 4, PAGE_EXECUTE_READWRITE, &old);
  *ptr_jump_value = jump_value;
  VirtualProtect((LPVOID)ptr_jump_value, 4, old, &old);
  FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<PVOID *>(ptr_jump_value), 4);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
  switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
      handle_process_attach();
      break;
    case DLL_PROCESS_DETACH:
      break;  // DLL is pinned; this never fires until process exit.
    default:
      break;
  }
  return TRUE;
}
