if (!(Test-Path Web)) { New-Item Web -ItemType Directory }
em++ .\tetris.cpp -s WASM=1 -o Web/index.html -s USE_SDL=2 -std=c++17 -Wall -Wextra
