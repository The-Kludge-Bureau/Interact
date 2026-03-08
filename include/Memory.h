#pragma once

#include <cstdint>

template <typename T> inline T ReadMemory(uintptr_t addr) {
  return *reinterpret_cast<T *>(addr);
}
