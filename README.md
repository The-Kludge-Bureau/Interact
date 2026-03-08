# Interact

A DLL mod for vanilla World of Warcraft (1.12.1) that adds a smart interact keybind — loot corpses, open chests, click mailboxes, and target NPCs without having to click on them.

## Features

- **Priority-based targeting** — nearby objects are evaluated in order: lootable corpses → interactable objects → skinnable corpses → alive units
- **Cycles through multiple targets** — in a raid or party with multiple corpses, repeated keypresses walk through all lootable targets rather than hammering the same one
- **Resets on movement** — the cycle clears automatically when you move away from the current batch of objects
- **Smart object filtering** — ignores decorative world objects (forges, cauldrons, cooking fires) that can't be meaningfully interacted with

## Requirements

- [VanillaFixes](https://github.com/hannesmann/vanillafixes) — required to load custom DLLs into the client

## Installation

1. Download the [latest release](https://github.com/The-Kludge-Bureau/Interact/releases/latest/download/Interact.zip) and extract it into your World of Warcraft folder
2. Open `dlls.txt` in your WoW folder and add `Interact.dll` on a new line at the end
3. Launch the game via `VanillaFixes.exe`
4. Open the keybindings menu and bind a key to **Interact** or **Interact (auto-loot)**

> [!NOTE]
> If your launcher has built-in mod support, it will handle steps 2–3 for you.

## Building

Requires MSVC, CMake, and [vcpkg](https://github.com/microsoft/vcpkg).

```sh
cmake --preset x86-release
cmake --build build
```

The output DLL will be in `build/`.
