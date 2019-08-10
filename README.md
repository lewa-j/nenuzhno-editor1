# nenuzhno-editor

Meant to be an editor for [nenuzhno-engine](https://github.com/lewa-j/nenuzhno-engine_iter1)
Most of features either unfinished or bugged as hell.
Actualy editing features wasn't done at all. Only some importers, viewers, exporters.

## Model Viewer

Import

* obj - Wavefront
* mesh - plain binary vertices

Export

* mesh - plain binary vertices
* nmf - Nenuzhno model format
* smd - [Studiomdl Data](https://developer.valvesoftware.com/wiki/Studiomdl_Data)

Also can scale meshes

## PCC Viewer

Mass Effect 3 content viewer.
Can open pcc file and shows its content in different ways:

* tree
* names table
* imports
* exports
Additionaly exports can be filtered by type:
* Mesh
* Texture

If tree or export entry can be opened, "view" button show up.
Static and Dynamic Meshes, Textures, Levels have their own view screens.
On Mesh view screen you can: rotate or scale mesh, select render mode(texture, normal, lighting, normalmaped) and export mesh (smd).
On Textures view screen you can: select transparancy display (RGBA, RGB), switch to next or previous texture, export (dds).
On Level view screen you can just fly around. There is an export to vmf, but it is unfinished and unusable.
Material, MaterialInstanceConstant, AnimSet, AnimSequence, BioAnimSetData haven't their view screens, but prints some info to log.

### Other fuctions

**View levels** - reads test_levels.txt file with scene description. It basically merges levels.
File format:
```
start position
texture lod
pcc files count
	for each file
	name and export index
the rest is ignored
```

**View Anim** - reads pcc_anim.txt file with models list, in general - character. Meant to be an animation viewer, but animation loading wasn't complete.
File format:
```
animation mode (0 - nothing, 1 - random)
pcc files count
	for each file
	name and export index
the rest is ignored
```

Thanks to 
[ME3Explorer](https://github.com/ME3Explorer/ME3Explorer)
[UModel](https://github.com/gildor2/UModel)

## Minimalist Bink Decoder

Thanks to [ffmpeg](https://ffmpeg.org/)

## The Rest ##

Mostly incomplete and unusable.

* Sound editor - generates sin waves or reads vaw file. Control only through config
* Model Editor
* Graph Util

## Dependencies

* [nenuzhno-engine](https://github.com/lewa-j/nenuzhno-engine_iter1)
* [MinGW-W64](https://mingw-w64.org)
* [GLFW 3.3](https://www.glfw.org/)
* [GLM 0.9.9.0](https://glm.g-truc.net/0.9.9/index.html)
* [GLEW 2.0](http://glew.sourceforge.net/)
* [OpenAL-soft](https://github.com/kcat/openal-soft)

Also uses **base** folder from engine assets.
