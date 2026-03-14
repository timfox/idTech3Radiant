/*
   Copyright (C) 1999-2006 Id Software, Inc. and contributors.
   AI Assistant - Editor-side AI integration for Radiant.
   Structured context extraction, placement plans, validation, undo-safe execution.
*/

#include "ai_assistant.h"

#include "mainframe.h"
#include "map.h"
#include "camwindow.h"
#include "entity.h"
#include "ientity.h"
#include "select.h"
#include "eclasslib.h"
#include "grid.h"
#include "iundo.h"
#include "ishaders.h"
#include "ifilesystem.h"
#include "preferences.h"
#include "preferencesystem.h"
#include "stream/stringstream.h"
#include "stringio.h"
#include "string/string.h"
#include "scenelib.h"
#include "generic/callback.h"
#include "nullmodel.h"
#include "qe3.h"
#include "os/path.h"

#include <QMainWindow>
#include <QDockWidget>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QGroupBox>
#include <QCheckBox>
#include <QFormLayout>
#include <QListWidget>
#include <QSettings>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QMessageBox>
#include <QApplication>
#include <QElapsedTimer>
#include <QInputDialog>
#include <QScrollArea>
#include <QTabWidget>
#include <QProcess>
#include <QFileDialog>
#include <QImage>
#include <QPixmap>
#include <QLabel>
#include <QScrollBar>
#include <QTemporaryFile>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>

#define RAPIDJSON_PARSE_DEFAULT_FLAGS ( kParseCommentsFlag | kParseTrailingCommasFlag | kParseNanAndInfFlag )
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include <cmath>

namespace
{

QDockWidget* g_aiAssistantDock{};
QNetworkAccessManager* g_aiNetworkManager{};

const char* const c_aiAssistantSettingsPrefix = "AIAssistant/";

// Image generation
CopiedString g_aiImageIrisPath;
CopiedString g_aiImageIrisModel;
CopiedString g_aiImageDalleApiKey;
QProcess* g_aiImageProcess{};
QImage g_aiImageLastGenerated;
QString g_aiImageLastPath;
QPlainTextEdit* g_aiImagePromptEdit{};
QComboBox* g_aiImageProviderCombo{};
QLineEdit* g_aiImageIrisPathEdit{};
QLineEdit* g_aiImageIrisModelEdit{};
QLabel* g_aiImagePreviewLabel{};
QLabel* g_aiImageStatusLabel{};
QPushButton* g_aiImageGenerateBtn{};
QPushButton* g_aiImageSaveBtn{};
QPushButton* g_aiImageApplyBtn{};

bool g_aiAssistantEnabled = true;
std::vector<struct AIAssistantAgentConfig> g_aiAssistantAgents;
CopiedString g_aiAssistantActiveAgent = "OpenAI";

struct AIAssistantAgentConfig {
	CopiedString name;
	CopiedString provider;   // OpenAI, Gemini, Mock
	CopiedString endpoint;
	CopiedString model;
	bool useEnvVar = true;   // true = use keyEnvVar, false = use apiKey
	CopiedString keyEnvVar;
	CopiedString apiKey;     // when useEnvVar=false
};

QSettings& AIAssistant_settings(){
	static QSettings settings;
	return settings;
}

QString AIAssistant_setting( const char* key, const QString& fallback = {} ){
	return AIAssistant_settings().value( StringStream( c_aiAssistantSettingsPrefix, key ).c_str(), fallback ).toString();
}

void AIAssistant_setSetting( const char* key, const QVariant& value ){
	AIAssistant_settings().setValue( StringStream( c_aiAssistantSettingsPrefix, key ).c_str(), value );
}

static std::string agentsToJson(){
	rapidjson::Document doc( rapidjson::kArrayType );
	auto& alloc = doc.GetAllocator();
	for ( const auto& a : g_aiAssistantAgents ) {
		rapidjson::Value obj( rapidjson::kObjectType );
		obj.AddMember( "name", rapidjson::Value( a.name.c_str(), alloc ), alloc );
		obj.AddMember( "provider", rapidjson::Value( a.provider.c_str(), alloc ), alloc );
		obj.AddMember( "endpoint", rapidjson::Value( a.endpoint.c_str(), alloc ), alloc );
		obj.AddMember( "model", rapidjson::Value( a.model.c_str(), alloc ), alloc );
		obj.AddMember( "useEnvVar", a.useEnvVar, alloc );
		obj.AddMember( "keyEnvVar", rapidjson::Value( a.keyEnvVar.c_str(), alloc ), alloc );
		obj.AddMember( "apiKey", rapidjson::Value( a.apiKey.c_str(), alloc ), alloc );
		doc.PushBack( obj, alloc );
	}
	rapidjson::StringBuffer buf;
	rapidjson::Writer<rapidjson::StringBuffer> w( buf );
	doc.Accept( w );
	return std::string( buf.GetString() );
}

static void agentsFromJson( const char* json ){
	g_aiAssistantAgents.clear();
	if ( !json || !*json ) return;
	rapidjson::Document doc;
	doc.Parse( json );
	if ( doc.HasParseError() || !doc.IsArray() ) return;
	for ( rapidjson::SizeType i = 0; i < doc.Size(); ++i ) {
		const auto& o = doc[i];
		if ( !o.IsObject() ) continue;
		AIAssistantAgentConfig a;
		a.name = o.HasMember( "name" ) ? o["name"].GetString() : "";
		a.provider = o.HasMember( "provider" ) ? o["provider"].GetString() : "OpenAI";
		a.endpoint = o.HasMember( "endpoint" ) ? o["endpoint"].GetString() : "https://api.openai.com/v1/chat/completions";
		a.model = o.HasMember( "model" ) ? o["model"].GetString() : "gpt-4o-mini";
		a.useEnvVar = o.HasMember( "useEnvVar" ) ? o["useEnvVar"].GetBool() : true;
		a.keyEnvVar = o.HasMember( "keyEnvVar" ) ? o["keyEnvVar"].GetString() : "OPENAI_API_KEY";
		a.apiKey = o.HasMember( "apiKey" ) ? o["apiKey"].GetString() : "";
		if ( !a.name.empty() ) g_aiAssistantAgents.push_back( a );
	}
}

static void ensureDefaultAgents(){
	if ( g_aiAssistantAgents.empty() ) {
		AIAssistantAgentConfig a;
		a.name = "OpenAI";
		a.provider = "OpenAI";
		a.endpoint = "https://api.openai.com/v1/chat/completions";
		a.model = "gpt-4o-mini";
		a.useEnvVar = true;
		a.keyEnvVar = "OPENAI_API_KEY";
		g_aiAssistantAgents.push_back( a );
		a.name = "Gemini";
		a.provider = "Gemini";
		a.endpoint = "https://generativelanguage.googleapis.com/v1beta/models/gemini-1.5-flash:generateContent";
		a.model = "gemini-1.5-flash";
		a.keyEnvVar = "GEMINI_API_KEY";
		g_aiAssistantAgents.push_back( a );
		a.name = "Mock";
		a.provider = "Mock";
		a.endpoint = "";
		g_aiAssistantAgents.push_back( a );
		g_aiAssistantActiveAgent = "OpenAI";
	}
}

static const AIAssistantAgentConfig* getActiveAgent(){
	ensureDefaultAgents();
	for ( const auto& a : g_aiAssistantAgents ) {
		if ( a.name == g_aiAssistantActiveAgent.c_str() ) return &a;
	}
	return g_aiAssistantAgents.empty() ? nullptr : &g_aiAssistantAgents[0];
}

static const char* getApiKeyForAgent( const AIAssistantAgentConfig* agent ){
	if ( !agent ) return nullptr;
	if ( agent->useEnvVar ) {
		return getenv( agent->keyEnvVar.c_str() );
	}
	return agent->apiKey.empty() ? nullptr : agent->apiKey.c_str();
}

// --- JSON schema types (EditorContext, PlacementPlan, PlacementAction) ---

struct Vector3Json { double x, y, z; };
struct AABBJson { Vector3Json origin, extents; };

struct SelectionItemJson {
	std::string classname;
	std::string modelPath;
	Vector3Json position;
	Vector3Json angles;
	Vector3Json scale;
	AABBJson bounds;
	std::vector<std::pair<std::string, std::string>> keyvalues;
};

struct CameraContextJson {
	Vector3Json origin;
	Vector3Json angles;
	Vector3Json viewVector;
};

struct EditorContextJson {
	std::string mapPath;
	std::string mapName;
	CameraContextJson camera;
	Vector3Json cursorOrFocal;
	AABBJson selectionBounds;
	AABBJson workZone;
	std::vector<SelectionItemJson> selectedItems;
	std::vector<SelectionItemJson> nearbyEntities;
	std::vector<std::string> allowedModels;
	std::vector<std::string> allowedEntities;
	std::vector<std::string> allowedShaders;
	double gridSize;
};

struct PlacementActionJson {
	std::string action;  // "place_entity", "place_model", "place_light", "reject"
	std::string classname;
	std::string modelPath;
	Vector3Json position;
	Vector3Json angles;
	Vector3Json scale;
	std::vector<std::pair<std::string, std::string>> keyvalues;
	double confidence;
	std::string reason;
};

struct PlacementPlanJson {
	std::vector<PlacementActionJson> actions;
	std::string summary;
};

class SelectionItemKeyValueCollector final : public Entity::Visitor
{
	SelectionItemJson& m_item;
public:
	SelectionItemKeyValueCollector( SelectionItemJson& item )
		: m_item( item ){
	}

	void visit( const char* key, const char* value ) override {
		if ( !string_empty( key ) && !string_empty( value ) ) {
			m_item.keyvalues.emplace_back( key, value );
		}
	}
};

// --- Context extraction ---

class AIContextCollector : public scene::Graph::Walker
{
	std::vector<SelectionItemJson>& m_selected;
	std::vector<SelectionItemJson>& m_nearby;
	const Vector3& m_cameraOrigin;
	double m_nearbyRadius;
	const AABB* m_selectionBounds;
	mutable bool m_hasSelectionBounds;

