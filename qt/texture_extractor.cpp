#include "texture_extractor.h"

#include <QFile>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QImage>
#include <QImageWriter>
#include <QRegularExpression>
// QZipReader removed in Qt6 - would need QuaZip or similar library
// For now, PK3 extraction is stubbed
#include <QDebug>

TextureExtractor::TextureExtractor() {
}

bool TextureExtractor::extractFromBSP(const QString& bspPath, const QString& outputDir) {
	QFile file(bspPath);
	if (!file.open(QIODevice::ReadOnly)) {
		return false;
	}
	
	// In full implementation, would:
	// 1. Parse BSP file structure
	// 2. Find texture lump
	// 3. Extract texture data
	// 4. Convert to output format
	// 5. Save to output directory
	
	// For now, simplified version
	QStringList textures = findTexturesInBSP(bspPath);
	
	QDir dir(outputDir);
	if (!dir.exists()) {
		dir.mkpath(".");
	}
	
	for (const QString& texture : textures) {
		if (!matchesFilter(texture)) continue;
		
		// In full implementation, would extract actual texture data
		// For now, just track it
		QString outputPath = dir.absoluteFilePath(texture + "." + m_outputFormat);
		QFileInfo info(outputPath);
		dir.mkpath(info.absolutePath());
		
		// Would extract and save texture here
		m_extractedTextures.append(texture);
		m_extractedCount++;
	}
	
	return true;
}

bool TextureExtractor::extractFromPK3(const QString& pk3Path, const QString& outputDir) {
	// QZipReader removed in Qt6 - would need QuaZip or similar library
	// For now, this is a stub implementation
	// In full implementation, would use QuaZip or minizip to read PK3 files
	
	QDir dir(outputDir);
	if (!dir.exists()) {
		dir.mkpath(".");
	}
	
	QStringList textures = findTexturesInPK3(pk3Path);
	
	for (const QString& texture : textures) {
		if (!matchesFilter(texture)) continue;
		
		// Stub: would extract from ZIP here
		// For now, just track it
		QString outputPath = dir.absoluteFilePath(texture);
		QFileInfo info(outputPath);
		dir.mkpath(info.absolutePath());
		
		m_extractedTextures.append(texture);
		m_extractedCount++;
	}
	
	return true;
}

bool TextureExtractor::extractFromWAD(const QString& wadPath, const QString& outputDir) {
	Q_UNUSED(wadPath);
	Q_UNUSED(outputDir);
	
	// In full implementation, would:
	// 1. Parse WAD file structure
	// 2. Extract texture entries
	// 3. Convert to output format
	// 4. Save to output directory
	
	// WAD support for id Tech 3 is less common than PK3
	return false;
}

bool TextureExtractor::extractFromDirectory(const QString& gameDir, const QString& outputDir, bool recursive) {
	QDir sourceDir(gameDir);
	if (!sourceDir.exists()) {
		return false;
	}
	
	QDir targetDir(outputDir);
	if (!targetDir.exists()) {
		targetDir.mkpath(".");
	}
	
	QStringList textureExtensions = {"*.tga", "*.jpg", "*.jpeg", "*.png"};
	QDirIterator::IteratorFlags flags = recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags;
	
	QDirIterator it(gameDir, textureExtensions, QDir::Files, flags);
	
	while (it.hasNext()) {
		QString sourcePath = it.next();
		QString relativePath = sourceDir.relativeFilePath(sourcePath);
		
		if (!matchesFilter(relativePath)) continue;
		
		QString targetPath = targetDir.absoluteFilePath(relativePath);
		QFileInfo targetInfo(targetPath);
		targetDir.mkpath(targetInfo.absolutePath());
		
		// Convert format if needed
		QString sourceExt = QFileInfo(sourcePath).suffix().toLower();
		QString targetExt = m_outputFormat.toLower();
		
		// Change output extension
		QString basePath = targetPath;
		QRegularExpression extRegex("\\.[^.]+$");
		basePath.replace(extRegex, "");
		targetPath = basePath + "." + targetExt;
		
		if (sourceExt != targetExt) {
			if (convertTexture(sourcePath, targetPath, targetExt)) {
				m_extractedTextures.append(relativePath);
				m_extractedCount++;
			} else {
				m_failedCount++;
			}
		} else {
			// Just copy
			if (QFile::copy(sourcePath, targetPath)) {
				m_extractedTextures.append(relativePath);
				m_extractedCount++;
			} else {
				m_failedCount++;
			}
		}
	}
	
	return true;
}

bool TextureExtractor::extractTexture(const QByteArray& data, const QString& outputPath, const QString& format) {
	QImage image;
	if (!image.loadFromData(data)) {
		return false;
	}
	
	QImageWriter writer(outputPath);
	writer.setFormat(format.toUtf8());
	return writer.write(image);
}

QStringList TextureExtractor::findTexturesInBSP(const QString& bspPath) {
	Q_UNUSED(bspPath);
	
	// In full implementation, would parse BSP file and find texture names
	// For now, return empty list
	return QStringList();
}

QStringList TextureExtractor::findTexturesInPK3(const QString& pk3Path) {
	Q_UNUSED(pk3Path);
	QStringList textures;
	
	// QZipReader removed in Qt6 - would need QuaZip or similar library
	// For now, this is a stub implementation
	// In full implementation, would use QuaZip or minizip to read PK3 files
	// and extract texture file paths
	
	// Stub: would iterate through ZIP entries here
	// For now, return empty list
	return textures;
}

QStringList TextureExtractor::findTexturesInWAD(const QString& wadPath) {
	Q_UNUSED(wadPath);
	
	// In full implementation, would parse WAD file
	return QStringList();
}

bool TextureExtractor::matchesFilter(const QString& texturePath) const {
	if (m_filters.isEmpty()) {
		return true;
	}
	
	QString lower = texturePath.toLower();
	for (const QString& filter : m_filters) {
		if (lower.contains(filter.toLower())) {
			return true;
		}
	}
	
	return false;
}

bool TextureExtractor::convertTexture(const QString& inputPath, const QString& outputPath, const QString& format) {
	QImage image;
	if (!image.load(inputPath)) {
		return false;
	}
	
	QImageWriter writer(outputPath);
	writer.setFormat(format.toUtf8());
	return writer.write(image);
}

void TextureExtractor::resetStats() {
	m_extractedCount = 0;
	m_failedCount = 0;
	m_extractedTextures.clear();
}
