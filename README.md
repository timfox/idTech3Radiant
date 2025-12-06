id Tech 3 Radiant
==========

id Tech 3 Radiant is an open-source, cross-platform level editor for id Tech based games. It comes with some map compilers and data authoring tools.

Useful links
------------

- [id Tech 3 Radiant website](https://idtech3.com/radiant/)

Supported games
---------------

GtkRadiant provides level editing support for [Quake](https://en.wikipedia.org/wiki/Quake_(video_game)), [Quake2](https://en.wikipedia.org/wiki/Quake_II), [Quake2 Re-Release](https://en.wikipedia.org/wiki/Quake_II#Enhanced_version_and_Call_of_the_Machine), [Quake III Arena](https://ioquake3.org), [QuakeLive](https://www.quakelive.com), [Quetoo](http://quetoo.org), [Return to Castle Wolfenstein](https://en.wikipedia.org/wiki/Return_to_Castle_Wolfenstein), [Star Trek Voyager: Elite Force](https://en.wikipedia.org/wiki/Star_Trek:_Voyager_–_Elite_Force), [Star Wars Jedi Knight: Jedi Academy](https://en.wikipedia.org/wiki/Star_Wars_Jedi_Knight:_Jedi_Academy), [Unvanquished](https://www.unvanquished.net), [Urban Terror](http://urbanterror.info), [Wolfenstein: Enemy Territory](http://www.splashdamage.com/content/wolfenstein-enemy-territory-barracks).

How to build
------------

You can find more complete instructions to build on Windows [here](https://icculus.org/gtkradiant/documentation/windows_compile_guide/) and to build on Mac OS [here](apple/README.md).

The Linux version is developed and distributed via Flatpak. See [GtkRadiant on Flathub](https://flathub.org/apps/io.github.TTimo.GtkRadiant).

```sh
# ArchLinux
pacman -S git scons libxml2 gtk2 freeglut gtkglext subversion libjpeg-turbo
```

```sh
# get the source
git clone "https://github.com/timfox/idTech3Radiant.git"

# enter the source tree
cd radiant

# build everything
scons
```

You can build a specific part like this:

```sh
# only build the GtkRadiant level editor
scons target="radiant"

# only build the q3map2 map compiler and the q3data tool
scons target="q3map2,q3data"
```

Level editor binary (`radiant`) and tools (like `q3map2`) will be found in `install/` directory. 
The build process automatically fetches gamepacks.

Building on Linux with the Flatpak SDK
--------------------------------------

```sh
# get the flatpak manifest
git clone "https://github.com/flathub/io.github.TTimo.GtkRadiant.git"

# use flatpak-builder
cd io.github.TTimo.GtkRadiant
flatpak-builder --force-clean --user --install gtkradiant io.github.TTimo.GtkRadiant.json
```

You can also checkout the GtkRadiant tree locally and [modify the manifest to point to it](https://gist.github.com/TTimo/fd683b368048dc8802e9aaf353ef68ec).

Here is what you can do for debugging:

```sh
flatpak run --command=sh --devel io.github.TTimo.GtkRadiant
gdb /app/gtkradiant/radiant.bin
```
