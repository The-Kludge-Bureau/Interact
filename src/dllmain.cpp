// dllmain.cpp : Defines the entry point for the DLL application.
#include "Game.h"
#include "Memory.h"
#include "MinHook.h"

#include <cmath>
#include <optional>
#include <unordered_set>

static const std::unordered_set<uint32_t> blacklist = {179830, 179831, 179785,
                                                       179786};

typedef void(__stdcall *LoadScriptFunctions_t)();
LoadScriptFunctions_t LoadScriptFunctions_o = nullptr;

static float distance3D(const C3Vector &v1, const C3Vector &v2) {
  float dx = v2.x - v1.x;
  float dy = v2.y - v1.y;
  float dz = v2.z - v1.z;

  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

static uint32_t InteractNearest(void *L) {
  if (!Game::IsInWorld())
    return 0;

  if (!Lua::IsNumber(L, 1)) {
    Lua::PrintError(L, "Usage: InteractNearest(autoloot)");
    return 0;
  }

  uintptr_t objects = ReadMemory<uintptr_t>(Offsets::VISIBLE_OBJECTS);
  uintptr_t currentObject = ReadMemory<uintptr_t>(objects + 0xAC);

  // Create separate candidate storage for each of the four priority tiers
  struct CandidateInfo {
    uint64_t guid;
    uintptr_t pointer; // For units and corpses, stores currentObject; for game
                       // objects, stores the GetObjectPointer result
    ObjectType type;
  };

  std::optional<CandidateInfo> candidateLootable;
  std::optional<CandidateInfo> candidateGameObject;
  std::optional<CandidateInfo> candidateSkinnable;
  std::optional<CandidateInfo> candidateAliveUnit;

  float bestDistanceLootable = 1000.0f;
  float bestDistanceGameObject = 1000.0f;
  float bestDistanceSkinnable = 1000.0f;
  float bestDistanceAliveUnit = 1000.0f;

  uint64_t playerGUID = ReadMemory<uint64_t>(objects + 0xC0);
  uintptr_t player = Game::GetObjectPointer(playerGUID);

  C3Vector pPos = Game::GetUnitPosition(player);
  C3Vector oPos;

  while (currentObject != 0 && (currentObject & 1) == 0) {
    uint64_t guid = ReadMemory<uint64_t>(currentObject + 0x30);
    uintptr_t pointer = Game::GetObjectPointer(guid);
    ObjectType type = ReadMemory<ObjectType>(pointer + 0x14);

    uint64_t summonedByGUID =
        ReadMemory<uint64_t>(ReadMemory<uintptr_t>(pointer + 0x8) + 0x30);
    uintptr_t summonedBy = Game::GetObjectPointer(summonedByGUID);

    if (summonedByGUID != 0 && summonedBy != 0) {
      ObjectType owner = ReadMemory<ObjectType>(summonedBy + 0x14);
      if (owner == ObjectType::PLAYER) {
        currentObject = ReadMemory<uintptr_t>(currentObject + 0x3C);
        continue;
      }
    }

    if (type == ObjectType::UNIT) {
      oPos = Game::GetUnitPosition(currentObject);
    } else if (type == ObjectType::GAMEOBJECT) {
      oPos = Game::GetObjectPosition(currentObject);
    } else {
      currentObject = ReadMemory<uintptr_t>(currentObject + 0x3C);
      continue;
    }

    float distance = distance3D(oPos, pPos);
    if (distance <= 5.0) {
      if (type == ObjectType::UNIT) {
        bool isDead = Game::GetUnitHealth(currentObject) == 0;
        if (isDead) {
          bool isLootable = Game::IsUnitLootable(currentObject);
          bool isSkinnable = Game::IsUnitSkinnable(currentObject);

          // Priority 1 - Lootable corpse
          if (isLootable && distance < bestDistanceLootable) {
            bestDistanceLootable = distance;
            candidateLootable = {guid, currentObject, type};
          }
          // Priority 3 - Skinnable-only corpse (not lootable)
          else if (!isLootable && isSkinnable &&
                   distance < bestDistanceSkinnable) {
            bestDistanceSkinnable = distance;
            candidateSkinnable = {guid, currentObject, type};
          }
        } else if (Game::GetUnitHealth(currentObject) > 0 &&
                   distance < bestDistanceAliveUnit) {
          // Priority 4 - Alive unit
          bestDistanceAliveUnit = distance;
          candidateAliveUnit = {guid, currentObject, type};
        }
      } else if (type == ObjectType::GAMEOBJECT) {
        uint32_t id = ReadMemory<uint32_t>(pointer + 0x294);
        if (!blacklist.count(id) && distance < bestDistanceGameObject) {
          // Priority 2 - Game object
          bestDistanceGameObject = distance;
          candidateGameObject = {guid, pointer, type};
        }
      }
    }

    currentObject = ReadMemory<uintptr_t>(currentObject + 0x3C);
  }

  // Select the final candidate in priority order
  const std::optional<CandidateInfo> finalCandidate =
      candidateLootable     ? candidateLootable
      : candidateGameObject ? candidateGameObject
      : candidateSkinnable  ? candidateSkinnable
                            : candidateAliveUnit;

  if (!finalCandidate)
    return 0;

  int autoloot = Lua::ToNumber(L, 1);

  if (finalCandidate->type == ObjectType::UNIT) {
    Game::SetTarget(finalCandidate->guid);
    Game::Interact(finalCandidate->pointer, autoloot,
                   Offsets::FUN_RIGHT_CLICK_UNIT);
  } else if (finalCandidate->type == ObjectType::GAMEOBJECT) {
    Game::Interact(finalCandidate->pointer, autoloot,
                   Offsets::FUN_RIGHT_CLICK_OBJECT);
  }

  return 1;
}

void __stdcall LoadScriptFunctions_h() {
  DWORD oldProtect;
  BYTE trampoline[] = {
      0xB8, 0,   0, 0, 0, // mov eax, &InteractNearest
      0xFF, 0xE0          // jmp eax
  };
  DWORD addr = (DWORD)&InteractNearest;
  memcpy(&trampoline[1], &addr, 4);
  VirtualProtect((LPVOID)Offsets::FUN_CUSTOM_INTERACT, sizeof(trampoline),
                 PAGE_EXECUTE_READWRITE, &oldProtect);
  memcpy((void *)Offsets::FUN_CUSTOM_INTERACT, trampoline, sizeof(trampoline));
  VirtualProtect((LPVOID)Offsets::FUN_CUSTOM_INTERACT, sizeof(trampoline),
                 oldProtect, &oldProtect);

  Game::RegisterFunction("InteractNearest", Offsets::FUN_CUSTOM_INTERACT);

  LoadScriptFunctions_o();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call,
                      LPVOID lpReserved) {
  switch (ul_reason_for_call) {
  case DLL_PROCESS_ATTACH:
    DisableThreadLibraryCalls(hModule);

    if (MH_Initialize() != MH_OK)
      return FALSE;

    if (MH_CreateHook(
            (LPVOID)Offsets::FUN_LOAD_SCRIPT_FUNCTIONS, &LoadScriptFunctions_h,
            reinterpret_cast<LPVOID *>(&LoadScriptFunctions_o)) != MH_OK)
      return FALSE;

    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK)
      return FALSE;

    break;

  case DLL_PROCESS_DETACH:
    MH_Uninitialize();
    break;
  }

  return TRUE;
}
