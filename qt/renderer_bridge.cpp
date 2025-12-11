#include "renderer_bridge.h"

bool RendererBridge::load(const QString& path){
	unload();

	m_lib.setFileName(path);
	m_lib.setLoadHints(QLibrary::ResolveAllSymbolsHint);

	if ( !m_lib.load() ) {
		m_status = QStringLiteral("Failed to load renderer: %1").arg(m_lib.errorString());
		return false;
	}

#ifdef RADIANT_USE_ENGINE_RENDERER_VK
	m_getRefApi = reinterpret_cast<GetRefAPI_t>( m_lib.resolve("GetRefAPI") );
	if ( !m_getRefApi ) {
		m_status = QStringLiteral("Loaded renderer but GetRefAPI not found");
		m_lib.unload();
		return false;
	}
	m_status = QStringLiteral("Renderer loaded; GetRefAPI resolved.");
#else
	m_status = QStringLiteral("Renderer loaded (GetRefAPI resolution disabled in this build).");
#endif

	return true;
}

void RendererBridge::unload(){
#ifdef RADIANT_USE_ENGINE_RENDERER_VK
	m_getRefApi = nullptr;
#endif
	if ( m_lib.isLoaded() ) {
		m_lib.unload();
	}
}
