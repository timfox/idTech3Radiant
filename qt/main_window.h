#pragma once

#include <QMainWindow>
#include <QVector3D>
#include <QTreeWidgetItem>

#include "qt_env.h"

class QPlainTextEdit;
class QLabel;
class VkViewportWidget;
class QTreeWidget;
class QListWidget;
class QDoubleSpinBox;
class QLineEdit;
class QComboBox;
class ColorPickerDialog;
class KeybindEditor;

class RadiantMainWindow : public QMainWindow {
	Q_OBJECT
public:
	explicit RadiantMainWindow(const QtRadiantEnv& env, QWidget* parent = nullptr);

private:
	void buildUi(const QtRadiantEnv& env);
	void appendLogInfo(const QtRadiantEnv& env);
	void populateOutliner();
	void populateInspector();
	void populateTextureBrowser(const QtRadiantEnv& env);
	void setupDockWidgetMinimize(QDockWidget* dock);
	void setOutlinerLoadedMap(const QString& path);
	void createBrushAtSelection();
	void createEntityAtSelection();
	bool hasSelection() const;
	void populateInspectorForItem(QTreeWidgetItem* item);
	void saveMap();
	bool saveMapToFile(const QString& path);
public:
	void loadMap(const QString& path);
	void writeEntityToMap(QTextStream& out, QTreeWidgetItem* item);
	void writeBrushToMap(QTextStream& out, QTreeWidgetItem* item);
	void openColorPicker();
	void openKeybindEditor();
	bool eventFilter(QObject* obj, QEvent* event) override;

	QPlainTextEdit* m_console = nullptr;

	// Enhanced status bar widgets
	QLabel* m_selectionStatus = nullptr;
	QLabel* m_coordinatesStatus = nullptr;
	QLabel* m_gridStatus = nullptr;
	QLabel* m_toolStatus = nullptr;
	QLabel* m_cameraStatus = nullptr;
	QLabel* m_performanceStatus = nullptr;

	QTreeWidget* m_outliner = nullptr;
	QWidget* m_inspector = nullptr;
	QListWidget* m_textureList = nullptr;
	VkViewportWidget* m_viewPerspective = nullptr;
	VkViewportWidget* m_viewTop = nullptr;
	VkViewportWidget* m_viewFront = nullptr;
	VkViewportWidget* m_viewSide = nullptr;
	QDoubleSpinBox* m_posX = nullptr;
	QDoubleSpinBox* m_posY = nullptr;
	QDoubleSpinBox* m_posZ = nullptr;
	QLineEdit* m_nameEdit = nullptr;
	QComboBox* m_typeCombo = nullptr;
	QVector3D m_lastSelection{0.0f, 0.0f, 0.0f};

	// Hotbox widget
	class HotboxWidget* m_hotbox = nullptr;
	QString m_currentMaterial;
	QAction* m_createBrushAction = nullptr;
	QAction* m_createEntityAction = nullptr;
	
	// Editor object visibility toggles
	bool m_showToolTextures = true;
	bool m_showEditorModels = true;
	QAction* m_toggleToolTexturesAction = nullptr;
	QAction* m_toggleEditorModelsAction = nullptr;
	
	void toggleToolTextures();
	void toggleEditorModels();
	
	// Autosave and crash recovery
	class AutosaveManager* m_autosaveManager = nullptr;
	QString m_currentMapPath;
	void setupAutosave();
	void checkForRecoveryFiles();
	
	// Grid settings
	void openGridSettings();
	
	// Tool improvements
	void setupGizmoControls();
	void cycleGizmoMode();
	void cycleGizmoOperation();
	void toggleGizmoSpace();
	void setGizmoAxisLock(int axis, bool locked);
	
	// Clipping tool
	void activateClippingTool();
	void toggleClippingMode();
	
	// Merge tool
	void mergeSelectedBrushes();
	class MergeTool* m_mergeTool = nullptr;
	
