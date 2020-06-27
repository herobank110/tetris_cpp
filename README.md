# Tetris

Tetris game with SDL2 for desktop and emscripten.

[Live demo](https://davidkanekanian.co.uk/res/random_stuff/emscripten_2/Web/)

## Building

### Desktop

The desktop version can be built with Visual Studio by building the `tetris` solution.

**Requirements:**
- Visual Studio 2019 (16.6.1 or later) with Desktop C++ Development workload
- SDL2 (can be installed via [vcpkg](https://github.com/microsoft/vcpkg))

### Web

The web version can be built by running the `build.ps1` script. Emscripten will automatically download SDL2 if necessary.

**Requirements:**
- Powershell (version 7.0.2)
- Emscripten (version 1.38.13)
