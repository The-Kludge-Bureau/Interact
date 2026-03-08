// dllmain.cpp : Defines the entry point for the DLL application.
#include "Game.h"
#include "Memory.h"
#include "MinHook.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <unordered_set>
#include <vector>

// Warsong Gulch flag objects — excluded from interaction to avoid
// accidentally triggering PvP flag grabs. Includes orphaned duplicates
// that exist in the world without attached data.
static const std::unordered_set<uint32_t> pvpFlagObjects = {
    179785,
    179786,
    179830, // Silverwing Flag (WSG)
    179831, // Warsong Flag (WSG)
};

struct GameObjectSubtypeHasher {
  size_t operator()(GameObjectSubtype s) const {
    return std::hash<uint32_t>{}(static_cast<uint32_t>(s));
  }
};

// Subtypes where right-clicking the object serves a direct purpose.
// Excludes proximity-only objects (SPELL_FOCUS), decorative objects (GENERIC),
// and objects the player cannot meaningfully interact with (CHAIR, TEXT, etc.)
static const std::unordered_set<GameObjectSubtype, GameObjectSubtypeHasher>
    interactableSubtypes = {
        GameObjectSubtype::DOOR,        GameObjectSubtype::BUTTON,
        GameObjectSubtype::QUESTGIVER,  GameObjectSubtype::CHEST,
        GameObjectSubtype::BINDER,      GameObjectSubtype::GOOBER,
        GameObjectSubtype::FISHINGNODE, GameObjectSubtype::RITUAL,
        GameObjectSubtype::MAILBOX,     GameObjectSubtype::AUCTIONHOUSE,
        GameObjectSubtype::SPELLCASTER, GameObjectSubtype::MEETINGSTONE,
        GameObjectSubtype::FLAGSTAND,   GameObjectSubtype::FISHINGHOLE,
        GameObjectSubtype::FLAGDROP,
};
static std::unordered_set<uint64_t> triedGuids;

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

  struct CandidateInfo {
    uint64_t guid;
    uintptr_t pointer; // For units and corpses, stores currentObject; for game
                       // objects, stores the GetObjectPointer result
    ObjectType type;
  };

  using DistancedCandidate = std::pair<float, CandidateInfo>;

  // Collect all candidates into per-tier vectors for sorting
  std::vector<DistancedCandidate> lootables;
  std::vector<DistancedCandidate> gameObjects;
  std::vector<DistancedCandidate> skinnables;
  std::vector<DistancedCandidate> aliveUnits;

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
    if (distance <= 5.0f) {
      if (type == ObjectType::UNIT) {
        int health = Game::GetUnitHealth(currentObject);
        if (health == 0) {
          bool isLootable = Game::IsUnitLootable(currentObject);
          bool isSkinnable = Game::IsUnitSkinnable(currentObject);

          // Priority 1 - Lootable corpse
          if (isLootable)
            lootables.push_back({distance, {guid, currentObject, type}});
          // Priority 3 - Skinnable-only corpse (not lootable)
          else if (isSkinnable)
            skinnables.push_back({distance, {guid, currentObject, type}});
        } else if (health > 0) {
          // Priority 4 - Alive unit
          aliveUnits.push_back({distance, {guid, currentObject, type}});
        }
      } else if (type == ObjectType::GAMEOBJECT) {
        uint32_t id = ReadMemory<uint32_t>(pointer + Offsets::GO_ENTRY);
        uintptr_t descriptor =
            ReadMemory<uintptr_t>(pointer + Offsets::GO_DESCRIPTOR);
        GameObjectSubtype subtype =
            ReadMemory<GameObjectSubtype>(descriptor + Offsets::GO_SUBTYPE);
        if (!pvpFlagObjects.count(id) && interactableSubtypes.count(subtype))
          // Priority 2 - Game object
          gameObjects.push_back({distance, {guid, pointer, type}});
      }
    }

    currentObject = ReadMemory<uintptr_t>(currentObject + 0x3C);
  }

  // Sort each tier by distance, nearest first
  auto byDistance = [](const DistancedCandidate &a,
                       const DistancedCandidate &b) {
    return a.first < b.first;
  };
  std::sort(lootables.begin(), lootables.end(), byDistance);
  std::sort(gameObjects.begin(), gameObjects.end(), byDistance);
  std::sort(skinnables.begin(), skinnables.end(), byDistance);
  std::sort(aliveUnits.begin(), aliveUnits.end(), byDistance);

  // Flatten into a single priority-ordered list
  std::vector<CandidateInfo> candidates;
  for (auto &[dist, info] : lootables)
    candidates.push_back(info);
  for (auto &[dist, info] : gameObjects)
    candidates.push_back(info);
  for (auto &[dist, info] : skinnables)
    candidates.push_back(info);
  for (auto &[dist, info] : aliveUnits)
    candidates.push_back(info);

  if (candidates.empty())
    return 0;

  // If none of the current candidates were previously tried, the player has
  // moved away from the last batch — reset the cycle
  bool anyOverlap = false;
  for (const auto &c : candidates) {
    if (triedGuids.count(c.guid)) {
      anyOverlap = true;
      break;
    }
  }
  if (!anyOverlap)
    triedGuids.clear();

  // Find the first candidate not yet tried this cycle
  const CandidateInfo *target = nullptr;
  for (const auto &c : candidates) {
    if (!triedGuids.count(c.guid)) {
      target = &c;
      break;
    }
  }

  // All candidates exhausted — reset and restart from the top of the list
  if (!target) {
    triedGuids.clear();
    target = &candidates.front();
  }

  triedGuids.insert(target->guid);

  int autoloot = static_cast<int>(Lua::ToNumber(L, 1));

  if (target->type == ObjectType::UNIT) {
    Game::SetTarget(target->guid);
    Game::Interact(target->pointer, autoloot, Offsets::FUN_RIGHT_CLICK_UNIT);
  } else if (target->type == ObjectType::GAMEOBJECT) {
    Game::Interact(target->pointer, autoloot, Offsets::FUN_RIGHT_CLICK_OBJECT);
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
