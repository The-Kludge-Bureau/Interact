#pragma once

#include "Memory.h"
#include "Offsets.h"

#include <cstdint>

enum class ObjectType : uint32_t {
  NONE,
  ITEM,
  CONTAINER,
  UNIT,
  PLAYER,
  GAMEOBJECT,
  DYNAMICOBJECT,
  CORPSE
};

/* clang-format off */
using FUN_ONRIGHTCLICK             = void(__thiscall *)(uintptr_t pointer, int autoloot);
using FrameScript_RegisterFunction = void(__fastcall *)(const char *, uintptr_t);
using lua_error                    = void(__cdecl *)(void *, const char *);
using lua_isnumber                 = bool(__fastcall *)(void *, int);
using lua_tonumber                 = double(__fastcall *)(void *, int);
/* clang-format on */

struct C3Vector {
  float y;
  float x;
  float z;
};

namespace Game {
auto const RegisterFunction =
    (FrameScript_RegisterFunction)Offsets::FUN_REGISTER_LUA_FUNCTION;

uintptr_t __stdcall GetObjectPointer(uint64_t guid);

C3Vector GetObjectPosition(uintptr_t pointer);

int GetUnitHealth(uintptr_t unit);

C3Vector GetUnitPosition(uintptr_t unit);

void Interact(uintptr_t pointer, int autoloot, uintptr_t fun_ptr);

bool IsUnitLootable(uintptr_t unit);
bool IsUnitSkinnable(uintptr_t unit);

void SetTarget(uint64_t guid);

inline bool IsInWorld() { return ReadMemory<char>(Offsets::FUN_IS_IN_WORLD); }
} // namespace Game

namespace Lua {
auto const PrintError = (lua_error)Offsets::LUA_ERROR;
auto const IsNumber = (lua_isnumber)Offsets::LUA_ISNUMBER;
auto const ToNumber = (lua_tonumber)Offsets::LUA_TONUMBER;
} // namespace Lua