	static Vector3Json vec3ToJson( const Vector3& v ){
		return { v[0], v[1], v[2] };
	}
	static AABBJson aabbToJson( const AABB& aabb ){
		return {
			vec3ToJson( aabb.origin ),
			{ std::fabs( aabb.extents[0] ), std::fabs( aabb.extents[1] ), std::fabs( aabb.extents[2] ) }
		};
	}
public:
	AIContextCollector( std::vector<SelectionItemJson>& selected, std::vector<SelectionItemJson>& nearby,
	                   const Vector3& camOrigin, double nearbyRadius, const AABB* selBounds )
		: m_selected( selected ), m_nearby( nearby ), m_cameraOrigin( camOrigin ), m_nearbyRadius( nearbyRadius )
		, m_selectionBounds( selBounds ), m_hasSelectionBounds( selBounds != nullptr ){
	}

	bool pre( const scene::Path& path, scene::Instance& instance ) const override {
		Entity* entity = Node_getEntity( path.top() );
		if ( entity == nullptr ) return true;
		if ( path.top().get_pointer() == Map_FindWorldspawn( g_map ) ) return true;

		SelectionItemJson item;
		item.classname = entity->getClassName();
		item.position = vec3ToJson( instance.worldAABB().origin );
		item.angles = { 0, 0, 0 };
		item.scale = { 1, 1, 1 };
		item.bounds = aabbToJson( instance.worldAABB() );

		const EntityClass& eclass = entity->getEntityClass();
		if ( eclass.miscmodel_is ) {
			const char* key = eclass.miscmodel_key();
			if ( !string_empty( key ) ) {
				const char* val = entity->getKeyValue( key );
				if ( !string_empty( val ) ) item.modelPath = val;
			}
		}
		SelectionItemKeyValueCollector keyValueCollector( item );
		entity->forEachKeyValue( keyValueCollector );

		// Angles
		const char* angleVal = entity->getKeyValue( "angle" );
		if ( string_empty( angleVal ) ) angleVal = entity->getKeyValue( "angles" );
		if ( !string_empty( angleVal ) ) {
			float ax = 0, ay = 0, az = 0;
			if ( std::sscanf( angleVal, "%f %f %f", &ax, &ay, &az ) >= 1 )
				item.angles = { ax, ay, az };
		}

		// Scale (modelscale)
		const char* scaleVal = entity->getKeyValue( "modelscale" );
		if ( !string_empty( scaleVal ) ) {
			float s = 1;
			if ( std::sscanf( scaleVal, "%f", &s ) >= 1 )
				item.scale = { s, s, s };
		}

		const double dist = vector3_length( instance.worldAABB().origin - m_cameraOrigin );
		const bool inRadius = dist <= m_nearbyRadius;
		const bool inSelectionBox = m_hasSelectionBounds && m_selectionBounds;
		// TODO: proper AABB overlap with selectionBounds
		( void )inSelectionBox;

		if ( Instance_isSelected( instance ) || instance.childSelected() ) {
			m_selected.push_back( item );
		} else if ( inRadius ) {
			m_nearby.push_back( item );
		}
		return true;
	}
	void post( const scene::Path&, scene::Instance& ) const override {}
};

void AIAssistant_extractContext( EditorContextJson& ctx ){
	ctx.mapPath = Map_Valid( g_map ) ? Map_Name( g_map ) : "unnamed.map";
	ctx.mapName = ctx.mapPath;
	{
		size_t slash = ctx.mapName.find_last_of( "/\\" );
		if ( slash != std::string::npos ) ctx.mapName = ctx.mapName.substr( slash + 1 );
	}

	CamWnd* cam = g_pParentWnd ? g_pParentWnd->GetCamWnd() : nullptr;
	if ( cam ) {
		const Vector3& o = Camera_getOrigin( *cam );
		const Vector3& a = Camera_getAngles( *cam );
		const Vector3& v = Camera_getViewVector( *cam );
		ctx.camera.origin = { o[0], o[1], o[2] };
		ctx.camera.angles = { a[0], a[1], a[2] };
		ctx.camera.viewVector = { v[0], v[1], v[2] };
		ctx.cursorOrFocal = { o[0], o[1], o[2] };
	} else {
		ctx.cursorOrFocal = ctx.camera.origin = ctx.camera.angles = ctx.camera.viewVector = { 0, 0, 0 };
	}

	const select_workzone_t& wz = Select_getWorkZone();
	ctx.workZone.origin = { ( wz.d_work_min[0] + wz.d_work_max[0] ) / 2,
	                        ( wz.d_work_min[1] + wz.d_work_max[1] ) / 2,
	                        ( wz.d_work_min[2] + wz.d_work_max[2] ) / 2 };
	ctx.workZone.extents = { ( wz.d_work_max[0] - wz.d_work_min[0] ) / 2,
	                         ( wz.d_work_max[1] - wz.d_work_min[1] ) / 2,
	                         ( wz.d_work_max[2] - wz.d_work_min[2] ) / 2 };

	Vector3 selMin, selMax;
	Select_GetBounds( selMin, selMax );
	AABB selAabb;
	if ( GlobalSelectionSystem().countSelected() > 0 ) {
		selAabb = GlobalSelectionSystem().getBoundsSelected();
		ctx.selectionBounds.origin = { selAabb.origin[0], selAabb.origin[1], selAabb.origin[2] };
		ctx.selectionBounds.extents = { selAabb.extents[0], selAabb.extents[1], selAabb.extents[2] };
	} else {
		ctx.selectionBounds = ctx.workZone;
	}

	ctx.selectedItems.clear();
	ctx.nearbyEntities.clear();
	const double nearbyRadius = 512.0;
	const Vector3 camOrigin = cam ? Camera_getOrigin( *cam ) : Vector3( 0, 0, 0 );
	AIContextCollector collector( ctx.selectedItems, ctx.nearbyEntities, camOrigin, nearbyRadius,
	                              GlobalSelectionSystem().countSelected() > 0 ? &selAabb : nullptr );
	GlobalSceneGraph().traverse( collector );

	ctx.gridSize = GetGridSize() > 0 ? static_cast<double>( GetGridSize() ) : 8.0;

	// Collect allowed entities
	class EntityCollector : public EntityClassVisitor
	{
		std::vector<std::string>& m_out;
		bool m_modelsOnly;
	public:
		EntityCollector( std::vector<std::string>& out, bool modelsOnly ) : m_out( out ), m_modelsOnly( modelsOnly ){}
		void visit( EntityClass* eclass ) override {
			if ( eclass->unknown ) return;
			if ( m_modelsOnly && !eclass->miscmodel_is ) return;
			m_out.push_back( eclass->name() );
		}
	};
	EntityCollector ecCollect( ctx.allowedEntities, false );
	GlobalEntityClassManager().forEach( ecCollect );

	ctx.allowedModels.clear();
	ctx.allowedShaders.clear();
	// Populate shaders from GlobalShaderSystem
	class ShaderNameCollector
	{
		std::vector<std::string>& m_shaders;
		std::size_t m_limit;
	public:
		ShaderNameCollector( std::vector<std::string>& out, std::size_t limit = 500 ) : m_shaders( out ), m_limit( limit ){}
		void operator()( const char* name ){
			if ( m_shaders.size() < m_limit && name && *name )
				m_shaders.push_back( name );
		}
	};
	ShaderNameCollector shaderCollect( ctx.allowedShaders, 500 );
	GlobalShaderSystem().foreachShaderName( makeCallback( shaderCollect ) );
	// Populate models from VFS (models/ with common extensions)
	class ModelPathCollector
	{
		std::vector<std::string>& m_models;
		std::size_t m_limit;
	public:
		ModelPathCollector( std::vector<std::string>& out, std::size_t limit = 300 ) : m_models( out ), m_limit( limit ){}
		void operator()( const char* path ){
			if ( m_models.size() < m_limit && path && *path )
				m_models.push_back( path );
		}
	};
	ModelPathCollector modelCollect( ctx.allowedModels, 300 );
	GlobalFileSystem().forEachFile( "models/", "ase", makeCallback( modelCollect ), 3 );
	GlobalFileSystem().forEachFile( "models/", "md3", makeCallback( modelCollect ), 3 );
	GlobalFileSystem().forEachFile( "models/", "obj", makeCallback( modelCollect ), 3 );
}

// --- Serialize EditorContext to JSON ---

static void writeVec3( rapidjson::Value& obj, rapidjson::Document::AllocatorType& alloc, const char* key, const Vector3Json& v ){
	rapidjson::Value arr( rapidjson::kArrayType );
	arr.PushBack( v.x, alloc );
	arr.PushBack( v.y, alloc );
	arr.PushBack( v.z, alloc );
	obj.AddMember( rapidjson::StringRef( key ), arr, alloc );
}

static std::string EditorContext_toJson( const EditorContextJson& ctx ){
	rapidjson::Document doc( rapidjson::kObjectType );
	auto& alloc = doc.GetAllocator();

	doc.AddMember( "mapPath", rapidjson::Value( ctx.mapPath.c_str(), alloc ), alloc );
	doc.AddMember( "mapName", rapidjson::Value( ctx.mapName.c_str(), alloc ), alloc );
	doc.AddMember( "gridSize", ctx.gridSize, alloc );

	rapidjson::Value camObj( rapidjson::kObjectType );
	writeVec3( camObj, alloc, "origin", ctx.camera.origin );
	writeVec3( camObj, alloc, "angles", ctx.camera.angles );
	writeVec3( camObj, alloc, "viewVector", ctx.camera.viewVector );
	doc.AddMember( "camera", camObj, alloc );

	rapidjson::Value cursorObj( rapidjson::kObjectType );
	writeVec3( cursorObj, alloc, "position", ctx.cursorOrFocal );
	doc.AddMember( "cursorOrFocal", cursorObj, alloc );

	rapidjson::Value selBoundsObj( rapidjson::kObjectType );
	writeVec3( selBoundsObj, alloc, "origin", ctx.selectionBounds.origin );
	writeVec3( selBoundsObj, alloc, "extents", ctx.selectionBounds.extents );
	doc.AddMember( "selectionBounds", selBoundsObj, alloc );

	rapidjson::Value selectedArr( rapidjson::kArrayType );
	for ( const auto& item : ctx.selectedItems ) {
		rapidjson::Value itemObj( rapidjson::kObjectType );
		itemObj.AddMember( "classname", rapidjson::Value( item.classname.c_str(), alloc ), alloc );
		if ( !item.modelPath.empty() )
			itemObj.AddMember( "modelPath", rapidjson::Value( item.modelPath.c_str(), alloc ), alloc );
		writeVec3( itemObj, alloc, "position", item.position );
		writeVec3( itemObj, alloc, "angles", item.angles );
		writeVec3( itemObj, alloc, "scale", item.scale );
		rapidjson::Value kvArr( rapidjson::kArrayType );
		for ( const auto& kv : item.keyvalues ) {
			rapidjson::Value kvObj( rapidjson::kObjectType );
			kvObj.AddMember( "key", rapidjson::Value( kv.first.c_str(), alloc ), alloc );
			kvObj.AddMember( "value", rapidjson::Value( kv.second.c_str(), alloc ), alloc );
			kvArr.PushBack( kvObj, alloc );
		}
		itemObj.AddMember( "keyvalues", kvArr, alloc );
		selectedArr.PushBack( itemObj, alloc );
	}
	doc.AddMember( "selectedItems", selectedArr, alloc );

	rapidjson::Value nearbyArr( rapidjson::kArrayType );
	for ( const auto& item : ctx.nearbyEntities ) {
		rapidjson::Value itemObj( rapidjson::kObjectType );
		itemObj.AddMember( "classname", rapidjson::Value( item.classname.c_str(), alloc ), alloc );
		if ( !item.modelPath.empty() )
			itemObj.AddMember( "modelPath", rapidjson::Value( item.modelPath.c_str(), alloc ), alloc );
		writeVec3( itemObj, alloc, "position", item.position );
		nearbyArr.PushBack( itemObj, alloc );
	}
	doc.AddMember( "nearbyEntities", nearbyArr, alloc );

	rapidjson::Value allowedModelsArr( rapidjson::kArrayType );
	for ( const auto& s : ctx.allowedModels )
		allowedModelsArr.PushBack( rapidjson::Value( s.c_str(), alloc ), alloc );
	doc.AddMember( "allowedModels", allowedModelsArr, alloc );

	rapidjson::Value allowedEntitiesArr( rapidjson::kArrayType );
	for ( const auto& s : ctx.allowedEntities )
		allowedEntitiesArr.PushBack( rapidjson::Value( s.c_str(), alloc ), alloc );
	doc.AddMember( "allowedEntities", allowedEntitiesArr, alloc );

	rapidjson::StringBuffer buffer;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer( buffer );
	doc.Accept( writer );
	return std::string( buffer.GetString() );
}

// --- Parse PlacementPlan from JSON ---

static bool parseVec3( const rapidjson::Value& v, Vector3Json& out ){
	if ( !v.IsArray() || v.Size() < 3 ) return false;
	out.x = v[0].GetDouble();
	out.y = v[1].GetDouble();
	out.z = v[2].GetDouble();
	return true;
}

static bool parsePlacementPlan( const char* json, size_t len, PlacementPlanJson& plan ){
	rapidjson::Document doc;
	doc.Parse( json, len );
	if ( doc.HasParseError() || !doc.IsObject() ) return false;

	const rapidjson::Value* actionsVal = nullptr;
	if ( doc.HasMember( "placement_plan" ) )
		actionsVal = &doc["placement_plan"];
	else if ( doc.HasMember( "actions" ) )
		actionsVal = &doc["actions"];
	else
		return false;

	if ( !actionsVal->IsArray() ) return false;

	plan.actions.clear();
	for ( rapidjson::SizeType i = 0; i < actionsVal->Size(); ++i ) {
		const rapidjson::Value& a = ( *actionsVal )[i];
		if ( !a.IsObject() ) continue;
		PlacementActionJson act;
		act.action = a.HasMember( "action" ) ? a["action"].GetString() : "place_entity";
		act.classname = a.HasMember( "classname" ) ? a["classname"].GetString() : "";
		act.modelPath = a.HasMember( "modelPath" ) ? a["modelPath"].GetString() : "";
		act.confidence = a.HasMember( "confidence" ) ? a["confidence"].GetDouble() : 1.0;
		act.reason = a.HasMember( "reason" ) ? a["reason"].GetString() : "";
		act.position = act.angles = act.scale = { 0, 0, 0 };
		act.scale = { 1, 1, 1 };
		if ( a.HasMember( "position" ) ) parseVec3( a["position"], act.position );
		if ( a.HasMember( "angles" ) ) parseVec3( a["angles"], act.angles );
		if ( a.HasMember( "scale" ) ) parseVec3( a["scale"], act.scale );
		if ( a.HasMember( "keyvalues" ) && a["keyvalues"].IsArray() ) {
			for ( rapidjson::SizeType k = 0; k < a["keyvalues"].Size(); ++k ) {
				const auto& kv = a["keyvalues"][k];
				if ( kv.IsObject() && kv.HasMember( "key" ) && kv.HasMember( "value" ) )
					act.keyvalues.emplace_back( kv["key"].GetString(), kv["value"].GetString() );
			}
		}
		plan.actions.push_back( act );
	}
	if ( doc.HasMember( "summary" ) && doc["summary"].IsString() )
		plan.summary = doc["summary"].GetString();
	return true;
}

// --- Validation ---

struct ValidationResult {
	bool valid;
	std::string reason;
};

ValidationResult validatePlacementAction( const PlacementActionJson& act, const EditorContextJson& ctx ){
	if ( act.action == "reject" ) return { true, "rejected by AI" };
	if ( act.confidence < 0.3 ) return { false, "confidence too low" };

	if ( act.action == "place_entity" || act.action == "place_model" ) {
		if ( act.classname.empty() ) return { false, "missing classname" };
		bool found = false;
		for ( const auto& e : ctx.allowedEntities ) {
			if ( e == act.classname ) { found = true; break; }
		}
		if ( !found ) return { false, "classname not in allowed list" };

		EntityClass* eclass = GlobalEntityClassManager().findOrInsert( act.classname.c_str(), false );
		if ( eclass->unknown ) return { false, "unknown entity class" };

		if ( eclass->miscmodel_is ) {
			const char* modelKey = eclass->miscmodel_key();
			if ( !string_empty( modelKey ) ) {
				const char* modelPath = act.modelPath.empty() ? nullptr : act.modelPath.c_str();
				if ( string_empty( modelPath ) ) return { false, "model entity requires modelPath" };
			}
		}
	}
	return { true, "" };
}

// --- Execution ---

static double snapToGrid( double v, double grid ){
	if ( grid <= 0 ) return v;
	return std::floor( v / grid + 0.5 ) * grid;
}

bool executePlacementAction( const PlacementActionJson& act ){
	if ( act.action == "reject" ) return true;

	EntityClass* eclass = GlobalEntityClassManager().findOrInsert( act.classname.c_str(), false );
	if ( eclass->unknown ) return false;

	double grid = GetGridSize() > 0 ? static_cast<double>( GetGridSize() ) : 8.0;
	Vector3 origin(
		snapToGrid( act.position.x, grid ),
		snapToGrid( act.position.y, grid ),
		snapToGrid( act.position.z, grid )
	);

	UndoableCommand undo( "aiPlacement" );

	NodeSmartReference node( GlobalEntityCreator().createEntity( eclass ) );
	Node_getTraversable( GlobalSceneGraph().root() )->insert( node );

	scene::Path entitypath( makeReference( GlobalSceneGraph().root() ) );
	entitypath.push( makeReference( node.get() ) );
	scene::Instance& instance = findInstance( entitypath );

	Entity* entity = Node_getEntity( node );

	if ( Transformable* transform = Instance_getTransformable( instance ) ) {
		transform->setType( TRANSFORM_PRIMITIVE );
		transform->setTranslation( origin );
		transform->freezeTransform();
	}

	GlobalSelectionSystem().setSelectedAll( false );
	Instance_setSelected( instance, true );

	if ( eclass->miscmodel_is && !act.modelPath.empty() ) {
		const char* key = eclass->miscmodel_key();
		if ( !string_empty( key ) )
			entity->setKeyValue( key, act.modelPath.c_str() );
	}

	if ( act.angles.x != 0 || act.angles.y != 0 || act.angles.z != 0 ) {
		char buf[64];
		std::snprintf( buf, sizeof( buf ), "%g %g %g", act.angles.x, act.angles.y, act.angles.z );
		entity->setKeyValue( "angles", buf );
	}
	if ( act.scale.x != 1 || act.scale.y != 1 || act.scale.z != 1 ) {
		char buf[64];
		std::snprintf( buf, sizeof( buf ), "%g", act.scale.x );
		entity->setKeyValue( "modelscale", buf );
	}
	for ( const auto& kv : act.keyvalues ) {
		if ( !kv.first.empty() && kv.first != "classname" )
			entity->setKeyValue( kv.first.c_str(), kv.second.c_str() );
	}

	SceneChangeNotify();
	return true;
}

// --- Provider: build request body for OpenAI/Gemini style ---

static std::string buildOpenAIRequest( const std::string& systemPrompt, const std::string& userPrompt, const std::string& contextJson, const std::string& model ){
	rapidjson::Document doc( rapidjson::kObjectType );
	auto& alloc = doc.GetAllocator();

	rapidjson::Value messagesArr( rapidjson::kArrayType );
	{
		rapidjson::Value sysMsg( rapidjson::kObjectType );
		sysMsg.AddMember( "role", "system", alloc );
		sysMsg.AddMember( "content", rapidjson::Value( systemPrompt.c_str(), alloc ), alloc );
		messagesArr.PushBack( sysMsg, alloc );
	}
	{
		rapidjson::Value userMsg( rapidjson::kObjectType );
		userMsg.AddMember( "role", "user", alloc );
		std::string content = userPrompt;
		content += "\n\nEditor context (JSON):\n";
		content += contextJson;
		userMsg.AddMember( "content", rapidjson::Value( content.c_str(), alloc ), alloc );
		messagesArr.PushBack( userMsg, alloc );
	}
	doc.AddMember( "messages", messagesArr, alloc );
	doc.AddMember( "model", rapidjson::Value( model.c_str(), alloc ), alloc );
	doc.AddMember( "temperature", 0.3, alloc );

	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer( buffer );
	doc.Accept( writer );
	return std::string( buffer.GetString() );
}

static std::string buildGeminiRequest( const std::string& systemPrompt, const std::string& userPrompt, const std::string& contextJson ){
	rapidjson::Document doc( rapidjson::kObjectType );
	auto& alloc = doc.GetAllocator();

	std::string userContent = userPrompt;
	userContent += "\n\nEditor context (JSON):\n";
	userContent += contextJson;

	rapidjson::Value contentsArr( rapidjson::kArrayType );
	{
		rapidjson::Value contentObj( rapidjson::kObjectType );
		rapidjson::Value partsArr( rapidjson::kArrayType );
		{
			rapidjson::Value partObj( rapidjson::kObjectType );
			partObj.AddMember( "text", rapidjson::Value( userContent.c_str(), alloc ), alloc );
			partsArr.PushBack( partObj, alloc );
		}
		contentObj.AddMember( "parts", partsArr, alloc );
		contentsArr.PushBack( contentObj, alloc );
	}
	doc.AddMember( "contents", contentsArr, alloc );

	rapidjson::Value sysInstr( rapidjson::kObjectType );
	rapidjson::Value sysParts( rapidjson::kArrayType );
	{
		rapidjson::Value partObj( rapidjson::kObjectType );
		partObj.AddMember( "text", rapidjson::Value( systemPrompt.c_str(), alloc ), alloc );
		sysParts.PushBack( partObj, alloc );
	}
	sysInstr.AddMember( "parts", sysParts, alloc );
	doc.AddMember( "systemInstruction", sysInstr, alloc );

	rapidjson::Value genConfig( rapidjson::kObjectType );
	genConfig.AddMember( "temperature", 0.3, alloc );
	doc.AddMember( "generationConfig", genConfig, alloc );

	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer( buffer );
	doc.Accept( writer );
	return std::string( buffer.GetString() );
}

static const char* systemPromptPlacement = R"(
You are an AI assistant for a 3D level editor (Radiant). You receive editor context as JSON and must respond with a JSON placement plan.

Response format (strict JSON):
{
  "placement_plan": [
    {
      "action": "place_entity" | "place_model" | "place_light" | "reject",
      "classname": "misc_model" | "light" | etc,
      "modelPath": "models/props/chair.ase" (only for misc_model),
      "position": [x, y, z],
      "angles": [pitch, yaw, roll],
      "scale": [x, y, z],
      "keyvalues": [{"key":"k","value":"v"}],
      "confidence": 0.0-1.0,
      "reason": "brief explanation"
    }
  ],
  "summary": "short description"
}

Rules:
- Use only classnames and modelPaths from allowedEntities/allowedModels in the context.
- Position in editor units (typically 1 unit = 1 inch).
- Snap positions to grid (gridSize in context).
- For "place_model" use classname "misc_model" and set modelPath.
- If confidence < 0.3, use action "reject" with reason.
- Return only valid JSON, no markdown.
)";

