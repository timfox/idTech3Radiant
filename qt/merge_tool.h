#pragma once

#include <QVector3D>
#include <QVector>
#include <QString>

QT_BEGIN_NAMESPACE
class QTreeWidgetItem;
QT_END_NAMESPACE

// Merge tool for combining selected brushes into convex shapes
class MergeTool {
public:
	MergeTool();
	
	// Merge selected brushes
	// Returns true if merge was successful
	bool mergeBrushes(const QVector<QTreeWidgetItem*>& brushItems);
	
	// Validation
	bool canMerge(const QVector<QTreeWidgetItem*>& brushItems) const;
	QString getValidationError() const { return m_validationError; }
	
	// Result
	QTreeWidgetItem* getMergedBrush() const { return m_mergedBrush; }
	
	// Texture preservation
	void setPreserveTextures(bool preserve) { m_preserveTextures = preserve; }
	bool preserveTextures() const { return m_preserveTextures; }

private:
	mutable QString m_validationError;
	QTreeWidgetItem* m_mergedBrush{nullptr};
	bool m_preserveTextures{true};
	
	// Geometry validation
	bool validateConvex(const QVector<QVector3D>& vertices) const;
	bool isConvexHull(const QVector<QVector3D>& vertices) const;
	
	// Brush geometry helpers
	QVector<QVector3D> getBrushVertices(QTreeWidgetItem* brushItem) const;
	QVector<QVector3D> computeConvexHull(const QVector<QVector3D>& vertices) const;
	
	// Texture/material helpers
	QString getBrushMaterial(QTreeWidgetItem* brushItem) const;
};
