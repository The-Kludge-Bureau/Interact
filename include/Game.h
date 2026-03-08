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

typedef void(__thiscall *FUN_ONRIGHTCLICK)(uintptr_t pointer, int autoloot);
typedef void(__fastcall *FrameScript_RegisterFunction)(const char *, uintptr_t);
typedef void(__cdecl *lua_error)(void *, const char *);
typedef bool(__fastcall *lua_isnumber)(void *, int);
typedef double(__fastcall *lua_tonumber)(void *, int);

typedef struct {
  float y;
  float x;
  float z;
} C3Vector;

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