// --- UI ---

QPlainTextEdit* g_aiPromptEdit{};
QPlainTextEdit* g_aiResponseEdit{};
QPlainTextEdit* g_aiLogEdit{};
QLabel* g_aiStatusLabel{};
QComboBox* g_aiAgentCombo{};
QLineEdit* g_aiEndpointEdit{};
QLineEdit* g_aiModelEdit{};
QLineEdit* g_aiKeyEnvEdit{};
QLineEdit* g_aiApiKeyEdit{};
QCheckBox* g_aiUseEnvVarCheck{};
QPushButton* g_aiAddAgentBtn{};
QPushButton* g_aiRemoveAgentBtn{};
QCheckBox* g_aiIncludeSelection{};
QCheckBox* g_aiIncludeNearby{};
QCheckBox* g_aiDryRunCheck{};
QListWidget* g_aiPlanList{};
QPushButton* g_aiApplySelectedBtn{};
QPushButton* g_aiApplyAllBtn{};

PlacementPlanJson g_aiLastPlan;

void AIAssistant_appendLog( const QString& text ){
	if ( g_aiLogEdit ) {
		g_aiLogEdit->appendPlainText( text );
		g_aiLogEdit->verticalScrollBar()->setValue( g_aiLogEdit->verticalScrollBar()->maximum() );
	}
}

