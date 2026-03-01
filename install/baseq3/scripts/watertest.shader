// Sky shader - provides skybox and sunlight
textures/watertest/sky
{
	qer_editorimage textures/skies/toxicsky.tga
	surfaceparm sky
	surfaceparm noimpact
	surfaceparm nolightmap
	surfaceparm nodlight
	q3map_sun 1.0 0.95 0.8 150 45 65
	q3map_surfacelight 80
	skyparms - 512 -
}

// Water shader - translucent, non-solid water volume
textures/watertest/water
{
	qer_editorimage textures/liquids/pool3d_3e.tga
	qer_trans 0.5
	surfaceparm trans
	surfaceparm nonsolid
	surfaceparm water
	surfaceparm nolightmap
	q3map_globaltexture
	cull disable
	{
		map $whiteimage
		blendFunc GL_DST_COLOR GL_ZERO
		rgbGen const ( 0.3 0.4 0.5 )
	}
}

// Yellow fog shader - 1-foot fog layer above water
textures/watertest/yellowfog
{
	qer_editorimage textures/sfx/fog_yellow.tga
	surfaceparm trans
	surfaceparm nonsolid
	surfaceparm fog
	surfaceparm nolightmap
	surfaceparm nodrop
	fogparms ( 0.9 0.8 0.1 ) 48
}
