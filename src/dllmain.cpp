// dllmain.cpp : Defines the entry point for the DLL application.
#include "Game.h"
#include "MinHook.h"

#include <cmath>
#include <set>

std::set<int> blacklist = {179830, 179831, 179785, 179786};

typedef void(__stdcall *LoadScriptFunctions_t)();
LoadScriptFunctions_t LoadScriptFunctions_o = nullptr;

static float distance3D(C3Vector v1, C3Vector v2) {
  float dx = v2.x - v1.x;
  float dy = v2.y - v1.y;
  float dz = v2.z - v1.z;

  return sqrt(dx * dx + dy * dy + dz * dz);
}

static uint32_t InteractNearest(void *L) {
  if (!Game::IsInWorld())
    return 0;

  if (!Lua::IsNumber(L, 1)) {
    Lua::PrintError(L, "Usage: InteractNearest(autoloot)");
    return 0;
  }

  uint32_t objects = *reinterpret_cast<uint32_t *>(Offsets::VISIBLE_OBJECTS);
  uint32_t currentObject = *reinterpret_cast<uint32_t *>(objects + 0xAC);

  // 修改：为四个优先级创建独立的候选存储
  struct CandidateInfo {
    uint64_t guid;
    uint32_t
        pointer; // 对于单位和尸体，存currentObject；对于游戏物体，存GetObjectPointer结果
    uint32_t type;
  };

  CandidateInfo candidateLootable = {0, static_cast<uint32_t>(-1),
                                     ObjectType::NONE};
  CandidateInfo candidateGameObject = {0, static_cast<uint32_t>(-1),
                                       ObjectType::NONE};
  CandidateInfo candidateSkinnable = {0, static_cast<uint32_t>(-1),
                                      ObjectType::NONE};
  CandidateInfo candidateAliveUnit = {0, static_cast<uint32_t>(-1),
                                      ObjectType::NONE};

  float bestDistanceLootable = 1000.0f;
  float bestDistanceGameObject = 1000.0f;
  float bestDistanceSkinnable = 1000.0f;
  float bestDistanceAliveUnit = 1000.0f;

  uint64_t playerGUID = *reinterpret_cast<uint64_t *>(objects + 0xC0);
  uint32_t player = Game::GetObjectPointer(playerGUID);

  C3Vector pPos = Game::GetUnitPosition(player);
  C3Vector oPos;

  while (currentObject != 0 && (currentObject & 1) == 0) {
    uint64_t guid = *reinterpret_cast<uint64_t *>(currentObject + 0x30);
    uint32_t pointer = Game::GetObjectPointer(guid);
    uint32_t type = *reinterpret_cast<uint32_t *>(pointer + 0x14);

    uint64_t summonedByGUID = *reinterpret_cast<uint64_t *>(
        *reinterpret_cast<uint32_t *>(pointer + 0x8) + 0x30);
    uint32_t summonedBy = Game::GetObjectPointer(summonedByGUID);

    if (summonedByGUID != 0 && summonedBy != 0) {
      uint32_t owner = *reinterpret_cast<uint32_t *>(summonedBy + 0x14);
      if (owner == ObjectType::PLAYER) {
        currentObject = *reinterpret_cast<uint32_t *>(currentObject + 0x3C);
        continue;
      }
    }

    if (type == ObjectType::UNIT) {
      oPos = Game::GetUnitPosition(currentObject);
    } else if (type == ObjectType::GAMEOBJECT) {
      oPos = Game::GetObjectPosition(currentObject);
    } else {
      currentObject = *reinterpret_cast<uint32_t *>(currentObject + 0x3C);
      continue;
    }

    float distance = distance3D(oPos, pPos);
    if (distance <= 5.0) {
      if (type == ObjectType::UNIT) {
        bool isDead = Game::GetUnitHealth(currentObject) == 0;
        if (isDead) {
          bool isLootable = Game::IsUnitLootable(currentObject);
          bool isSkinnable = Game::IsUnitSkinnable(currentObject);

          // 修改：第一优先级 - 可拾取的尸体
          if (isLootable && distance < bestDistanceLootable) {
            bestDistanceLootable = distance;
            candidateLootable = {guid, currentObject, type};
          }
          // 修改：第三优先级 - 仅可剥皮但不可拾取的尸体
          else if (!isLootable && isSkinnable &&
                   distance < bestDistanceSkinnable) {
            bestDistanceSkinnable = distance;
            candidateSkinnable = {guid, currentObject, type};
          }
        } else if (Game::GetUnitHealth(currentObject) > 0 &&
                   distance < bestDistanceAliveUnit) {
          // 修改：第四优先级 - 活着的单位
          bestDistanceAliveUnit = distance;
          candidateAliveUnit = {guid, currentObject, type};
        }
      } else if (type == ObjectType::GAMEOBJECT) {
        uint32_t id = *reinterpret_cast<uint32_t *>(pointer + 0x294);
        if (!blacklist.count(id) && distance < bestDistanceGameObject) {
          // 修改：第二优先级 - 游戏物体
          bestDistanceGameObject = distance;
          candidateGameObject = {guid, pointer, type};
        }
      }
    }

    currentObject = *reinterpret_cast<uint32_t *>(currentObject + 0x3C);
  }

  // 修改：按优先级顺序选择最终候选
  CandidateInfo finalCandidate = {0, static_cast<uint32_t>(-1),
                                  ObjectType::NONE};
  if (candidateLootable.pointer != static_cast<uint32_t>(-1)) {
    finalCandidate = candidateLootable;
  } else if (candidateGameObject.pointer != static_cast<uint32_t>(-1)) {
    finalCandidate = candidateGameObject;
  } else if (candidateSkinnable.pointer != static_cast<uint32_t>(-1)) {
    finalCandidate = candidateSkinnable;
  } else if (candidateAliveUnit.pointer != static_cast<uint32_t>(-1)) {
    finalCandidate = candidateAliveUnit;
  }

  if (finalCandidate.pointer == static_cast<uint32_t>(-1))
    return 0;

  int autoloot = Lua::ToNumber(L, 1);

  if (finalCandidate.type == ObjectType::UNIT) {
    Game::SetTarget(finalCandidate.guid);
    Game::Interact(finalCandidate.pointer, autoloot,
                   Offsets::FUN_RIGHT_CLICK_UNIT);
  } else if (finalCandidate.type == ObjectType::GAMEOBJECT) {
    Game::Interact(finalCandidate.pointer, autoloot,
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