void AIAssistant_setStatus( const QString& text ){
	if ( g_aiStatusLabel ) g_aiStatusLabel->setText( text );
}

void AIAssistant_handleMockResponse( const EditorContextJson& ctx ){
	PlacementPlanJson plan;
	PlacementActionJson act;
	act.action = "place_entity";
	act.classname = "light";
	act.modelPath = "";
	act.position = ctx.camera.origin;
	act.position.y += 64;
	act.angles = { 0, 0, 0 };
	act.scale = { 1, 1, 1 };
	act.confidence = 0.8;
	act.reason = "Mock: light at camera + 64";
	plan.actions.push_back( act );
	plan.summary = "Mock provider: 1 light";

	g_aiLastPlan = plan;
	AIAssistant_setStatus( "Mock plan: " + QString::number( plan.actions.size() ) + " actions" );
	if ( g_aiPlanList ) {
		g_aiPlanList->clear();
		for ( size_t i = 0; i < plan.actions.size(); ++i ) {
			const auto& a = plan.actions[i];
			QString desc = QString::fromUtf8( a.classname.c_str() );
			if ( !a.modelPath.empty() ) desc += " @ " + QString::fromUtf8( a.modelPath.c_str() );
			desc += QString( " (%.0f%%)" ).arg( a.confidence * 100 );
			QListWidgetItem* item = new QListWidgetItem( desc );
			item->setData( Qt::UserRole, static_cast<int>( i ) );
			g_aiPlanList->addItem( item );
		}
	}
	if ( g_aiResponseEdit )
		g_aiResponseEdit->setPlainText( "{\"placement_plan\":[{\"action\":\"place_entity\",\"classname\":\"light\",\"position\":[0,0,0],\"confidence\":0.8}],\"summary\":\"Mock\"}" );
	AIAssistant_appendLog( "[INFO] Mock provider returned " + QString::number( plan.actions.size() ) + " actions" );
}

void AIAssistant_refreshAgentCombo(){
	if ( !g_aiAgentCombo ) return;
	g_aiAgentCombo->clear();
	for ( const auto& a : g_aiAssistantAgents )
		g_aiAgentCombo->addItem( a.name.c_str() );
	int idx = g_aiAgentCombo->findText( g_aiAssistantActiveAgent.c_str() );
	g_aiAgentCombo->setCurrentIndex( idx >= 0 ? idx : 0 );
}

void AIAssistant_onAgentChanged(){
	QString name = g_aiAgentCombo ? g_aiAgentCombo->currentText() : "";
	g_aiAssistantActiveAgent = name.toLatin1().constData();
	for ( const auto& a : g_aiAssistantAgents ) {
		if ( a.name == g_aiAssistantActiveAgent.c_str() ) {
			if ( g_aiEndpointEdit ) g_aiEndpointEdit->setText( a.endpoint.c_str() );
			if ( g_aiModelEdit ) g_aiModelEdit->setText( a.model.c_str() );
			if ( g_aiKeyEnvEdit ) g_aiKeyEnvEdit->setText( a.keyEnvVar.c_str() );
			if ( g_aiApiKeyEdit ) g_aiApiKeyEdit->setText( a.apiKey.c_str() );
			if ( g_aiUseEnvVarCheck ) g_aiUseEnvVarCheck->setChecked( a.useEnvVar );
			if ( g_aiKeyEnvEdit ) g_aiKeyEnvEdit->setEnabled( a.useEnvVar );
			if ( g_aiApiKeyEdit ) {
				g_aiApiKeyEdit->setEnabled( !a.useEnvVar );
				g_aiApiKeyEdit->setEchoMode( QLineEdit::Password );
			}
			return;
		}
	}
}

void AIAssistant_saveCurrentAgent(){
	QString name = g_aiAgentCombo ? g_aiAgentCombo->currentText() : "";
	for ( auto& a : g_aiAssistantAgents ) {
		if ( a.name == name.toLatin1().constData() ) {
			if ( g_aiEndpointEdit ) a.endpoint = g_aiEndpointEdit->text().trimmed().toLatin1().constData();
			if ( g_aiModelEdit ) a.model = g_aiModelEdit->text().trimmed().toLatin1().constData();
			if ( g_aiKeyEnvEdit ) a.keyEnvVar = g_aiKeyEnvEdit->text().trimmed().toLatin1().constData();
			if ( g_aiApiKeyEdit ) a.apiKey = g_aiApiKeyEdit->text().trimmed().toLatin1().constData();
			if ( g_aiUseEnvVarCheck ) a.useEnvVar = g_aiUseEnvVarCheck->isChecked();
			return;
		}
	}
}

