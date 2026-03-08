// ===========================================================================
// Copyright (C) 2026 Gopex LLC. All rights reserved.
// This file is original work by Gopex LLC and is not derived from any prior sources.
// ===========================================================================
//
// Default skybox shader (Hammer-style tools texture equivalent)
// Apply to brush faces that form the sky boundary. Ships with the editor.
// Replace with your own sky images by creating textures/common/toolsskybox_ft, _bk, _lf, _rt, _up, _dn

textures/common/toolsskybox
{
	qer_editorImage _skybox
	surfaceparm sky
	surfaceparm noimpact
	surfaceparm nolightmap
	surfaceparm nodlight
	skyParms textures/common/toolsskybox 128 -
}
