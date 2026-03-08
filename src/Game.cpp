#include "Game.h"

namespace Game {
uint32_t __stdcall GetObjectPointer(uint64_t guid) {
  typedef uint32_t __fastcall func(uint64_t guid);
  func *function = (func *)Offsets::FUN_GET_OBJECT_POINTER;

  return function(guid);
}

C3Vector GetObjectPosition(uint32_t pointer) {
  pointer = ReadMemory<uint32_t>(pointer + 0x110);
  return {
      ReadMemory<float>(pointer + 0x24),
      ReadMemory<float>(pointer + 0x28),
      ReadMemory<float>(pointer + 0x2C),
  };
}

int GetUnitHealth(uint32_t unit) {
  return ReadMemory<int>(ReadMemory<uint32_t>(unit + 0x8) + 0x58);
}

C3Vector GetUnitPosition(uint32_t unit) {
  return {
      ReadMemory<float>(unit + 0x09B8),
      ReadMemory<float>(unit + 0x09BC),
      ReadMemory<float>(unit + 0x09C0),
  };
}

void Interact(uint32_t pointer, int autoloot, uintptr_t fun_ptr) {
  FUN_ONRIGHTCLICK function = reinterpret_cast<FUN_ONRIGHTCLICK>(fun_ptr);
  function(pointer, autoloot);
}

bool IsUnitLootable(uint32_t unit) {
  int flags = ReadMemory<int>(ReadMemory<uint32_t>(unit + 0x8) + 0x23C);
  return (flags & 1) != 0;
}

bool IsUnitSkinnable(uint32_t unit) {
  int flags = ReadMemory<int>(ReadMemory<uint32_t>(unit + 0x8) + 0xB8);
  int flag = 0x4000000;
  return (flags & flag) == flag;
}

void SetTarget(uint64_t guid) {
  typedef void __stdcall func(uint64_t guid);
  func *function = (func *)Offsets::FUN_SET_TARGET;

  function(guid);
}
} // namespace Game
