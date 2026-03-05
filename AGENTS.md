## Cursor Cloud specific instructions

NetRadiant-Custom is a C++ desktop level editor for id Tech engine games (Quake 3, OpenArena, etc.). It is a single-product codebase (not a monorepo) built entirely with GNU Make.

### Build

```
make CC=gcc-14 CXX=g++-14 CXXFLAGS="-Wno-deprecated-copy" DEPENDENCIES_CHECK=off DOWNLOAD_GAMEPACKS=no BUILD=debug -j$(nproc)
```

- **gcc-14 is required.** gcc-13 fails on C++20 aggregate initialization of `BasicVector3` explicit constructors.
- Output goes to `install/` directory; main binaries: `install/radiant.x86_64`, `install/q3map2.x86_64`, `install/q2map.x86_64`.
- The `libs/recast` git submodule references a commit that no longer exists upstream. Workaround: `cd libs/recast && git checkout origin/main`. The build works fine with the latest `main` branch.

### Running the editor

- Requires at least one gamepack. Download one with: `PACKFILTER="OpenArenaPack" BATCH=1 bash download-gamepacks.sh` then install: `bash install-gamepack.sh games/OpenArenaPack install/gamepacks`.
- Launch: `cd install && ./radiant.x86_64`
- On first run after a clean build, the editor may show a "Startup Failure" dialog asking to reset preferences — click "Yes", then select the game and click OK.

### Testing

- No formal test framework. Regression tests live in `regression_tests/q3map2/` (18 map-based test cases).
- Run a BSP compile test: `install/q3map2.x86_64 -game quake3 -fs_basepath /tmp/q3test -bsp regression_tests/q3map2/<test>/maps/<test>.map`
- There is no lint tool configured for this C++ codebase.

### System dependencies (Ubuntu 24.04)

Installed via apt: `gcc-14 g++-14 mesa-common-dev qtbase5-dev libqt5svg5-dev libglib2.0-dev libjpeg-dev libpng-dev libassimp-dev libxml2-dev zlib1g-dev`
