#pragma once

#include <cstdint>

namespace Offsets {
/* clang-format off */
  // Custom functions
  constexpr uintptr_t FUN_CUSTOM_INTERACT         = 0x517405;
  // Game functions
  constexpr uintptr_t FUN_GET_OBJECT_POINTER      = 0x464870;
  constexpr uintptr_t FUN_IS_IN_WORLD             = 0xB4B424;
  constexpr uintptr_t FUN_LOAD_SCRIPT_FUNCTIONS   = 0x490250;
  constexpr uintptr_t FUN_REGISTER_LUA_FUNCTION   = 0x704120;
  constexpr uintptr_t FUN_RIGHT_CLICK_UNIT        = 0x60BEA0;
  constexpr uintptr_t FUN_RIGHT_CLICK_OBJECT      = 0x5F8660;
  constexpr uintptr_t FUN_SET_TARGET              = 0x493540;
  // Lua API functions
  constexpr uintptr_t LUA_ERROR                   = 0x6F4940;
  constexpr uintptr_t LUA_ISNUMBER                = 0x6F34D0;
  constexpr uintptr_t LUA_TONUMBER                = 0x6F3620;
  // Misc
  constexpr uintptr_t VISIBLE_OBJECTS             = 0xB41414;
  // Game object fields
  constexpr uintptr_t GO_DESCRIPTOR               = 0x8;
  constexpr uintptr_t GO_ENTRY                    = 0x294;
  constexpr uintptr_t GO_SUBTYPE                  = 0x54;
/* clang-format on */
} // namespace Offsets
