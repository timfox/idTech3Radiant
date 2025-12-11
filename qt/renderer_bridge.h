#pragma once

#include <QString>
#include <QLibrary>

#ifdef RADIANT_USE_ENGINE_RENDERER_VK
#include "tr_public.h"
#endif

// Minimal loader for the engine Vulkan renderer shared library.
// Currently only resolves GetRefAPI; full refimport wiring will follow.
class RendererBridge {
public:
	bool load(const QString& path);
	void unload();
	bool isLoaded() const { return m_lib.isLoaded(); }
	QString status() const { return m_status; }

#ifdef RADIANT_USE_ENGINE_RENDERER_VK
	GetRefAPI_t getRefApi() const { return m_getRefApi; }
#endif

private:
	QLibrary m_lib;
	QString m_status;
#ifdef RADIANT_USE_ENGINE_RENDERER_VK
	GetRefAPI_t m_getRefApi = nullptr;
#endif
};
