//Maya ASCII 2023 scene
//Name: test_scene.ma
requires maya "2023";
// Room: 512x512x384, walls 16 thick
// Floor brush: (-272,-272,0) to (272,272,-16) in Maya Y-up coords = (-272,0,-272) to (272,-16,272)
createNode transform -n "floor";
	setAttr ".t" -type "double3" 0 -8 0 ;
createNode mesh -n "floorShape" -p "floor";
	setAttr -s 8 ".vt[0:7]"
		-272 -8 -272
		272 -8 -272
		-272 8 -272
		272 8 -272
		-272 8 272
		272 8 272
		-272 -8 272
		272 -8 272;
	setAttr -s 12 ".ed[0:11]"
		0 1 0  2 3 0  4 5 0  6 7 0
		0 2 0  1 3 0  2 4 0  3 5 0
		4 6 0  5 7 0  6 0 0  7 1 0;
	setAttr -s 6 -ch 24 ".fc[0:5]" -type "polyFaces"
		f 4 0 5 -2 -5
		f 4 1 7 -3 -7
		f 4 2 9 -4 -9
		f 4 3 11 -1 -11
		f 4 -12 -10 -8 -6
		f 4 10 4 6 8;
// Ceiling brush
createNode transform -n "ceiling";
	setAttr ".t" -type "double3" 0 392 0 ;
createNode mesh -n "ceilingShape" -p "ceiling";
	setAttr -s 8 ".vt[0:7]"
		-272 -8 -272
		272 -8 -272
		-272 8 -272
		272 8 -272
		-272 8 272
		272 8 272
		-272 -8 272
		272 -8 272;
	setAttr -s 12 ".ed[0:11]"
		0 1 0  2 3 0  4 5 0  6 7 0
		0 2 0  1 3 0  2 4 0  3 5 0
		4 6 0  5 7 0  6 0 0  7 1 0;
	setAttr -s 6 -ch 24 ".fc[0:5]" -type "polyFaces"
		f 4 0 5 -2 -5
		f 4 1 7 -3 -7
		f 4 2 9 -4 -9
		f 4 3 11 -1 -11
		f 4 -12 -10 -8 -6
		f 4 10 4 6 8;
// North wall
createNode transform -n "wall_north";
	setAttr ".t" -type "double3" 0 192 264 ;
createNode mesh -n "wallNorthShape" -p "wall_north";
	setAttr -s 8 ".vt[0:7]"
		-272 -192 -8
		272 -192 -8
		-272 192 -8
		272 192 -8
		-272 192 8
		272 192 8
		-272 -192 8
		272 -192 8;
	setAttr -s 12 ".ed[0:11]"
		0 1 0  2 3 0  4 5 0  6 7 0
		0 2 0  1 3 0  2 4 0  3 5 0
		4 6 0  5 7 0  6 0 0  7 1 0;
	setAttr -s 6 -ch 24 ".fc[0:5]" -type "polyFaces"
		f 4 0 5 -2 -5
		f 4 1 7 -3 -7
		f 4 2 9 -4 -9
		f 4 3 11 -1 -11
		f 4 -12 -10 -8 -6
		f 4 10 4 6 8;
// South wall
createNode transform -n "wall_south";
	setAttr ".t" -type "double3" 0 192 -264 ;
createNode mesh -n "wallSouthShape" -p "wall_south";
	setAttr -s 8 ".vt[0:7]"
		-272 -192 -8
		272 -192 -8
		-272 192 -8
		272 192 -8
		-272 192 8
		272 192 8
		-272 -192 8
		272 -192 8;
	setAttr -s 12 ".ed[0:11]"
		0 1 0  2 3 0  4 5 0  6 7 0
		0 2 0  1 3 0  2 4 0  3 5 0
		4 6 0  5 7 0  6 0 0  7 1 0;
	setAttr -s 6 -ch 24 ".fc[0:5]" -type "polyFaces"
		f 4 0 5 -2 -5
		f 4 1 7 -3 -7
		f 4 2 9 -4 -9
		f 4 3 11 -1 -11
		f 4 -12 -10 -8 -6
		f 4 10 4 6 8;
// East wall
createNode transform -n "wall_east";
	setAttr ".t" -type "double3" 264 192 0 ;
createNode mesh -n "wallEastShape" -p "wall_east";
	setAttr -s 8 ".vt[0:7]"
		-8 -192 -272
		8 -192 -272
		-8 192 -272
		8 192 -272
		-8 192 272
		8 192 272
		-8 -192 272
		8 -192 272;
	setAttr -s 12 ".ed[0:11]"
		0 1 0  2 3 0  4 5 0  6 7 0
		0 2 0  1 3 0  2 4 0  3 5 0
		4 6 0  5 7 0  6 0 0  7 1 0;
	setAttr -s 6 -ch 24 ".fc[0:5]" -type "polyFaces"
		f 4 0 5 -2 -5
		f 4 1 7 -3 -7
		f 4 2 9 -4 -9
		f 4 3 11 -1 -11
		f 4 -12 -10 -8 -6
		f 4 10 4 6 8;
// West wall
createNode transform -n "wall_west";
	setAttr ".t" -type "double3" -264 192 0 ;
createNode mesh -n "wallWestShape" -p "wall_west";
	setAttr -s 8 ".vt[0:7]"
		-8 -192 -272
		8 -192 -272
		-8 192 -272
		8 192 -272
		-8 192 272
		8 192 272
		-8 -192 272
		8 -192 272;
	setAttr -s 12 ".ed[0:11]"
		0 1 0  2 3 0  4 5 0  6 7 0
		0 2 0  1 3 0  2 4 0  3 5 0
		4 6 0  5 7 0  6 0 0  7 1 0;
	setAttr -s 6 -ch 24 ".fc[0:5]" -type "polyFaces"
		f 4 0 5 -2 -5
		f 4 1 7 -3 -7
		f 4 2 9 -4 -9
		f 4 3 11 -1 -11
		f 4 -12 -10 -8 -6
		f 4 10 4 6 8;
// Point light
createNode transform -n "light1";
	setAttr ".t" -type "double3" 0 300 0 ;
createNode pointLight -n "pointLight1" -p "light1";
	setAttr ".in" 1.5;
	setAttr ".cl" -type "float3" 1.0 0.95 0.85 ;
// Player spawn - transform named as Q3 entity classname
createNode transform -n "info_player_deathmatch";
	setAttr ".t" -type "double3" 0 24 0 ;
// A second light
createNode transform -n "light2";
	setAttr ".t" -type "double3" 128 200 128 ;
createNode pointLight -n "pointLight2" -p "light2";
	setAttr ".in" 0.8;
	setAttr ".cl" -type "float3" 0.8 0.9 1.0 ;
