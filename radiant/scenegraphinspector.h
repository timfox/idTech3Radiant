/*
   Copyright (C) 1999-2006 Id Software, Inc. and contributors.
   This file is part of GtkRadiant.

   GtkRadiant is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
 */

#pragma once

class QMainWindow;

void ScenegraphInspector_createDock( QMainWindow* window );
void ScenegraphInspector_destroyDock();
void ScenegraphInspector_toggleShown();
