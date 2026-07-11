## Cursor Cloud specific instructions

NetRadiant-Custom is a C++ desktop level editor for id Tech engine games (Quake 3, OpenArena, etc.). It is a single-product codebase (not a monorepo) built entirely with GNU Make.

### Project overview

See `COMPILING` and `README.md` for broader project details.

### Build

Debug build:

```sh
make CC=gcc-14 CXX=g++-14 CXXFLAGS="-Wno-deprecated-copy" DEPENDENCIES_CHECK=off DOWNLOAD_GAMEPACKS=no BUILD=debug -j$(nproc)
```

Release build:

```sh
make CC=gcc-14 CXX=g++-14 CXXFLAGS="-Wno-deprecated-copy" DOWNLOAD_GAMEPACKS=no BUILD=release -j$(nproc)
```

- `gcc-14`/`g++-14` is required.
- Output goes to `install/`.
- Main binaries: `install/radiant.x86_64`, `install/q3map2.x86_64`, `install/q2map.x86_64`.
- `make clean` removes build artifacts.
- The `libs/recast` submodule reference is stale upstream. If needed, use `cd libs/recast && git checkout origin/main`.

### Running the editor

- At least one gamepack is required.
- Example: `PACKFILTER="OpenArenaPack" BATCH=1 bash download-gamepacks.sh`
- Then install it with: `bash install-gamepack.sh games/OpenArenaPack install/gamepacks`
- Launch with: `cd install && ./radiant.x86_64`

### System dependencies (Ubuntu 24.04)

Installed via apt: `gcc-14 g++-14 mesa-common-dev qtbase5-dev libqt5svg5-dev libglib2.0-dev libjpeg-dev libpng-dev libassimp-dev libxml2-dev zlib1g-dev`

### Testing

- No formal unit test framework is configured.
- Regression tests live in `regression_tests/q3map2/`.
- Example BSP regression run:
  `install/q3map2.x86_64 -game quake3 -fs_basepath /tmp/q3test -bsp regression_tests/q3map2/<test>/maps/<test>.map`
