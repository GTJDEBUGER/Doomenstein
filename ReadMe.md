# Doomenstein

A classic Doom-inspired FPS featuring an epic, multi-stage boss fight against a giant demon dragon on an oceanic altar. Master shifting mechanics to defeat the beast, then unwind with a quirky fishing minigame finale.

> **Gameplay Showcase:** Watch the game in action [here](https://gaotianji.site/#portfolio/project16).

## Screenshot

<table style="width:100%">
  <tr>
    <td width="50%"><img src="https://github.com/user-attachments/assets/e628f80e-e177-471c-bd24-bc5a081c71fd" alt="Gameplay Screenshot 1" width="100%"></td>
    <td width="50%"><img src="https://github.com/user-attachments/assets/7eda7bdc-c186-4cfb-89a8-97708669bb60" alt="Gameplay Screenshot 2" width="100%"></td>
  </tr>
  <tr>
    <td width="50%"><img src="https://github.com/user-attachments/assets/58c8635f-909a-40e1-864e-f2bfc198ff47" alt="Gameplay Screenshot 3" width="100%"></td>
    <td width="50%"><img src="https://github.com/user-attachments/assets/9f2874ca-9581-41b7-889f-cda044de5e13" alt="Gameplay Screenshot 4" width="100%"></td>
  </tr>
</table>

## How to Play？

### Controls (Keyboard / Gamepad)

* <kbd>SPACE</kbd> / <kbd>START</kbd> — Join player in attract mode or start game in lobby mode
* <kbd>ESC</kbd> / <kbd>BACK</kbd> — Switch from game mode to attract mode, or exit application
* <kbd>Mouse</kbd> / <kbd>Right Stick</kbd> — Control player view / camera
* <kbd>Left Click</kbd> / <kbd>RT</kbd> — Fire currently possessed actor's equipped weapon
* <kbd>A</kbd> <kbd>D</kbd> / <kbd>Left Stick X</kbd> — Move left/right (relative to player orientation)
* <kbd>W</kbd> <kbd>S</kbd> / <kbd>Right Stick Y</kbd> — Move forward/backward (relative to player orientation)
* <kbd>SHIFT</kbd> / <kbd>LT</kbd> — Increase movement speed (hold to sprint)
* <kbd>SPACE</kbd> / <kbd>A</kbd> — Jump
* <kbd>1</kbd> / <kbd>D-Pad Up</kbd> — Switch to Weapon 1
* <kbd>2</kbd> / <kbd>D-Pad Down</kbd> — Switch to Weapon 2
* <kbd>3</kbd> / <kbd>D-Pad Left</kbd> — Switch to Weapon 3 (if available)
* <kbd>4</kbd> / <kbd>D-Pad Right</kbd> — Switch to Weapon 4 (if available)

### Debug & System Controls (PC Only)

* <kbd>F</kbd> — Toggle free camera mode (Single-player only)
* <kbd>N</kbd> — Possess next valid actor in map (Single-player only)
* <kbd>P</kbd> — Pause the game
* <kbd>O</kbd> — Step forward one frame
* <kbd>T</kbd> — Slow down game time (while held)
* <kbd>H</kbd> — Speed up game time (while held)
* <kbd>F1</kbd> — Toggle debug drawing (includes FPS counter)

## Build Instructions

> **Important Dependency Note:**
> This game is built on my [Custom Game Engine](https://github.com/GTJDEBUGER/GameEngine). You must pull the engine project first to build this game. 
> 
> Additionally, due to FMOD audio dependencies, **you must manually place `fmod64.dll` into the `Run/` directory after building**, otherwise the executable will throw a missing DLL error.

**Prerequisites:**
* Visual Studio 2022
* FMOD DLL

**Compilation Steps:**
1. Clone the [Game Engine](https://github.com/GTJDEBUGER/GameEngine) repository.
2. Ensure both the engine project and the Starship game project are located in the same parent directory.
3. Open the game project solution using **Visual Studio 2022**.
4. Set the build configuration to **Release / x64**.
5. Build the solution.
6. Download or locate `fmod64.dll` and place it directly into the game's `Run/` folder.
7. Navigate to the `Run/` directory and launch the compiled executable.
