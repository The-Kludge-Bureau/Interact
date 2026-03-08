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

// Subtype of a GAMEOBJECT — distinct from ObjectType.
// Describes what kind of game object it is and how it behaves.
enum class GameObjectSubtype : uint32_t {
  /* clang-format off */
  DOOR          = 0,
  BUTTON        = 1,
  QUESTGIVER    = 2,
  CHEST         = 3,  // includes mining nodes, herb nodes, lockboxes
  BINDER        = 4,
  GENERIC       = 5,  // decorative, no interaction
  TRAP          = 6,
  CHAIR         = 7,
  SPELL_FOCUS   = 8,  // forge, anvil, cooking fire — proximity only, not clicked
  TEXT          = 9,
  GOOBER        = 10, // misc interactables: quest objects, clickable world items
  TRANSPORT     = 11,
  AREADAMAGE    = 12,
  CAMERA        = 13,
  MAP_OBJECT    = 14,
  MO_TRANSPORT  = 15,
  DUEL_ARBITER  = 16,
  FISHINGNODE   = 17,
  RITUAL        = 18, // summoning stones
  MAILBOX       = 19,
  AUCTIONHOUSE  = 20,
  GUARDPOST     = 21,
  SPELLCASTER   = 22,
  MEETINGSTONE  = 23,
  FLAGSTAND     = 24,
  FISHINGHOLE   = 25,
  FLAGDROP      = 26,
  /* clang-format on */
};

/* clang-format off */
using FUN_ONRIGHTCLICK             = void(__thiscall *)(uintptr_t pointer, int autoloot);
using FrameScript_RegisterFunction = void(__fastcall *)(const char *, uintptr_t);
using lua_error                    = void(__cdecl *)(void *, const char *);
using lua_isnumber                 = bool(__fastcall *)(void *, int);
using lua_tonumber                 = double(__fastcall *)(void *, int);
/* clang-format on */

// WoW stores positions in memory as X, Y, Z. Field order matches this layout.
struct C3Vector {
  float x;
  float y;
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
