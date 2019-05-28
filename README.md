# GTASuspender

![Screenshot](https://i.imgur.com/rexpeWv.png)

This is a standalone program which provides a convenient way to empty a lobby in GTA5 (without injecting any code into the game), instead of doing it manually with Task Manager or Resource Monitor.

It does the following:
- Moves the GTA5 window off-screen to not be in the way (hotkey: `Ctrl + Shift + F9`)
- Suspends GTA5 to force you to be put in a lobby by yourself in GTA Online (hotkey: `Ctrl + Shift + F10`)
- Suspends GTA5 in a loop (10sec suspended, 10sec resumed) to save power while idling in GTA Online (hotkey: `Ctrl + Shift + F11`)

Visual C++ 2019 redistributable must be installed to run the provided binaries.

This project uses the following open source libraries:

[`Boost C++ Libraries`](https://github.com/boostorg/boost)

[`termcolor`](https://github.com/ikalnytskyi/termcolor)

[`Windows Implementation Libraries (WIL)`](https://github.com/microsoft/wil)