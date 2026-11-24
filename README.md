# CASE ZERO — Crime Investigation & Detective Simulation

**C-language university project.** Play as a detective on a fictional campus, gather evidence, interview suspects, work out timelines, solve puzzles, and make decisions. The game has a **Windows console edition** and a **Linux desktop GUI edition**. Windows GUI has intentionally been removed.

## Run on Windows (console, no GTK or Python)

1. Right-click the downloaded ZIP and choose **Extract All**.
2. Open the extracted `CASE_ZERO_LINUX_GUI_WINDOWS_CONSOLE` folder.
3. Double-click **`run_windows.bat`** (or `run_console_windows.bat`). A terminal window should launch the game.
4. Type the number for a menu item, then press **Enter**.

**One-time prerequisite:** GCC (C compiler) must be installed. The launcher automatically uses a compiler in your PATH or at `C:\msys64\ucrt64\bin\gcc.exe`. If GCC is missing, install [MSYS2](https://www.msys2.org/), launch **MSYS2 UCRT64**, and run:

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-gcc
```

Then double-click `run_windows.bat` again in File Explorer. The `.bat` file compiles the C program into a temporary `.exe` and runs it, so it also picks up source changes. It uses a new temporary EXE filename to avoid the common `Permission denied` issue when an old executable is still open. Do **not** use the VS Code Code Runner *Output* panel: it does not properly support this interactive, ANSI-colored console game. Use the `.bat` launcher or a real VS Code Terminal.

**Optional PowerShell command** (if GCC is already on PATH), from the project folder:

```powershell
gcc -std=c11 case_zero.c -o case_zero_play.exe; if ($LASTEXITCODE -eq 0) { .\case_zero_play.exe }
```

## Run on Linux (GTK3 GUI)

On Ubuntu/Debian install GTK3 development files and GCC (one time):

```bash
sudo apt update && sudo apt install -y build-essential libgtk-3-dev
```

Extract the ZIP, open a terminal in the extracted project folder and run:

```bash
bash run_linux.sh
```

This script builds the C game engine and the GTK3 GUI, then opens the GUI. It uses no Python. For the Linux console version instead:

```bash
bash run_console_linux.sh
```

**Note:** The GTK3 GUI must run on a Linux **desktop session** with a graphical display. It cannot open in a text-only SSH terminal without display forwarding.

## How the game works

At the main menu, enter a number (Windows/Linux console) or click a choice (Linux GUI). Investigate before accusing a suspect. Some actions use limited moves or battery points. Read the evidence carefully: a matching access card alone does not prove guilt. Your score depends on your decisions. The game uses Pakistan Standard Time (UTC+5) and a 12-hour AM/PM clock; the session counter tracks how long you have played.

| Option | Game mode | What you do |
|---|---|---|
| 1 | The Blackout Conspiracy | Follow a linked campus mystery involving a courier, locked room, Sudoku archive and final reveal. |
| 2 | Mafia: Trust No One | Investigate and vote while hidden Mafia roles threaten the group. |
| 3 | The Midnight Murder | Investigate a death, check CCTV timestamps and card records, interview suspects, then accuse with evidence. |
| 4 | Escape the Darkness | Escape a basement by solving switch, time and code puzzles before running out of actions. |
| 5 | Sudoku Security Lock | Play or solve Sudoku puzzles with a built-in C solver. |
| 6 | Case History | Review prior results. |
| 7 | Detective Case Files | Play randomized investigations with difficulty settings, evidence tracking, suspicion scoring, hints, and save/load. |
| 8 | Exit | Quit the game. |

**CCTV example:** If the video recorder's clock reads 11:28 PM but is 15 minutes ahead, the corrected timestamp is 11:13 PM. If the game asks only for the corrected *minute* (0–59), enter `13`.

## Files and what they do

| File | Purpose |
|---|---|
| `case_zero.c` | Core C game engine: menu, scenarios, puzzles, scores, case history, terminal support. |
| `detective_cases.h` | C header included by the engine: additional case generator, evidence, suspicion meter, file save/load. |
| `case_zero_gui.c` | **Linux GUI** written in C using GTK3: game window, clickable menu, transcript, clock. |
| `run_windows.bat` | Recommended Windows launcher — **console only**. |
| `run_console_windows.bat` | Compiles and runs console C game on Windows using GCC. |
| `run_linux.sh` | Builds and starts Linux GTK3 GUI. |
| `run_console_linux.sh` | Builds and starts Linux console version. |
| `PROPOSAL_CHECKLIST.md` | Requirements cross-check against the original university proposal. |
| `README.md` | This guide. |

**Programming languages:** C11 is used for all game logic and the Linux GUI; Bash handles Linux launch scripts; Windows Batch handles Windows launching; Markdown is for text documentation. GTK3 is a C library, **not** another programming language. No Windows GTK3 build scripts, Windows GUI launchers, Python, or viva guide are included.

## Troubleshooting

- `gcc is not recognized`: install GCC via MSYS2 UCRT64 using the command above, then try `run_windows.bat` again.
- `case_zero.c: No such file or directory`: run the batch launcher **from the extracted folder** and keep source files together.
- `ENABLE_VIRTUAL_TERMINAL_PROCESSING undeclared`: this package includes a compatibility definition in `case_zero.c`.
- `Permission denied` when overwriting an EXE: the Windows batch launcher avoids overwriting the same filename each run.
- Escape codes like `ESC[31m` appear: do not use VS Code's Code Runner *Output* window. Use the proper console launcher.
- Linux GUI reports missing GTK3: install `libgtk-3-dev` and `build-essential` with `apt`.
- Linux GUI does not appear: check you are logged into a desktop session; the GUI has not been verified on every distribution.

**Testing caveat:** The console engine is compiled and smoke-tested on Linux. Windows compilation/launch and GTK3 GUI startup need verification on their respective desktop environments. No precompiled Windows executable is bundled.
