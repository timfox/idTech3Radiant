id Tech 3 Radiant
=================

The open-source, cross-platform level editor for id Tech based games.

## Features

#### Editing

* WASD camera binds
* Full 3D view editing (brush and entity creation, all manipulation tools)
* Uniform merge algorithm for brushes, components, and clipper points
* Vertex editing with add/remove vertices
* UV Tool for texture alignment on faces and patches
* Brush faces extrusion
* CSG Tool (shell modifier)
* Brush resize (QE tool): reduce selected faces to most wanted ones
* Arbitrary texture projections for brushes and curves
* Texture lock supporting any affine transformation, including during vertex/edge manipulation
* Brush formats: Axial projection, Brush primitives, Valve 220 (toggleable preference)
* Autodetect brush type on map open; automatic conversion on Import and Paste
* Support for "stupid quake bug" compatibility

#### Selection & Manipulation

* Tunnel selector and paint selector (left mouse)
* Focus camera on selected (Tab)
* Snapped manipulator modes
* Draggable transform origin for manipulators
* Quick vertices drag / brush faces shear shortcut
* QE tool component mode: drag without hitting handles
* New bbox manipulator: move, rotate, scale, skew
* Connected entities selector/walker

#### Texturing

* Simple shader editor
* Texture painting by drag
* Seamless brush face↔face, patch↔face texture paste
* Texture browser: alpha transparency, search in directories and tags, search shown textures

#### View & Display

* Viewports zoom to mouse pointer
* 50× faster light radius rendering
* Light power adjustable by mouse drag
* Anisotropic texture filtering
* Optional MSAA in viewports
* Fast entity names rendering
* Q3 shader-based skybox rendering

#### Workflow

* Autocaulk
* Model browser
* Patch thicken; patch prefabs aligned to active projection
* Filters toolbar with extra functions on right-click
* "All supported formats" default in open dialogs
* Open *.map via command line (associate *.map with Radiant)
* Working region compilations (compile regioned part only)
* Map info dialog: patches, entities, group entities counts
* Build→Customize: list available build variables
* Customizable keyboard shortcuts and GUI themes/fonts
* Numerous mouse shortcuts (Help → General → Mouse Shortcuts)
* MeshTex plugin
* Modelling maps in Blender/Maya: export brush template to OBJ, model in 3D app, use misc_model (see docs/Modelling_maps_in_Blender.txt)

#### Advanced Entity Systems

* Built-in entity definitions for modern engines (fire, water, wind, physics, gravity, vehicles)
* Loaded from `scripts/entities_advanced.def` in the editor installation
* Includes: env_fire_emitter, env_water_volume, env_fan, prop_breakable, trigger_gravity, env_spawn_volume, info_vehicle, and more
* Entity definitions also loaded from editor `scripts/` directory

#### Python Scripting

* Python Script Editor workbench (Tools → Python Script Editor, Ctrl+Alt+Y)
* Maya-like workflow: edit, run, inspect output in a built-in dock
* Scripts run in a subprocess with project paths in the environment; non-blocking execution
* `radiant` module: app_path, engine_path, game_path, maps_path, scripts_path, current_map
* Configurable Python executable in Preferences → Game

#### Performance

* Hash-based layer lookups for faster scene traversals
* Asynchronous update check (no UI freeze)

#### Q3Map2

* q3map_remapshader remaps on all stages
* Automatic map packager (complete Q3 support)
* Full path reporting on file syntax errors
* Simultaneous samples+filter use
* -brightness 0..a lot (def 1), -contrast -255..255, -saturation
* -bouncecolorratio 0..1, -nolm, -novertex 0..1, -vertexscale
* Area lights backsplash algorithm; -backsplash (float)scale (float)distance
* Faster lightmaps packing algorithm
* -extlmhacksize for external lightmaps
* Valve220 map format autodetection
* Consistent brush content deduction with mixed face parameters
* Model shaders path deduction
* Model autoclip with 20+ clipping modes; negative misc_model scale
* Assimp model loading (40+ formats)
* -json BSP export/import, -mergebsp
* No shaderlist.txt mode: load all shaders

See changelog-custom.txt for more.

## Building

See [COMPILING](COMPILING) for build instructions.
