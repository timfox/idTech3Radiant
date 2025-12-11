#pragma once

#include <QString>

#ifdef RADIANT_USE_ENGINE_RENDERER_VK
#include "tr_public.h"

// A minimal refimport_t shim to initialize the renderer in Qt.
// This is intentionally skeletal: logging and filesystem hooks only.
refimport_t MakeQtRefImport(const QString& logPath);
void SetVkSurfaceWindow(quintptr winId);

#endif