	// Gizmo state
	int m_currentGizmoMode{1}; // 0=None, 1=Box, 2=Gizmo
	int m_currentGizmoOperation{0}; // 0=Translate, 1=Rotate, 2=Scale
	bool m_gizmoAxisLocks[3]{false, false, false};

	// Direct gizmo mode setting (for toolbar buttons)
	void setGizmoMode(int mode);
	void setGizmoOperation(int operation);
	
	// Clipping tool state
	class ClippingTool* m_clippingTool = nullptr;
	bool m_clippingToolActive{false};
	
	// Polygon tool
	void activatePolygonTool();
	class PolygonTool* m_polygonTool = nullptr;
	bool m_polygonToolActive{false};
	
	// Vertex tool
	void activateVertexTool();
	class VertexTool* m_vertexTool = nullptr;
	bool m_vertexToolActive{false};
	
	// Audio browser
	void openAudioBrowser();
	
	// UV Tool
	void activateUVTool();
	class UVTool* m_uvTool = nullptr;
	bool m_uvToolActive{false};
	
	// Autocaulk
	void applyAutocaulk();
	class AutocaulkTool* m_autocaulkTool = nullptr;
	
	// Extrusion tool
	void activateExtrusionTool();
	class ExtrusionTool* m_extrusionTool = nullptr;
	bool m_extrusionToolActive{false};
	
	// Texture paint tool
	void activateTexturePaintTool();
	class TexturePaintTool* m_texturePaintTool = nullptr;
	bool m_texturePaintToolActive{false};
	
	// Light power adjustment
	void adjustLightPower(float delta);
	
	// Map info
	void showMapInfo();
	
	// Patch thicken
	void activatePatchThickenTool();
	class PatchThickenTool* m_patchThickenTool = nullptr;
	
	// CSG Tool (Shell modifier)
	void activateCSGTool();
	class CSGTool* m_csgTool = nullptr;
	
	// Brush resize tool
	void activateBrushResizeTool();
	class BrushResizeTool* m_brushResizeTool = nullptr;
	
	// Entity walker
	void selectConnectedEntities();
	void walkToNextEntity();
	void walkToPreviousEntity();
	class EntityWalker* m_entityWalker = nullptr;
	
	// Texture browser enhancements
	void enableTextureAlphaDisplay(bool enabled);
	void searchTextures(const QString& query);
	
	// Texture paste tool
	void activateTexturePasteTool();
	class TexturePasteTool* m_texturePasteTool = nullptr;
	bool m_texturePasteToolActive{false};
	
	// MESS (Macro Entity Scripting System)
	void openMESSDialog();
	class MESSManager* m_messManager = nullptr;
	
	// Texture extraction
	void openTextureExtractionDialog();
	
	// Surface Inspector
	void openSurfaceInspector();
	class SurfaceInspector* m_surfaceInspector = nullptr;
	
	// Selection Sets
	void openSelectionSetsDialog();
	class SelectionSets* m_selectionSets = nullptr;
	
	// Preferences
	void openPreferences();
	
	// Recent files
	void addRecentFile(const QString& filePath);
	void updateRecentFilesMenu();

	// 3D editing support
	void enable3DEditing(bool enabled) { m_3dEditingEnabled = enabled; }
	bool is3DEditingEnabled() const { return m_3dEditingEnabled; }

	// Status bar updates
	void updateGridStatus();
	void updateToolStatus();
	void updateCameraStatus();
	void updatePerformanceStatus();

	// Workspace management
	void saveWorkspace(const QString& name);
	void loadWorkspace(const QString& name);
	void deleteWorkspace(const QString& name);
	QStringList getWorkspaceNames() const;
	void saveCurrentWorkspace();
	void loadWorkspaceDialog();
	void deleteWorkspaceDialog();

	// Hotbox (Maya-style radial menu)
	void showHotbox();
	void hideHotbox();
	bool isHotboxVisible() const;

	// Camera controls
	void resetCamera();

protected:
	void keyPressEvent(QKeyEvent* event) override;
	
private:
	bool m_3dEditingEnabled{true}; // Enable editing in 3D view
};
