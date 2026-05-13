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

// Set to 1 to keep ZealService::create() gated off. ZealService brings up the
// full Zeal submodule tree (camera_mods, chatfilter, nameplate, etc.) and each
// submodule still references 2002-client addresses via game_addresses.h. Flip
// to 0 only after enough submodules have been RoF2-ported that ZealService
// can safely come up.
#define ZEAL_ROF2_LAYER0_VALIDATION 1

// Set to 1 to pop the two Layer 0 diagnostic MessageBoxes ([1/2] DllMain
// showing runtime base + ASLR delta + signature decision, [2/2] in
// initialize_zeal confirming the patched CALL reached us). On for active
// dev; off for production / friend-ready builds.
#define ZEAL_ROF2_LAYER0_DIAGNOSE 0

// R3 Stage 1: H/V mouse-sensitivity parity patch. Inside FUN_00516d40 (RoF2
// procMouse-equivalent), the Y-axis FMUL at 0x00516ff6 multiplies by 256.0
// while the X-axis FMUL elsewhere multiplies by 512.0 — a hardcoded 2:1
// disparity that's the real source of EQ's "vertical mouse feels slower than
// horizontal." The 256.0 literal at DAT_009c7328 is shared by 19 other
// functions in the binary (color/buffer/RTTI uses), so we can't patch the
// literal itself. Instead we redirect this one FMUL's 4-byte displacement
// field to point at our own 512.0 constant (k_r3_y_scale, below) which lives
// in this DLL's .rdata. Single 4-byte write per process; the in-game
// MouseSensitivity slider, the 8-bucket formula, and every other consumer of
// DAT_009c7328 stay untouched. See memory/feedback_player_benefit_first.md.
#define ZEAL_ROF2_R3_HV_PARITY 1

// Our replacement constant. The linker places this in Zeal.asi's .rdata and
// gives it a stable runtime address; the DLL is pinned for the process
// lifetime (initialize_zeal calls GetModuleHandleExA with FLAG_PIN), so the
// address is valid forever after handle_process_attach() runs.
static const float k_r3_y_scale = 512.0f;

// Y-axis FMUL instruction inside FUN_00516d40. Expected bytes:
//   D8 0D 28 73 9c 00   FMUL dword ptr [DAT_009c7328]
// The 4-byte displacement to patch lives at offset +2 from the opcode.
// At runtime the displacement bytes are RELOCATED by the PE loader to
// reflect ASLR — so the unpatched runtime value equals (ghidra + aslr_delta).
static const uintptr_t r3_y_fmul_addr_ghidra = 0x00516ff6;
static const uint8_t r3_y_fmul_opcode_byte_0 = 0xD8;  // FMUL m32
static const uint8_t r3_y_fmul_opcode_byte_1 = 0x0D;  // mod=00, reg=001, r/m=101 (disp32)
static const uintptr_t r3_y_fmul_disp_unpatched_ghidra = 0x009c7328;  // DAT_009c7328

static void __fastcall initialize_zeal(void *this_game, int unused_edx) {
  // Pin the DLL: Miles otherwise unloads/reloads on each char-select transition.
  static HMODULE hModule = nullptr;
  GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                     (LPCSTR)&initialize_zeal, &hModule);

  // Skip re-init on subsequent world logins (each refires the patched CALL).
  // ZealService::create() is internally singleton-guarded as a backstop.
  static bool zeal_service_created = false;
  if (!zeal_service_created) {
#if !ZEAL_ROF2_LAYER0_VALIDATION
    ZealService::create();
#endif
#if ZEAL_ROF2_LAYER0_DIAGNOSE
    MessageBoxA(NULL,
                "Patched CALL reached initialize_zeal() successfully.\n"
                "Chaining to original loadOptions next.",
                "Zeal-RoF2 Layer 0 [2/2] initialize_zeal",
                MB_OK | MB_ICONINFORMATION);
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

#if ZEAL_ROF2_R3_HV_PARITY
  // R3 Stage 1: redirect FUN_00516d40's Y-axis FMUL from [DAT_009c7328] (256.0,
  // shared) to [&k_r3_y_scale] (our 512.0). Single 4-byte write to the
  // instruction's displacement field at runtime address (0x00516ff8 + delta).
  {
    const uintptr_t y_fmul_runtime = r3_y_fmul_addr_ghidra + aslr_delta;
    const uint8_t *y_op = reinterpret_cast<const uint8_t *>(y_fmul_runtime);
    uint32_t *y_disp = reinterpret_cast<uint32_t *>(y_fmul_runtime + 2);

    // The instruction's displacement is an absolute address, so the PE loader
    // applies the ASLR delta to it during relocation. Compare against the
    // relocated value, not the static Ghidra view.
    const uint32_t y_disp_expected =
        static_cast<uint32_t>(r3_y_fmul_disp_unpatched_ghidra + aslr_delta);

    if (y_op[0] == r3_y_fmul_opcode_byte_0 && y_op[1] == r3_y_fmul_opcode_byte_1 &&
        *y_disp == y_disp_expected) {
      const uint32_t new_disp =
          static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&k_r3_y_scale));
      DWORD r3_old;
      VirtualProtect(y_disp, 4, PAGE_EXECUTE_READWRITE, &r3_old);
      *y_disp = new_disp;
      VirtualProtect(y_disp, 4, r3_old, &r3_old);
      FlushInstructionCache(GetCurrentProcess(), y_disp, 4);
    }
    // Signature mismatch: silently skip. Y axis stays vanilla 2:1. Better
    // than risking a patch on an unexpected eqgame.exe build.
  }
#endif
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
