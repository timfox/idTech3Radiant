## Cursor Cloud specific instructions

### Project overview

NetRadiant-Custom is a C++ native desktop 3D level editor for id Tech engine games. It uses GNU Make, Qt5, and OpenGL. See `COMPILING` and `README.md` for full details.

### Build commands

- **Full build**: `make CC=gcc-14 CXX=g++-14 CXXFLAGS="-Wno-deprecated-copy" DOWNLOAD_GAMEPACKS=no BUILD=release -j$(nproc)`
- **Clean**: `make clean`
- gcc-14/g++-14 is required; the default gcc-13 on Ubuntu 24.04 produces compilation errors on this codebase.

### System dependencies (Ubuntu 24.04)

Installed via apt: `mesa-common-dev qtbase5-dev libqt5svg5-dev libglib2.0-dev libjpeg-dev libpng-dev libassimp-dev libxml2-dev gcc-14 g++-14`

### Key binaries

After a successful build, binaries are in `install/`:
- `install/radiant.x86_64` — main GUI editor (Qt5, requires display)
- `install/q3map2.x86_64` — Q3 BSP map compiler (CLI)
- `install/q2map.x86_64` — Q2 map compiler (CLI)
- Modules in `install/modules/`, plugins in `install/plugins/`

### Git submodules

The repo has one submodule (`libs/recast`). Run `git submodule update --init --recursive` after cloning.

### Notes

- There is no automated test suite; validation is done by building and running the tools.
- There is no linter configured for this C/C++ project.
- The main editor (`radiant.x86_64`) is a GUI application requiring a display (X11/Wayland). Use the `computerUse` subagent Desktop pane for GUI testing.
- `DOWNLOAD_GAMEPACKS=no` skips downloading game asset packs during build (avoids network dependency, packs are optional for development).
