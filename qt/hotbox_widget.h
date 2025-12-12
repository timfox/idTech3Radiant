#pragma once

#include <QWidget>
#include <QPoint>
#include <QString>
#include <QVector>
#include <QRect>

QT_BEGIN_NAMESPACE
class QPainter;
QT_END_NAMESPACE

struct HotboxItem {
	QString label;
	QRect bounds; // Screen rectangle for this item
	std::function<void()> callback;
};

class HotboxWidget : public QWidget {
	Q_OBJECT

public:
	explicit HotboxWidget(QWidget* parent = nullptr);

	void setPosition(const QPoint& pos);
	void addItem(const QString& label, std::function<void()> callback);
	void clearItems();

Q_SIGNALS:
	void itemSelected(const QString& label);

protected:
	void paintEvent(QPaintEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void keyPressEvent(QKeyEvent* event) override;

private:
	QPoint m_center;
	QVector<HotboxItem> m_items;
	int m_radius = 120;
	int m_itemSize = 60;

	void updateItemBounds();
	HotboxItem* getItemAt(const QPoint& pos);
};