void AIAssistant_sendRequest(){
	if ( !Map_Valid( g_map ) ) {
		AIAssistant_setStatus( "No map loaded" );
		return;
	}
	if ( !g_aiAssistantEnabled ) {
		AIAssistant_setStatus( "AI Assistant is disabled in Preferences" );
		return;
	}

	AIAssistant_saveCurrentAgent();
	EditorContextJson ctx;
	AIAssistant_extractContext( ctx );

	const AIAssistantAgentConfig* agent = getActiveAgent();
	if ( !agent ) {
		AIAssistant_setStatus( "No agent configured" );
		return;
	}
	if ( agent->provider == "Mock" ) {
		AIAssistant_setStatus( "Mock provider" );
		AIAssistant_handleMockResponse( ctx );
		return;
	}

	if ( !g_aiNetworkManager ) g_aiNetworkManager = new QNetworkAccessManager();

	const char* apiKey = getApiKeyForAgent( agent );
	if ( !apiKey || !*apiKey ) {
		QString msg = agent->useEnvVar
			? QString( "API key not set. Set env var: " ) + agent->keyEnvVar.c_str()
			: "API key not set. Enter key in Preferences or dock.";
		AIAssistant_setStatus( msg );
		AIAssistant_appendLog( "[ERROR] " + msg );
		return;
	}

	std::string contextJson = EditorContext_toJson( ctx );

	QString userPrompt = g_aiPromptEdit ? g_aiPromptEdit->toPlainText().trimmed() : "Suggest placement for the current selection.";
	if ( userPrompt.isEmpty() ) userPrompt = "Suggest 2-3 props to place near the camera or selection. Use only allowed entities.";

	const bool isGemini = ( agent->provider == "Gemini" );
	QString endpoint = agent->endpoint.c_str();
	if ( isGemini && endpoint.isEmpty() )
		endpoint = "https://generativelanguage.googleapis.com/v1beta/models/gemini-1.5-flash:generateContent";
	std::string body;
	if ( isGemini )
		body = buildGeminiRequest( systemPromptPlacement, userPrompt.toUtf8().constData(), contextJson );
	else
		body = buildOpenAIRequest( systemPromptPlacement, userPrompt.toUtf8().constData(), contextJson, agent->model.c_str() );

	QNetworkRequest req{ QUrl( endpoint ) };
	req.setHeader( QNetworkRequest::ContentTypeHeader, "application/json" );
	if ( isGemini )
		req.setRawHeader( "x-goog-api-key", apiKey );
	else
		req.setRawHeader( "Authorization", ( "Bearer " + QByteArray( apiKey ) ) );

	AIAssistant_setStatus( "Sending request..." );
	AIAssistant_appendLog( "[INFO] " + QDateTime::currentDateTime().toString( Qt::ISODate ) + " Request to " + endpoint );

	QElapsedTimer timer;
	timer.start();

	const QByteArray requestBody = QByteArray::fromStdString( body );
	QNetworkReply* reply = g_aiNetworkManager->post( req, requestBody );
	QObject::connect( reply, &QNetworkReply::finished, [reply, timer, isGemini](){
		if ( reply->error() != QNetworkReply::NoError ) {
			AIAssistant_setStatus( "Error: " + reply->errorString() );
			AIAssistant_appendLog( "[ERROR] " + reply->errorString() );
			reply->deleteLater();
			return;
		}
		QByteArray data = reply->readAll();
		reply->deleteLater();

		const qint64 ms = timer.elapsed();
		AIAssistant_appendLog( "[INFO] Response received in " + QString::number( ms ) + " ms" );

		// Parse content from OpenAI or Gemini response
		QJsonDocument jdoc = QJsonDocument::fromJson( data );
		QString content;
		if ( jdoc.isObject() ) {
			QJsonObject obj = jdoc.object();
			if ( isGemini && obj.contains( "candidates" ) && obj["candidates"].isArray() ) {
				QJsonArray candidates = obj["candidates"].toArray();
				if ( !candidates.isEmpty() ) {
					QJsonObject cand = candidates[0].toObject();
					if ( cand.contains( "content" ) ) {
						QJsonObject c = cand["content"].toObject();
						if ( c.contains( "parts" ) && c["parts"].isArray() ) {
							QJsonArray parts = c["parts"].toArray();
							if ( !parts.isEmpty() && parts[0].isObject() && parts[0].toObject().contains( "text" ) )
								content = parts[0].toObject()["text"].toString();
						}
					}
				}
			} else if ( !isGemini && obj.contains( "choices" ) && obj["choices"].isArray() ) {
				QJsonArray choices = obj["choices"].toArray();
				if ( !choices.isEmpty() ) {
					QJsonObject choice = choices[0].toObject();
					if ( choice.contains( "message" ) ) {
						QJsonObject msg = choice["message"].toObject();
						if ( msg.contains( "content" ) )
							content = msg["content"].toString();
					}
				}
			}
		}
		if ( content.isEmpty() ) {
			AIAssistant_setStatus( "No content in response" );
			AIAssistant_appendLog( "[ERROR] Could not parse response" );
			return;
		}

		if ( g_aiResponseEdit ) g_aiResponseEdit->setPlainText( content );

		PlacementPlanJson plan;
		QByteArray utf8 = content.toUtf8();
		if ( parsePlacementPlan( utf8.constData(), utf8.size(), plan ) ) {
			g_aiLastPlan = plan;
			AIAssistant_setStatus( "Plan parsed: " + QString::number( plan.actions.size() ) + " actions" );
			if ( g_aiPlanList ) {
				g_aiPlanList->clear();
				for ( size_t i = 0; i < plan.actions.size(); ++i ) {
					const auto& a = plan.actions[i];
					QString desc = QString::fromUtf8( a.classname.c_str() );
					if ( !a.modelPath.empty() ) desc += " @ " + QString::fromUtf8( a.modelPath.c_str() );
					desc += QString( " (%.0f%%)" ).arg( a.confidence * 100 );
					QListWidgetItem* item = new QListWidgetItem( desc );
					item->setData( Qt::UserRole, static_cast<int>( i ) );
					g_aiPlanList->addItem( item );
				}
			}
		} else {
			AIAssistant_setStatus( "Could not parse placement plan" );
			AIAssistant_appendLog( "[WARN] Response is not valid placement plan JSON" );
		}
	} );
}

void AIAssistant_applySelected(){
	EditorContextJson ctx;
	AIAssistant_extractContext( ctx );
	QList<QListWidgetItem*> sel = g_aiPlanList ? g_aiPlanList->selectedItems() : QList<QListWidgetItem*>();
	int applied = 0;
	for ( QListWidgetItem* item : sel ) {
		int idx = item->data( Qt::UserRole ).toInt();
		if ( idx >= 0 && idx < static_cast<int>( g_aiLastPlan.actions.size() ) ) {
			const auto& act = g_aiLastPlan.actions[idx];
			ValidationResult vr = validatePlacementAction( act, ctx );
			if ( vr.valid && act.action != "reject" ) {
				if ( executePlacementAction( act ) ) ++applied;
			} else {
				AIAssistant_appendLog( "[WARN] Skipped action " + QString::number( idx ) + ": " + QString::fromUtf8( vr.reason.c_str() ) );
			}
		}
	}
	AIAssistant_setStatus( "Applied " + QString::number( applied ) + " actions" );
	AIAssistant_appendLog( "[INFO] Applied " + QString::number( applied ) + " selected actions" );
}

void AIAssistant_applyAll(){
	EditorContextJson ctx;
	AIAssistant_extractContext( ctx );
	int applied = 0;
	for ( const auto& act : g_aiLastPlan.actions ) {
		ValidationResult vr = validatePlacementAction( act, ctx );
		if ( vr.valid && act.action != "reject" ) {
			if ( executePlacementAction( act ) ) ++applied;
		}
	}
	AIAssistant_setStatus( "Applied " + QString::number( applied ) + " actions" );
	AIAssistant_appendLog( "[INFO] Applied " + QString::number( applied ) + " actions" );
}

// --- Image Generation ---

static void AIImage_setStatus( const QString& s ){
	if ( g_aiImageStatusLabel ) g_aiImageStatusLabel->setText( s );
}

static QString slugFromPrompt( const QString& prompt ){
	QString s = prompt.trimmed().left( 32 ).toLower();
	for ( int i = 0; i < s.size(); ++i )
		if ( !s[i].isLetterOrNumber() ) s[i] = '_';
	return s.isEmpty() ? "ai_gen" : s;
}

static void AIImage_onIrisFinished( int exitCode, QProcess::ExitStatus status ){
	if ( g_aiImageProcess ) g_aiImageProcess->deleteLater();
	g_aiImageProcess = nullptr;
	if ( g_aiImageGenerateBtn ) g_aiImageGenerateBtn->setEnabled( true );

	QString outPath = g_aiImageLastPath;
	if ( status != QProcess::NormalExit || exitCode != 0 ) {
		AIImage_setStatus( "iris failed (exit " + QString::number( exitCode ) + ")" );
		return;
	}
	QImage img( outPath );
	if ( img.isNull() ) {
		AIImage_setStatus( "Failed to load generated image" );
		return;
	}
	g_aiImageLastGenerated = img;
	if ( g_aiImagePreviewLabel ) {
		g_aiImagePreviewLabel->setPixmap( QPixmap::fromImage( g_aiImageLastGenerated ).scaled( 256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation ) );
		g_aiImagePreviewLabel->setToolTip( outPath );
	}
	if ( g_aiImageSaveBtn ) g_aiImageSaveBtn->setEnabled( true );
	if ( g_aiImageApplyBtn ) g_aiImageApplyBtn->setEnabled( true );
	AIImage_setStatus( "Generated " + QString::number( img.width() ) + "x" + QString::number( img.height() ) );
}

static void AIImage_generateIris( const QString& prompt, const QString& outPath ){
	QString irisExe = g_aiImageIrisPathEdit ? g_aiImageIrisPathEdit->text().trimmed() : QString::fromUtf8( g_aiImageIrisPath.c_str() );
	QString model = g_aiImageIrisModelEdit ? g_aiImageIrisModelEdit->text().trimmed() : QString::fromUtf8( g_aiImageIrisModel.c_str() );
	if ( irisExe.isEmpty() ) {
		AIImage_setStatus( "Set iris executable path (e.g. /path/to/iris)" );
		QMessageBox::warning( g_aiAssistantDock, "Image Generation", "Configure the iris executable path in the Image Generation tab.\n\nBuild iris from https://github.com/antirez/iris.c" );
		return;
	}
	if ( model.isEmpty() ) model = "flux-klein-4b";

	g_aiImageLastPath = outPath;
	g_aiImageProcess = new QProcess( g_aiAssistantDock );
	QObject::connect( g_aiImageProcess, QOverload<int, QProcess::ExitStatus>::of( &QProcess::finished ), []( int code, QProcess::ExitStatus status ){
		AIImage_onIrisFinished( code, status );
	} );
	QObject::connect( g_aiImageProcess, &QProcess::errorOccurred, []( QProcess::ProcessError err ){
		if ( g_aiImageProcess ) {
			g_aiImageProcess->deleteLater();
			g_aiImageProcess = nullptr;
		}
		if ( g_aiImageGenerateBtn ) g_aiImageGenerateBtn->setEnabled( true );
		AIImage_setStatus( "iris process error" );
	} );
	QStringList args;
	args << "-d" << model << "-p" << prompt << "-o" << outPath;
	g_aiImageProcess->start( irisExe, args );
	if ( !g_aiImageProcess->waitForStarted( 3000 ) ) {
		AIImage_setStatus( "Failed to start iris" );
		g_aiImageProcess->deleteLater();
		g_aiImageProcess = nullptr;
		if ( g_aiImageGenerateBtn ) g_aiImageGenerateBtn->setEnabled( true );
		return;
	}
	AIImage_setStatus( "Generating (iris may take 10–60s)..." );
}

