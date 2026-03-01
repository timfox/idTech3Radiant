//Maya ASCII 2023 scene
requires maya "2023";
createNode transform -n "box1";
	setAttr ".t" -type "double3" 0 0 0 ;
createNode mesh -n "boxShape1" -p "box1";
	setAttr -s 8 ".vt[0:7]"
		-64 -64 64
		64 -64 64
		-64 64 64
		64 64 64
		-64 64 -64
		64 64 -64
		-64 -64 -64
		64 -64 -64;
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
