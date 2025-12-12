#pragma once

#include <QString>
#include <QStringList>
#include <QMap>

// Texture extraction tool for id Tech 3 assets
class TextureExtractor {
public:
	TextureExtractor();
	
	// Extraction methods
	bool extractFromBSP(const QString& bspPath, const QString& outputDir);
	bool extractFromPK3(const QString& pk3Path, const QString& outputDir);
	bool extractFromWAD(const QString& wadPath, const QString& outputDir);
	bool extractFromDirectory(const QString& gameDir, const QString& outputDir, bool recursive = true);
	
	// Settings
	void setOutputFormat(const QString& format) { m_outputFormat = format; } // "png", "tga", "jpg"
	QString outputFormat() const { return m_outputFormat; }
	
	void setRecursive(bool recursive) { m_recursive = recursive; }
	bool recursive() const { return m_recursive; }
	
	// Statistics
	int extractedCount() const { return m_extractedCount; }
	int failedCount() const { return m_failedCount; }
	QStringList extractedTextures() const { return m_extractedTextures; }
	void resetStats();
	
	// Filter
	void setFilter(const QStringList& filters) { m_filters = filters; }
	QStringList filter() const { return m_filters; }

private:
	QString m_outputFormat{"png"};
	bool m_recursive{true};
	QStringList m_filters;
	
	int m_extractedCount{0};
	int m_failedCount{0};
	QStringList m_extractedTextures;
	
	// Extraction helpers
	bool extractTexture(const QByteArray& data, const QString& outputPath, const QString& format);
	QStringList findTexturesInBSP(const QString& bspPath);
	QStringList findTexturesInPK3(const QString& pk3Path);
	QStringList findTexturesInWAD(const QString& wadPath);
	bool matchesFilter(const QString& texturePath) const;
	
	// Format conversion
	bool convertTexture(const QString& inputPath, const QString& outputPath, const QString& format);
};