static void AIImage_generateDalle( const QString& prompt ){
	const char* key = getenv( "OPENAI_API_KEY" );
	if ( !g_aiImageDalleApiKey.empty() ) key = g_aiImageDalleApiKey.c_str();
	if ( !key || !*key ) {
		AIImage_setStatus( "Set OPENAI_API_KEY or DALL-E API key in preferences" );
		return;
	}
	if ( g_aiNetworkManager == nullptr )
		g_aiNetworkManager = new QNetworkAccessManager();

	QJsonObject body;
	body["model"] = "dall-e-3";
	body["prompt"] = prompt;
	body["n"] = 1;
	body["size"] = "1024x1024";
	body["response_format"] = "b64_json";
	QNetworkRequest req{ QUrl( "https://api.openai.com/v1/images/generations" ) };
	req.setHeader( QNetworkRequest::ContentTypeHeader, "application/json" );
	req.setRawHeader( "Authorization", QByteArray( "Bearer " ) + key );
	QNetworkReply* reply = g_aiNetworkManager->post( req, QJsonDocument( body ).toJson() );
	QObject::connect( reply, &QNetworkReply::finished, [reply, prompt](){
		reply->deleteLater();
		if ( reply->error() != QNetworkReply::NoError ) {
			AIImage_setStatus( "DALL-E request failed" );
			if ( g_aiImageGenerateBtn ) g_aiImageGenerateBtn->setEnabled( true );
			return;
		}
		QByteArray data = reply->readAll();
		QJsonDocument doc = QJsonDocument::fromJson( data );
		if ( !doc.isObject() ) {
			AIImage_setStatus( "Invalid DALL-E response" );
			if ( g_aiImageGenerateBtn ) g_aiImageGenerateBtn->setEnabled( true );
			return;
		}
		QJsonObject obj = doc.object();
		if ( obj.contains( "error" ) ) {
			QString errMsg = obj["error"].toObject()["message"].toString();
			AIImage_setStatus( "DALL-E: " + ( errMsg.isEmpty() ? "API error" : errMsg ) );
			if ( g_aiImageGenerateBtn ) g_aiImageGenerateBtn->setEnabled( true );
			return;
		}
		QJsonArray dataArr = obj["data"].toArray();
		if ( dataArr.isEmpty() ) {
			AIImage_setStatus( "No image in DALL-E response" );
			if ( g_aiImageGenerateBtn ) g_aiImageGenerateBtn->setEnabled( true );
			return;
		}
		QString b64 = dataArr[0].toObject()["b64_json"].toString();
		if ( b64.isEmpty() ) b64 = dataArr[0].toObject()["url"].toString();
		if ( b64.isEmpty() ) {
			AIImage_setStatus( "No image data in DALL-E response" );
			if ( g_aiImageGenerateBtn ) g_aiImageGenerateBtn->setEnabled( true );
			return;
		}
		QImage img;
		if ( b64.startsWith( "http" ) ) {
			// URL - would need another fetch; for now require b64
			AIImage_setStatus( "DALL-E returned URL; b64_json preferred" );
			if ( g_aiImageGenerateBtn ) g_aiImageGenerateBtn->setEnabled( true );
			return;
		}
		img.loadFromData( QByteArray::fromBase64( b64.toLatin1() ) );
		if ( img.isNull() ) {
			AIImage_setStatus( "Failed to decode DALL-E image" );
			if ( g_aiImageGenerateBtn ) g_aiImageGenerateBtn->setEnabled( true );
			return;
		}
		QTemporaryFile tmp( QDir::temp().filePath( "idtech3radiant-ai-XXXXXX.png" ) );
		tmp.setAutoRemove( false );
		if ( !tmp.open() ) {
			AIImage_setStatus( "Failed to create temp file" );
			if ( g_aiImageGenerateBtn ) g_aiImageGenerateBtn->setEnabled( true );
			return;
		}
		QString path = tmp.fileName();
		tmp.close();
		if ( !img.save( path ) ) {
			AIImage_setStatus( "Failed to save temp image" );
			if ( g_aiImageGenerateBtn ) g_aiImageGenerateBtn->setEnabled( true );
			return;
		}
		g_aiImageLastPath = path;
		g_aiImageLastGenerated = img;
		if ( g_aiImagePreviewLabel ) {
			g_aiImagePreviewLabel->setPixmap( QPixmap::fromImage( g_aiImageLastGenerated ).scaled( 256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation ) );
			g_aiImagePreviewLabel->setToolTip( path );
		}
		if ( g_aiImageSaveBtn ) g_aiImageSaveBtn->setEnabled( true );
		if ( g_aiImageApplyBtn ) g_aiImageApplyBtn->setEnabled( true );
		AIImage_setStatus( "Generated " + QString::number( img.width() ) + "x" + QString::number( img.height() ) );
		if ( g_aiImageGenerateBtn ) g_aiImageGenerateBtn->setEnabled( true );
	} );
	AIImage_setStatus( "Generating (DALL-E)..." );
}

static void AIImage_generate(){
	QString prompt = g_aiImagePromptEdit ? g_aiImagePromptEdit->toPlainText().trimmed() : "";
	if ( prompt.isEmpty() ) {
		AIImage_setStatus( "Enter a prompt" );
		return;
	}
	QString provider = g_aiImageProviderCombo ? g_aiImageProviderCombo->currentText() : "iris";
	if ( g_aiImageGenerateBtn ) g_aiImageGenerateBtn->setEnabled( false );
	if ( g_aiImageSaveBtn ) g_aiImageSaveBtn->setEnabled( false );
	if ( g_aiImageApplyBtn ) g_aiImageApplyBtn->setEnabled( false );

	if ( provider == "iris" ) {
		QTemporaryFile tmp( QDir::temp().filePath( "idtech3radiant-ai-XXXXXX.png" ) );
		tmp.setAutoRemove( false );
		if ( !tmp.open() ) {
			AIImage_setStatus( "Failed to create temp file" );
			if ( g_aiImageGenerateBtn ) g_aiImageGenerateBtn->setEnabled( true );
			return;
		}
		QString path = tmp.fileName();
		tmp.close();
		AIImage_generateIris( prompt, path );
	} else if ( provider == "DALL-E" ) {
		AIImage_generateDalle( prompt );
	} else {
		// Mock: create a placeholder
		QImage img( 512, 512, QImage::Format_RGB32 );
		img.fill( QColor( 80, 80, 120 ) );
		QTemporaryFile tmp( QDir::temp().filePath( "idtech3radiant-ai-XXXXXX.png" ) );
		tmp.setAutoRemove( false );
		if ( tmp.open() ) {
			QString path = tmp.fileName();
			tmp.close();
			img.save( path );
			g_aiImageLastPath = path;
			g_aiImageLastGenerated = img;
			if ( g_aiImagePreviewLabel ) {
				g_aiImagePreviewLabel->setPixmap( QPixmap::fromImage( g_aiImageLastGenerated ).scaled( 256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation ) );
			}
			if ( g_aiImageSaveBtn ) g_aiImageSaveBtn->setEnabled( true );
			if ( g_aiImageApplyBtn ) g_aiImageApplyBtn->setEnabled( true );
		}
		AIImage_setStatus( "Mock placeholder" );
		if ( g_aiImageGenerateBtn ) g_aiImageGenerateBtn->setEnabled( true );
	}
}

static void AIImage_save(){
	if ( g_aiImageLastGenerated.isNull() ) return;
	QString base = g_qeglobals.m_userGamePath.empty()
		? QDir::homePath()
		: QString::fromUtf8( g_qeglobals.m_userGamePath.c_str() );
	QString texturesDir = base + "/textures/ai_gen";
	QDir().mkpath( texturesDir );
	QString slug = slugFromPrompt( g_aiImagePromptEdit ? g_aiImagePromptEdit->toPlainText() : "" );
	QString defPath = texturesDir + "/" + slug + ".tga";
	QString path = QFileDialog::getSaveFileName( g_aiAssistantDock, "Save Generated Image", defPath, "TGA (*.tga);;PNG (*.png);;JPEG (*.jpg)" );
	if ( path.isEmpty() ) return;
	QImage img = g_aiImageLastGenerated;
	if ( !img.save( path ) ) {
		AIImage_setStatus( "Save failed" );
		return;
	}
	g_aiImageLastPath = path;
	// Create minimal shader for Q3 textures only when saving to game directory
	if ( !g_qeglobals.m_userGamePath.empty() ) {
		QString baseNorm = QString::fromUtf8( g_qeglobals.m_userGamePath.c_str() ).replace( '\\', '/' );
		if ( !baseNorm.endsWith( '/' ) ) baseNorm += '/';
		if ( path.replace( '\\', '/' ).startsWith( baseNorm ) ) {
			QString mid = path.mid( baseNorm.length() );
			if ( mid.startsWith( '/' ) ) mid = mid.mid( 1 );
			QString relPath = QString::fromUtf8( CopiedString( PathExtensionless( mid.toLatin1().constData() ) ).c_str() );
			QString scriptsDir = baseNorm + "scripts";
			QDir().mkpath( scriptsDir );
			QString shaderPath = scriptsDir + "/ai_gen.shader";
			QFile shaderFile( shaderPath );
			QString shaderName = relPath;
			QString shaderEntry = "\n" + shaderName + "\n{\n\tqer_editorImage " + relPath + "\n\tmap " + relPath + "\n}\n";
			bool append = true;
			if ( shaderFile.exists() && shaderFile.open( QIODevice::ReadOnly | QIODevice::Text ) ) {
				QString content = QString::fromUtf8( shaderFile.readAll() );
				shaderFile.close();
				if ( content.contains( shaderName ) ) append = false;
			}
			if ( append && shaderFile.open( QIODevice::Append | QIODevice::Text ) ) {
				shaderFile.write( shaderEntry.toUtf8() );
				shaderFile.close();
				GlobalShaderSystem().refresh();
			}
		}
	}
	AIImage_setStatus( "Saved to " + path );
}

