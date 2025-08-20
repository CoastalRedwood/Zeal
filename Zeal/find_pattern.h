#pragma once
#include <Windows.h>

namespace Zeal {
namespace Memory {
BOOLEAN MaskCompare(PVOID buffer, LPCSTR pattern, LPCSTR mask);
DWORD FindPattern(LPCSTR pattern, LPCSTR mask, HMODULE target_module);
}  // namespace Memory
}  // namespace Zeal