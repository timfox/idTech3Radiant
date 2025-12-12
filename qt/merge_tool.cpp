#include "merge_tool.h"

#include <QTreeWidgetItem>
#include <QVector3D>
#include <QVariant>
#include <QVariantList>
#include <cmath>
#include <algorithm>
#include <limits>

MergeTool::MergeTool() {
}

bool MergeTool::mergeBrushes(const QVector<QTreeWidgetItem*>& brushItems) {
	if (!canMerge(brushItems)) {
		return false;
	}
	
	// Collect all vertices from selected brushes
	QVector<QVector3D> allVertices;
	for (QTreeWidgetItem* item : brushItems) {
		QVector<QVector3D> brushVerts = getBrushVertices(item);
		allVertices.append(brushVerts);
	}
	
	if (allVertices.isEmpty()) {
		m_validationError = QStringLiteral("No vertices found in selected brushes");
		return false;
	}
	
	// Compute convex hull
	QVector<QVector3D> hullVertices = computeConvexHull(allVertices);
	
	if (hullVertices.size() < 4) {
		m_validationError = QStringLiteral("Convex hull has too few vertices");
		return false;
	}
	
	// Validate convexity
	if (!validateConvex(hullVertices)) {
		m_validationError = QStringLiteral("Resulting shape is not convex");
		return false;
	}
	
	// Create merged brush item
	// In full implementation, would create actual brush geometry
	m_mergedBrush = new QTreeWidgetItem();
	m_mergedBrush->setText(0, QStringLiteral("merged_brush"));
	m_mergedBrush->setText(1, QStringLiteral("brush"));
	
	// Store merged geometry
	QVariantList vertexList;
	for (const QVector3D& v : hullVertices) {
		vertexList.append(QVariant::fromValue(v));
	}
	m_mergedBrush->setData(0, Qt::UserRole, vertexList);
	
	// Preserve texture if enabled
	if (m_preserveTextures && !brushItems.isEmpty()) {
		QString material = getBrushMaterial(brushItems[0]);
		m_mergedBrush->setData(1, Qt::UserRole, material);
	}
	
	return true;
}

bool MergeTool::canMerge(const QVector<QTreeWidgetItem*>& brushItems) const {
	if (brushItems.isEmpty()) {
		m_validationError = QStringLiteral("No brushes selected");
		return false;
	}
	
	if (brushItems.size() < 2) {
		m_validationError = QStringLiteral("Select at least 2 brushes to merge");
		return false;
	}
	
	// Check all items are brushes
	for (QTreeWidgetItem* item : brushItems) {
		if (!item) {
			m_validationError = QStringLiteral("Invalid brush item");
			return false;
		}
		if (item->text(1) != QStringLiteral("brush")) {
			m_validationError = QStringLiteral("All selected items must be brushes");
			return false;
		}
	}
	
	// Collect vertices
	QVector<QVector3D> allVertices;
	for (QTreeWidgetItem* item : brushItems) {
		QVector<QVector3D> brushVerts = getBrushVertices(item);
		if (brushVerts.isEmpty()) {
			m_validationError = QStringLiteral("One or more brushes have no vertices");
			return false;
		}
		allVertices.append(brushVerts);
	}
	
	// Check if vertices can form a convex shape
	if (allVertices.size() < 4) {
		m_validationError = QStringLiteral("Not enough vertices to form a valid brush");
		return false;
	}
	
	return true;
}

bool MergeTool::validateConvex(const QVector<QVector3D>& vertices) const {
	if (vertices.size() < 4) return false;
	
	// Simplified convexity check
	// In full implementation, would check all faces are planar and form a convex polyhedron
	return isConvexHull(vertices);
}

bool MergeTool::isConvexHull(const QVector<QVector3D>& vertices) const {
	if (vertices.size() < 4) return false;
	
	// Basic check: ensure all vertices are on or inside the convex hull
	// In full implementation, would use proper convex hull algorithm
	// For now, assume valid if we have enough vertices
	return true;
}

QVector<QVector3D> MergeTool::getBrushVertices(QTreeWidgetItem* brushItem) const {
	QVector<QVector3D> vertices;
	
	if (!brushItem) return vertices;
	
	// Try to get vertices from brush data
	QVariant data = brushItem->data(0, Qt::UserRole);
	if (data.canConvert<QVector3D>()) {
		vertices.append(data.value<QVector3D>());
	} else if (data.canConvert<QVariantList>()) {
		QVariantList list = data.toList();
		for (const QVariant& v : list) {
			if (v.canConvert<QVector3D>()) {
				vertices.append(v.value<QVector3D>());
			}
		}
	}
	
	// If no vertices found, create default cube from position
	if (vertices.isEmpty()) {
		QVector3D pos = brushItem->data(0, Qt::UserRole).value<QVector3D>();
		if (pos.isNull()) {
			pos = QVector3D(0.0f, 0.0f, 0.0f);
		}
		
		const float size = 32.0f; // Default brush size
		vertices = {
			pos + QVector3D(-size, -size, -size),
			pos + QVector3D(size, -size, -size),
			pos + QVector3D(size, size, -size),
			pos + QVector3D(-size, size, -size),
			pos + QVector3D(-size, -size, size),
			pos + QVector3D(size, -size, size),
			pos + QVector3D(size, size, size),
			pos + QVector3D(-size, size, size)
		};
	}
	
	return vertices;
}

QVector<QVector3D> MergeTool::computeConvexHull(const QVector<QVector3D>& vertices) const {
	if (vertices.size() < 4) {
		return vertices;
	}
	
	// Simplified convex hull computation
	// In full implementation, would use proper algorithm (Graham scan, QuickHull, etc.)
	
	// For now, use bounding box approach (creates a convex shape)
	QVector3D minV(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
	QVector3D maxV(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest());
	
	for (const QVector3D& v : vertices) {
		minV.setX(std::min(minV.x(), v.x()));
		minV.setY(std::min(minV.y(), v.y()));
		minV.setZ(std::min(minV.z(), v.z()));
		maxV.setX(std::max(maxV.x(), v.x()));
		maxV.setY(std::max(maxV.y(), v.y()));
		maxV.setZ(std::max(maxV.z(), v.z()));
	}
	
	// Create box vertices
	QVector<QVector3D> hull = {
		QVector3D(minV.x(), minV.y(), minV.z()),
		QVector3D(maxV.x(), minV.y(), minV.z()),
		QVector3D(maxV.x(), maxV.y(), minV.z()),
		QVector3D(minV.x(), maxV.y(), minV.z()),
		QVector3D(minV.x(), minV.y(), maxV.z()),
		QVector3D(maxV.x(), minV.y(), maxV.z()),
		QVector3D(maxV.x(), maxV.y(), maxV.z()),
		QVector3D(minV.x(), maxV.y(), maxV.z())
	};
	
	return hull;
}

QString MergeTool::getBrushMaterial(QTreeWidgetItem* brushItem) const {
	if (!brushItem) return QStringLiteral("common/caulk");
	
	QVariant material = brushItem->data(1, Qt::UserRole);
	if (material.isValid() && material.canConvert<QString>()) {
		return material.toString();
	}
	
	return QStringLiteral("common/caulk");
}
