#pragma once

#include <QString>
#include <QVector3D>
#include <QVector>

// Seamless texture paste tool (face<->face, patch<->face)
class TexturePasteTool {
public:
	TexturePasteTool();
	
	// Copy texture from source
	void copyTexture(int sourceBrushIndex, int sourceFaceIndex);
	void copyTextureFromPatch(int sourcePatchIndex);
	
	// Paste texture to target
	bool pasteTexture(int targetBrushIndex, int targetFaceIndex);
	bool pasteTextureToPatch(int targetPatchIndex);
	
	// Paste to multiple targets
	bool pasteTextureToFaces(const QVector<int>& brushIndices, const QVector<int>& faceIndices);
	
	// Settings
	void setPasteMode(int mode) { m_pasteMode = mode; } // 0=Full, 1=Alignment only, 2=Scale only
	int pasteMode() const { return m_pasteMode; }
	
	// Status
	bool hasCopiedTexture() const { return m_hasCopiedTexture; }
	QString copiedTexture() const { return m_copiedTexture; }

private:
	QString m_copiedTexture;
	QVector3D m_copiedScale;
	QVector3D m_copiedOffset;
	float m_copiedRotation{0.0f};
	bool m_hasCopiedTexture{false};
	int m_pasteMode{0}; // 0=Full, 1=Alignment only, 2=Scale only
	
	// Texture data helpers
	void getTextureData(int brushIndex, int faceIndex, QString& texture, QVector3D& scale, QVector3D& offset, float& rotation);
	void setTextureData(int brushIndex, int faceIndex, const QString& texture, const QVector3D& scale, const QVector3D& offset, float rotation);
};