static void AIImage_apply(){
	if ( g_aiImageLastPath.isEmpty() ) return;
	if ( g_qeglobals.m_userGamePath.empty() ) {
		AIImage_setStatus( "No game path configured" );
		return;
	}
	QString base = QString::fromUtf8( g_qeglobals.m_userGamePath.c_str() ).replace( '\\', '/' );
	if ( !base.endsWith( '/' ) ) base += '/';
	QString pathNorm = QString( g_aiImageLastPath ).replace( '\\', '/' );
	if ( !pathNorm.startsWith( base ) ) {
		AIImage_setStatus( "Save to textures first, then apply" );
		return;
	}
	QString relPath = pathNorm.mid( base.length() );
	if ( relPath.startsWith( '/' ) ) relPath = relPath.mid( 1 );
	CopiedString shaderNameStr( PathExtensionless( relPath.toLatin1().constData() ) );
	QString shaderName = QString::fromUtf8( shaderNameStr.c_str() );
	if ( !string_equal_prefix_nocase( shaderNameStr.c_str(), "textures/" ) )
		shaderName = "textures/" + shaderName;
	if ( GlobalSelectionSystem().countSelected() == 0 ) {
		AIImage_setStatus( "Select faces to apply texture" );
		return;
	}
	UndoableCommand undo( "textureSet" );
	Select_SetShader_Undo( shaderName.toLatin1().constData() );
	AIImage_setStatus( "Applied " + shaderName );
}

} // namespace

void AIAssistant_createDock( QMainWindow* window ){
	if ( window == nullptr || g_aiAssistantDock != nullptr ) return;

	g_aiAssistantDock = new QDockWidget( "AI Assistant", window );
	g_aiAssistantDock->setObjectName( "dock_ai_assistant" );

	auto* tabs = new QTabWidget( g_aiAssistantDock );
	auto* placementTab = new QWidget( tabs );
	auto* vbox = new QVBoxLayout( placementTab );

	// Agent
	ensureDefaultAgents();
	auto* providerGroup = new QGroupBox( "Agent", placementTab );
	auto* providerForm = new QFormLayout( providerGroup );
	auto* agentRow = new QHBoxLayout();
	g_aiAgentCombo = new QComboBox( placementTab );
	AIAssistant_refreshAgentCombo();
	agentRow->addWidget( g_aiAgentCombo );
	g_aiAddAgentBtn = new QPushButton( "+", placementTab );
	g_aiAddAgentBtn->setMaximumWidth( 28 );
	g_aiRemoveAgentBtn = new QPushButton( "-", placementTab );
	g_aiRemoveAgentBtn->setMaximumWidth( 28 );
	agentRow->addWidget( g_aiAddAgentBtn );
	agentRow->addWidget( g_aiRemoveAgentBtn );
	providerForm->addRow( "Agent", agentRow );
	g_aiEndpointEdit = new QLineEdit( placementTab );
	g_aiEndpointEdit->setPlaceholderText( "https://api.openai.com/v1/chat/completions" );
	g_aiModelEdit = new QLineEdit( placementTab );
	g_aiModelEdit->setPlaceholderText( "gpt-4o-mini" );
	g_aiUseEnvVarCheck = new QCheckBox( "Use environment variable for API key", placementTab );
	g_aiUseEnvVarCheck->setChecked( true );
	g_aiKeyEnvEdit = new QLineEdit( placementTab );
	g_aiKeyEnvEdit->setPlaceholderText( "OPENAI_API_KEY" );
	g_aiApiKeyEdit = new QLineEdit( placementTab );
	g_aiApiKeyEdit->setPlaceholderText( "API key (stored in preferences)" );
	g_aiApiKeyEdit->setEchoMode( QLineEdit::Password );
	g_aiApiKeyEdit->setEnabled( false );
	providerForm->addRow( "Endpoint", g_aiEndpointEdit );
	providerForm->addRow( "Model", g_aiModelEdit );
	providerForm->addRow( "", g_aiUseEnvVarCheck );
	providerForm->addRow( "Env var name", g_aiKeyEnvEdit );
	providerForm->addRow( "API key (direct)", g_aiApiKeyEdit );
	QObject::connect( g_aiAgentCombo, QOverload<int>::of( &QComboBox::currentIndexChanged ), []( int ){
		AIAssistant_saveCurrentAgent();
		AIAssistant_onAgentChanged();
		Preferences_Save();
	} );
	QObject::connect( g_aiUseEnvVarCheck, &QCheckBox::toggled, []( bool useEnv ){
		if ( g_aiKeyEnvEdit ) g_aiKeyEnvEdit->setEnabled( useEnv );
		if ( g_aiApiKeyEdit ) g_aiApiKeyEdit->setEnabled( !useEnv );
	} );
	QObject::connect( g_aiAddAgentBtn, &QPushButton::clicked, [](){
		AIAssistant_saveCurrentAgent();
		QString name = QInputDialog::getText( g_aiAssistantDock, "Add Agent", "Agent name:", QLineEdit::Normal, "New Agent" );
		if ( name.isEmpty() ) return;
		QStringList providers = { "OpenAI", "Gemini", "Mock" };
		QString prov = QInputDialog::getItem( g_aiAssistantDock, "Add Agent", "Provider:", providers, 0, false );
		if ( prov.isEmpty() ) return;
		AIAssistantAgentConfig a;
		a.name = name.trimmed().toLatin1().constData();
		a.provider = prov.toLatin1().constData();
		if ( prov == "Gemini" ) {
			a.endpoint = "https://generativelanguage.googleapis.com/v1beta/models/gemini-1.5-flash:generateContent";
			a.model = "gemini-1.5-flash";
			a.keyEnvVar = "GEMINI_API_KEY";
		} else if ( prov == "Mock" ) {
			a.endpoint = "";
			a.model = "";
		} else {
			a.endpoint = "https://api.openai.com/v1/chat/completions";
			a.model = "gpt-4o-mini";
			a.keyEnvVar = "OPENAI_API_KEY";
		}
		a.useEnvVar = true;
		g_aiAssistantAgents.push_back( a );
		AIAssistant_refreshAgentCombo();
		g_aiAgentCombo->setCurrentText( a.name.c_str() );
		AIAssistant_onAgentChanged();
		Preferences_Save();
	} );
	QObject::connect( g_aiRemoveAgentBtn, &QPushButton::clicked, [](){
		QString name = g_aiAgentCombo ? g_aiAgentCombo->currentText() : "";
		if ( name.isEmpty() ) return;
		auto it = std::remove_if( g_aiAssistantAgents.begin(), g_aiAssistantAgents.end(),
			[&name]( const AIAssistantAgentConfig& a ){ return a.name == name.toLatin1().constData(); } );
		if ( it != g_aiAssistantAgents.end() ) {
			g_aiAssistantAgents.erase( it, g_aiAssistantAgents.end() );
			AIAssistant_refreshAgentCombo();
			Preferences_Save();
		}
	} );
	AIAssistant_onAgentChanged();
	vbox->addWidget( providerGroup );

	// Prompt
	auto* promptGroup = new QGroupBox( "Prompt", placementTab );
	auto* promptLayout = new QVBoxLayout( promptGroup );
	g_aiPromptEdit = new QPlainTextEdit( placementTab );
	g_aiPromptEdit->setPlaceholderText( "e.g. Place 3 props near this wall. Suggest cover objects." );
	g_aiPromptEdit->setMaximumHeight( 80 );
	g_aiIncludeSelection = new QCheckBox( "Include selection in context", placementTab );
	g_aiIncludeSelection->setChecked( true );
	g_aiIncludeNearby = new QCheckBox( "Include nearby entities", placementTab );
	g_aiIncludeNearby->setChecked( true );
	g_aiDryRunCheck = new QCheckBox( "Dry run (preview only)", placementTab );
	g_aiDryRunCheck->setChecked( true );
	promptLayout->addWidget( g_aiPromptEdit );
	promptLayout->addWidget( g_aiIncludeSelection );
	promptLayout->addWidget( g_aiIncludeNearby );
	promptLayout->addWidget( g_aiDryRunCheck );
	vbox->addWidget( promptGroup );

	// Buttons
	auto* btnRow = new QHBoxLayout();
	auto* sendBtn = new QPushButton( "Send Request", placementTab );
	auto* applySelectedBtn = new QPushButton( "Apply Selected", placementTab );
	auto* applyAllBtn = new QPushButton( "Apply All", placementTab );
	QObject::connect( sendBtn, &QPushButton::clicked, [](){ AIAssistant_sendRequest(); } );
	QObject::connect( applySelectedBtn, &QPushButton::clicked, [](){ AIAssistant_applySelected(); } );
	QObject::connect( applyAllBtn, &QPushButton::clicked, [](){ AIAssistant_applyAll(); } );
	g_aiApplySelectedBtn = applySelectedBtn;
	g_aiApplyAllBtn = applyAllBtn;
	btnRow->addWidget( sendBtn );
	btnRow->addWidget( applySelectedBtn );
	btnRow->addWidget( applyAllBtn );
	vbox->addLayout( btnRow );

	// Plan list
	g_aiPlanList = new QListWidget( placementTab );
	g_aiPlanList->setSelectionMode( QAbstractItemView::ExtendedSelection );
	g_aiPlanList->setMaximumHeight( 80 );
	vbox->addWidget( new QLabel( "Placement plan:", placementTab ) );
	vbox->addWidget( g_aiPlanList );

	// Response
	g_aiResponseEdit = new QPlainTextEdit( placementTab );
	g_aiResponseEdit->setReadOnly( true );
	g_aiResponseEdit->setMaximumHeight( 120 );
	vbox->addWidget( new QLabel( "Response:", placementTab ) );
	vbox->addWidget( g_aiResponseEdit );

	// Log
	g_aiLogEdit = new QPlainTextEdit( placementTab );
	g_aiLogEdit->setReadOnly( true );
	g_aiLogEdit->setMaximumHeight( 80 );
	vbox->addWidget( new QLabel( "Log:", placementTab ) );
	vbox->addWidget( g_aiLogEdit );

	g_aiStatusLabel = new QLabel( "Ready", placementTab );
	vbox->addWidget( g_aiStatusLabel );

	tabs->addTab( placementTab, "Placement" );

	// Image Generation tab
	auto* imgTab = new QWidget( tabs );
	auto* imgVbox = new QVBoxLayout( imgTab );
	auto* imgPromptGroup = new QGroupBox( "Prompt", imgTab );
	auto* imgPromptLayout = new QVBoxLayout( imgPromptGroup );
	g_aiImagePromptEdit = new QPlainTextEdit( imgTab );
	g_aiImagePromptEdit->setPlaceholderText( "e.g. A stone wall texture, weathered and mossy" );
	g_aiImagePromptEdit->setMaximumHeight( 60 );
	imgPromptLayout->addWidget( g_aiImagePromptEdit );
	imgVbox->addWidget( imgPromptGroup );

	auto* imgProviderGroup = new QGroupBox( "Provider", imgTab );
	auto* imgProviderForm = new QFormLayout( imgProviderGroup );
	g_aiImageProviderCombo = new QComboBox( imgTab );
	g_aiImageProviderCombo->addItems( { "iris", "DALL-E", "Mock" } );
	g_aiImageIrisPathEdit = new QLineEdit( imgTab );
	g_aiImageIrisPathEdit->setPlaceholderText( "e.g. /path/to/iris or iris.exe" );
	g_aiImageIrisPathEdit->setText( QString::fromUtf8( g_aiImageIrisPath.c_str() ) );
	g_aiImageIrisModelEdit = new QLineEdit( imgTab );
	g_aiImageIrisModelEdit->setPlaceholderText( "flux-klein-4b" );
	g_aiImageIrisModelEdit->setText( QString::fromUtf8( g_aiImageIrisModel.c_str() ) );
	imgProviderForm->addRow( "Provider", g_aiImageProviderCombo );
	imgProviderForm->addRow( "iris executable", g_aiImageIrisPathEdit );
	imgProviderForm->addRow( "Model dir", g_aiImageIrisModelEdit );
	imgVbox->addWidget( imgProviderGroup );

	auto* imgBtnRow = new QHBoxLayout();
	g_aiImageGenerateBtn = new QPushButton( "Generate", imgTab );
	g_aiImageSaveBtn = new QPushButton( "Save to textures", imgTab );
	g_aiImageApplyBtn = new QPushButton( "Apply to selection", imgTab );
	g_aiImageSaveBtn->setEnabled( false );
	g_aiImageApplyBtn->setEnabled( false );
	QObject::connect( g_aiImageGenerateBtn, &QPushButton::clicked, [](){ AIImage_generate(); } );
	QObject::connect( g_aiImageSaveBtn, &QPushButton::clicked, [](){ AIImage_save(); } );
	QObject::connect( g_aiImageApplyBtn, &QPushButton::clicked, [](){ AIImage_apply(); } );
	imgBtnRow->addWidget( g_aiImageGenerateBtn );
	imgBtnRow->addWidget( g_aiImageSaveBtn );
	imgBtnRow->addWidget( g_aiImageApplyBtn );
	imgVbox->addLayout( imgBtnRow );

	g_aiImagePreviewLabel = new QLabel( imgTab );
	g_aiImagePreviewLabel->setMinimumSize( 128, 128 );
	g_aiImagePreviewLabel->setAlignment( Qt::AlignCenter );
	g_aiImagePreviewLabel->setStyleSheet( "QLabel { background: #2a2a2a; border: 1px solid #444; }" );
	g_aiImagePreviewLabel->setText( "No image" );
	imgVbox->addWidget( g_aiImagePreviewLabel );

	g_aiImageStatusLabel = new QLabel( "Ready", imgTab );
	imgVbox->addWidget( g_aiImageStatusLabel );

	QObject::connect( g_aiImageIrisPathEdit, &QLineEdit::editingFinished, [](){
		g_aiImageIrisPath = g_aiImageIrisPathEdit->text().trimmed().toLatin1().constData();
		Preferences_Save();
	} );
	QObject::connect( g_aiImageIrisModelEdit, &QLineEdit::editingFinished, [](){
		g_aiImageIrisModel = g_aiImageIrisModelEdit->text().trimmed().toLatin1().constData();
		Preferences_Save();
	} );

	tabs->addTab( imgTab, "Image Generation" );

	g_aiAssistantDock->setWidget( tabs );
	window->addDockWidget( Qt::RightDockWidgetArea, g_aiAssistantDock );
	g_aiAssistantDock->hide();
}

