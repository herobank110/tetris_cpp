# Tetris

Tetris game with SDL2 for desktop and emscripten.

[Live demo](https://davidkanekanian.co.uk/res/random_stuff/emscripten_2/Web/)

## Building

Building the project currently must be done from a Windows computer.

### Desktop

The desktop version can be built with Visual Studio by building the `tetris` solution.

**Requirements:**
- Visual Studio 2019 (16.6.1 or later) with Desktop C++ Development
- SDL2 (can be installed via [vcpkg](https://github.com/microsoft/vcpkg))

### Web

The web version can be built by running the `build.ps1` script, which currently only works on
Windows. Emscripten will automatically download SDL2 if necessary.

**Requirements:**
- Emscripten (1.38.13 or later)
