#include "hotbox_widget.h"

#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPainterPath>
#include <cmath>
#include <QApplication>

HotboxWidget::HotboxWidget(QWidget* parent)
	: QWidget(parent, Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
{
	setAttribute(Qt::WA_TranslucentBackground);
	setAttribute(Qt::WA_ShowWithoutActivating);
	setMouseTracking(true);
	setFocusPolicy(Qt::StrongFocus);
}

void HotboxWidget::setPosition(const QPoint& pos) {
	m_center = pos;

	// Calculate widget bounds to encompass all items
	int totalRadius = m_radius + m_itemSize;
	QRect bounds(pos.x() - totalRadius, pos.y() - totalRadius,
				 totalRadius * 2, totalRadius * 2);

	setGeometry(bounds);
	updateItemBounds();
}

void HotboxWidget::addItem(const QString& label, std::function<void()> callback) {
	HotboxItem item;
	item.label = label;
	item.callback = callback;
	m_items.append(item);
	updateItemBounds();
}

void HotboxWidget::clearItems() {
	m_items.clear();
}

void HotboxWidget::updateItemBounds() {
	if (m_items.isEmpty()) return;

	int itemCount = m_items.size();
	float angleStep = 2.0f * M_PI / itemCount;

	for (int i = 0; i < itemCount; ++i) {
		float angle = i * angleStep - M_PI_2; // Start from top
		float x = m_center.x() + cos(angle) * m_radius - m_itemSize / 2;
		float y = m_center.y() + sin(angle) * m_radius - m_itemSize / 2;

		m_items[i].bounds = QRect(static_cast<int>(x), static_cast<int>(y), m_itemSize, m_itemSize);
	}
}

HotboxItem* HotboxWidget::getItemAt(const QPoint& pos) {
	for (HotboxItem& item : m_items) {
		if (item.bounds.contains(pos)) {
			return &item;
		}
	}
	return nullptr;
}

void HotboxWidget::paintEvent(QPaintEvent* event) {
	Q_UNUSED(event);

	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	// Draw background circle
	painter.setPen(QPen(QColor(80, 80, 80, 200), 2));
	painter.setBrush(QBrush(QColor(40, 40, 40, 240)));
	painter.drawEllipse(m_center, m_radius, m_radius);

	// Draw center circle
	painter.setPen(QPen(QColor(100, 150, 255), 2));
	painter.setBrush(QBrush(QColor(60, 90, 200, 180)));
	painter.drawEllipse(m_center, 25, 25);

	// Draw center text
	painter.setPen(QPen(Qt::white));
	painter.setFont(QFont("Arial", 8));
	painter.drawText(QRect(m_center.x() - 20, m_center.y() - 20, 40, 40),
					 Qt::AlignCenter, "HOTBOX");

	// Draw items
	for (const HotboxItem& item : m_items) {
		// Draw item background
		QColor bgColor(70, 70, 70, 200);
		painter.setPen(QPen(bgColor.darker(150), 1));
		painter.setBrush(QBrush(bgColor));
		painter.drawEllipse(item.bounds);

		// Draw item text
		painter.setPen(QPen(Qt::white));
		painter.setFont(QFont("Arial", 7));
		painter.drawText(item.bounds, Qt::AlignCenter, item.label);
	}
}

void HotboxWidget::mousePressEvent(QMouseEvent* event) {
	HotboxItem* item = getItemAt(event->pos());
	if (item) {
		// Highlight selected item
		update();
	}
	QWidget::mousePressEvent(event);
}

void HotboxWidget::mouseReleaseEvent(QMouseEvent* event) {
	HotboxItem* item = getItemAt(event->pos());
	if (item && item->callback) {
		item->callback();
		Q_EMIT itemSelected(item->label);
	}
	hide();
	QWidget::mouseReleaseEvent(event);
}

void HotboxWidget::keyPressEvent(QKeyEvent* event) {
	if (event->key() == Qt::Key_Escape || event->key() == Qt::Key_Space) {
		hide();
		event->accept();
	} else {
		QWidget::keyPressEvent(event);
	}
}