void AIAssistant_open(){
	if ( g_aiNetworkManager == nullptr )
		g_aiNetworkManager = new QNetworkAccessManager();

	if ( g_aiAssistantDock == nullptr ) return;

	g_aiAssistantDock->show();
	g_aiAssistantDock->raise();
	g_aiAssistantDock->activateWindow();
}

void AIAssistant_toggleShown(){
	if ( g_aiAssistantDock != nullptr )
		g_aiAssistantDock->setVisible( !g_aiAssistantDock->isVisible() );
}

void AIAssistant_destroy(){
	g_aiAssistantDock = nullptr;
	g_aiPromptEdit = nullptr;
	g_aiResponseEdit = nullptr;
	g_aiLogEdit = nullptr;
	g_aiStatusLabel = nullptr;
	g_aiAgentCombo = nullptr;
	g_aiEndpointEdit = nullptr;
	g_aiModelEdit = nullptr;
	g_aiKeyEnvEdit = nullptr;
	g_aiApiKeyEdit = nullptr;
	g_aiUseEnvVarCheck = nullptr;
	g_aiAddAgentBtn = nullptr;
	g_aiRemoveAgentBtn = nullptr;
	g_aiIncludeSelection = nullptr;
	g_aiIncludeNearby = nullptr;
	g_aiDryRunCheck = nullptr;
	g_aiPlanList = nullptr;
	g_aiApplySelectedBtn = nullptr;
	g_aiApplyAllBtn = nullptr;
	g_aiImagePromptEdit = nullptr;
	g_aiImageProviderCombo = nullptr;
	g_aiImageIrisPathEdit = nullptr;
	g_aiImageIrisModelEdit = nullptr;
	g_aiImagePreviewLabel = nullptr;
	g_aiImageStatusLabel = nullptr;
	g_aiImageGenerateBtn = nullptr;
	g_aiImageSaveBtn = nullptr;
	g_aiImageApplyBtn = nullptr;
	if ( g_aiImageProcess ) {
		g_aiImageProcess->kill();
		g_aiImageProcess->deleteLater();
		g_aiImageProcess = nullptr;
	}
	if ( g_aiNetworkManager != nullptr ) {
		delete g_aiNetworkManager;
		g_aiNetworkManager = nullptr;
	}
}

bool AIAssistant_enabled(){
	return g_aiAssistantEnabled;
}

// --- Preferences ---

void AIAssistantAgents_import( const char* value ){
	agentsFromJson( value );
}
typedef FreeCaller<void(const char*), AIAssistantAgents_import> AIAssistantAgentsImportCaller;
void AIAssistantAgents_export( const StringImportCallback& importer ){
	importer( agentsToJson().c_str() );
}
typedef FreeCaller<void(const StringImportCallback&), AIAssistantAgents_export> AIAssistantAgentsExportCaller;

void AIAssistant_constructPreferences( PreferencesPage& page ){
	page.appendCheckBox( "", "Enable AI Assistant", g_aiAssistantEnabled );
	page.appendEntry( "Active agent", g_aiAssistantActiveAgent );
	page.appendEntry( "Image: iris path", g_aiImageIrisPath );
	page.appendEntry( "Image: iris model", g_aiImageIrisModel );
}

void AIAssistant_constructPage( PreferenceGroup& group ){
	PreferencesPage page( group.createPage( "AI Assistant", "AI Assistant Settings" ) );
	AIAssistant_constructPreferences( page );
}

void AIAssistant_registerPreferencesPage(){
	GlobalPreferenceSystem().registerPreference( "AIAssistantEnabled", BoolImportStringCaller( g_aiAssistantEnabled ), BoolExportStringCaller( g_aiAssistantEnabled ) );
	GlobalPreferenceSystem().registerPreference( "AIAssistantAgents", AIAssistantAgentsImportCaller(), AIAssistantAgentsExportCaller() );
	GlobalPreferenceSystem().registerPreference( "AIAssistantActiveAgent", CopiedStringImportStringCaller( g_aiAssistantActiveAgent ), CopiedStringExportStringCaller( g_aiAssistantActiveAgent ) );
	GlobalPreferenceSystem().registerPreference( "AIAssistantImageIrisPath", CopiedStringImportStringCaller( g_aiImageIrisPath ), CopiedStringExportStringCaller( g_aiImageIrisPath ) );
	GlobalPreferenceSystem().registerPreference( "AIAssistantImageIrisModel", CopiedStringImportStringCaller( g_aiImageIrisModel ), CopiedStringExportStringCaller( g_aiImageIrisModel ) );
	PreferencesDialog_addSettingsPage( makeCallbackF( AIAssistant_constructPage ) );
}
