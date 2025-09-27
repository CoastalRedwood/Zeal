#pragma once
#include <Windows.h>

#include <memory>
#include <string>
#include <unordered_map>

#define czVOID(c) (void)c

// Types of supported hooks. All hooks store the original bytes for restoration at deletion and their
// original() methods execute the original code (stored in the trampoline).
enum hook_type_ {
  hook_type_replace_call,  // Replace an 0xe8 relative call or 0xe9 relative jump (middle of function).
  hook_type_detour,        // More general insertion of a detour hook at start of function.
  hook_type_vtable,        // Replace a vtable int address entry.
};

class hook {
 public:
  hook() = delete;              // No simple implicit construction.
  hook(const hook &) = delete;  // Copies not allowed.
  hook(hook &&) = delete;       // Moves not allowed.

  ~hook();  // Copies back original code.

  template <typename X, typename T>
  hook(X patch_address_, T replacement_function_ptr, hook_type_ hooktype = hook_type_detour)
      : patch_address(patch_address_), replacement_callee_addr((int)replacement_function_ptr), hook_type(hooktype) {
    switch (hook_type) {
      case hook_type_detour: {
        detour();
        break;
      }
      case hook_type_replace_call: {
        replace_call();
        break;
      }
      case hook_type_vtable: {
        replace_vtable();
        break;
      }
      default:
        fatal_error();
        break;
    }
  }

  template <typename T>
  T original(T fnType) {
    czVOID(fnType);
    return (T)trampoline;
  }

 private:
  void detour();
  void replace_call();
  void replace_vtable();
  void fatal_error();
  void patch_trampoline_relative_jumps();

  const int patch_address;                            // Address to patch to jump to the callee.
  const int replacement_callee_addr;                  // Address of the replacement function.
  const hook_type_ hook_type;                         // Type of replacement.
  int orig_byte_count;                                // Number of original patch bytes modified.
  BYTE original_bytes[11];                            // 5 byte jump with 6 more bytes for opcode boundary.
  BYTE trampoline_bytes[sizeof(original_bytes) + 5];  // Additional space for an 0xe9 jump opcode at end.
  int trampoline = reinterpret_cast<int>(&trampoline_bytes[0]);
};

class HookWrapper {
 public:
  std::unordered_map<std::string, std::unique_ptr<hook>> hook_map;

  template <typename X, typename T>
  void Add(std::string name, X addr, T fnc, hook_type_ type) {
    hook_map[name] = std::make_unique<hook>(addr, fnc, type);
  }

  void Remove(std::string name) { hook_map.erase(name); }
};
