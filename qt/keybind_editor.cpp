#include "keybind_editor.h"

#include <QTableWidget>
#include <QTableWidgetItem>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeySequenceEdit>
#include <QMessageBox>
#include <QSettings>
#include <QGroupBox>

KeybindEditor::KeybindEditor(QWidget* parent)
	: QDialog(parent)
{
	setWindowTitle(QStringLiteral("Keybind Editor"));
	setModal(true);
	resize(700, 500);
	
	auto* mainLayout = new QVBoxLayout(this);
	
	// Filter
	auto* filterLayout = new QHBoxLayout();
	filterLayout->addWidget(new QLabel(QStringLiteral("Filter:")));
	m_filterEdit = new QLineEdit(this);
	m_filterEdit->setPlaceholderText(QStringLiteral("Search keybinds..."));
	filterLayout->addWidget(m_filterEdit);
	mainLayout->addLayout(filterLayout);
	
	// Table
	m_table = new QTableWidget(this);
	m_table->setColumnCount(3);
	m_table->setHorizontalHeaderLabels({QStringLiteral("Action"), QStringLiteral("Description"), QStringLiteral("Keybind")});
	m_table->horizontalHeader()->setStretchLastSection(true);
	m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_table->setSelectionMode(QAbstractItemView::SingleSelection);
	m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	mainLayout->addWidget(m_table);
	
	// Controls
	auto* controlsLayout = new QHBoxLayout();
	m_assignBtn = new QPushButton(QStringLiteral("Assign Keybind..."), this);
	m_resetBtn = new QPushButton(QStringLiteral("Reset"), this);
	m_resetAllBtn = new QPushButton(QStringLiteral("Reset All"), this);
	controlsLayout->addWidget(m_assignBtn);
	controlsLayout->addWidget(m_resetBtn);
	controlsLayout->addWidget(m_resetAllBtn);
	controlsLayout->addStretch();
	mainLayout->addLayout(controlsLayout);
	
	// Status
	m_statusLabel = new QLabel(this);
	mainLayout->addWidget(m_statusLabel);
	
	// Buttons
	auto* buttonLayout = new QHBoxLayout();
	buttonLayout->addStretch();
	auto* okBtn = new QPushButton(QStringLiteral("OK"), this);
	auto* cancelBtn = new QPushButton(QStringLiteral("Cancel"), this);
	buttonLayout->addWidget(okBtn);
	buttonLayout->addWidget(cancelBtn);
	mainLayout->addLayout(buttonLayout);
	
	// Connections
	connect(m_table, &QTableWidget::itemSelectionChanged, this, &KeybindEditor::onItemSelectionChanged);
	connect(m_assignBtn, &QPushButton::clicked, this, &KeybindEditor::onAssignKeybind);
	connect(m_resetBtn, &QPushButton::clicked, this, &KeybindEditor::onResetKeybind);
	connect(m_resetAllBtn, &QPushButton::clicked, this, &KeybindEditor::onResetAll);
	connect(m_filterEdit, &QLineEdit::textChanged, this, &KeybindEditor::onFilterChanged);
	connect(okBtn, &QPushButton::clicked, this, [this]() {
		saveKeybinds();
		accept();
	});
	connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
	
	populateKeybinds();
	loadKeybinds();
	updateKeybindDisplay();
}

void KeybindEditor::populateKeybinds() {
	// Define all available keybinds
	m_keybinds.clear();
	
	// File operations
	m_keybinds["file.open"] = {"Open Map", "Open a map file", QKeySequence("Ctrl+O"), QKeySequence()};
	m_keybinds["file.save"] = {"Save Map", "Save the current map", QKeySequence("Ctrl+S"), QKeySequence()};
	
	// Geometry tools
	m_keybinds["tool.create_brush"] = {"Create Brush", "Create a new brush", QKeySequence("Ctrl+B"), QKeySequence()};
	m_keybinds["tool.create_entity"] = {"Create Entity", "Create a new entity", QKeySequence("Ctrl+E"), QKeySequence()};
	
	// Selection tools
	m_keybinds["tool.select"] = {"Select Tool", "Selection tool", QKeySequence("S"), QKeySequence()};
	m_keybinds["tool.move"] = {"Move Tool", "Move tool", QKeySequence("M"), QKeySequence()};
	m_keybinds["tool.rotate"] = {"Rotate Tool", "Rotate tool", QKeySequence("R"), QKeySequence()};
	m_keybinds["tool.scale"] = {"Scale Tool", "Scale tool", QKeySequence("T"), QKeySequence()};
	
	// Color picker
	m_keybinds["tool.color_picker"] = {"Color Picker", "Open color picker", QKeySequence("C"), QKeySequence()};
	
	// Viewport
	m_keybinds["viewport.grid"] = {"Toggle Grid", "Toggle grid display", QKeySequence("G"), QKeySequence()};
	m_keybinds["viewport.3d_grid"] = {"Toggle 3D Grid", "Toggle 3D grid", QKeySequence("P"), QKeySequence()};
	
	// Editor objects
	m_keybinds["editor.toggle_textures"] = {"Toggle Tool Textures", "Toggle tool textures", QKeySequence("Ctrl+Shift+F2"), QKeySequence()};
	m_keybinds["editor.toggle_models"] = {"Toggle Editor Models", "Toggle editor models/sprites", QKeySequence("Alt+O"), QKeySequence()};
	
	// Camera controls
	m_keybinds["camera.focus_selection"] = {"Focus on Selection", "Focus camera on selected object", QKeySequence("Tab"), QKeySequence()};
	m_keybinds["camera.forward"] = {"Camera Forward", "Move camera forward (WASD)", QKeySequence("W"), QKeySequence()};
	m_keybinds["camera.left"] = {"Camera Left", "Strafe camera left (WASD)", QKeySequence("A"), QKeySequence()};
	m_keybinds["camera.backward"] = {"Camera Backward", "Move camera backward (WASD)", QKeySequence("S"), QKeySequence()};
	m_keybinds["camera.right"] = {"Camera Right", "Strafe camera right (WASD)", QKeySequence("D"), QKeySequence()};
	
	// Tools
	m_keybinds["tool.clipping"] = {"Clipping Tool", "Activate clipping tool", QKeySequence("Ctrl+X"), QKeySequence()};
	m_keybinds["tool.polygon"] = {"Polygon Tool", "Activate polygon tool", QKeySequence(), QKeySequence()};
	m_keybinds["tool.vertex"] = {"Vertex Tool", "Activate vertex tool", QKeySequence(), QKeySequence()};
	m_keybinds["tool.uv"] = {"UV Tool", "Activate UV tool", QKeySequence(), QKeySequence()};
	m_keybinds["tool.extrude"] = {"Extrude Tool", "Activate face extrusion tool", QKeySequence(), QKeySequence()};
	m_keybinds["tool.texture_paint"] = {"Texture Paint", "Activate texture paint tool", QKeySequence(), QKeySequence()};
	m_keybinds["tool.merge"] = {"Merge Brushes", "Merge selected brushes", QKeySequence("Ctrl+Shift+M"), QKeySequence()};
	m_keybinds["tool.autocaulk"] = {"Autocaulk", "Apply autocaulk to selected", QKeySequence("Ctrl+Alt+C"), QKeySequence()};
	
	// Gizmo
	m_keybinds["gizmo.toggle_mode"] = {"Toggle Gizmo Mode", "Cycle gizmo mode (None/Box/Gizmo)", QKeySequence("X"), QKeySequence()};
	m_keybinds["gizmo.toggle_space"] = {"Toggle Gizmo Space", "Toggle Global/Local space", QKeySequence("Ctrl+J"), QKeySequence()};
	m_keybinds["gizmo.lock_x"] = {"Lock X Axis", "Lock/unlock X axis", QKeySequence("X"), QKeySequence()};
	m_keybinds["gizmo.lock_y"] = {"Lock Y Axis", "Lock/unlock Y axis", QKeySequence("Y"), QKeySequence()};
	m_keybinds["gizmo.lock_z"] = {"Lock Z Axis", "Lock/unlock Z axis", QKeySequence("Z"), QKeySequence()};
	
	// Viewport
	m_keybinds["viewport.zoom_in"] = {"Zoom In", "Zoom in viewport", QKeySequence("Ctrl++"), QKeySequence()};
	m_keybinds["viewport.zoom_out"] = {"Zoom Out", "Zoom out viewport", QKeySequence("Ctrl+-"), QKeySequence()};
	m_keybinds["viewport.fit_selection"] = {"Fit Selection", "Fit viewport to selection", QKeySequence("F"), QKeySequence()};
}

void KeybindEditor::loadKeybinds() {
	QSettings settings;
	settings.beginGroup("Keybinds");
	
	for (auto it = m_keybinds.begin(); it != m_keybinds.end(); ++it) {
		QString key = settings.value(it.key(), QString()).toString();
		if (!key.isEmpty()) {
			it.value().currentSequence = QKeySequence::fromString(key);
		} else {
			it.value().currentSequence = it.value().defaultSequence;
		}
	}
	
	settings.endGroup();
}

void KeybindEditor::saveKeybinds() {
	QSettings settings;
	settings.beginGroup("Keybinds");
	
	for (auto it = m_keybinds.constBegin(); it != m_keybinds.constEnd(); ++it) {
		if (it.value().currentSequence != it.value().defaultSequence) {
			settings.setValue(it.key(), it.value().currentSequence.toString());
		} else {
			settings.remove(it.key());
		}
	}
	
	settings.endGroup();
}

QKeySequence KeybindEditor::getKeybind(const QString& actionName) const {
	if (m_keybinds.contains(actionName)) {
		return m_keybinds[actionName].currentSequence;
	}
	return QKeySequence();
}

void KeybindEditor::setKeybind(const QString& actionName, const QKeySequence& sequence) {
	if (m_keybinds.contains(actionName)) {
		m_keybinds[actionName].currentSequence = sequence;
		updateKeybindDisplay();
	}
}

void KeybindEditor::updateKeybindDisplay() {
	QString filter = m_filterEdit->text().toLower();
	
	m_table->setRowCount(0);
	
	for (auto it = m_keybinds.begin(); it != m_keybinds.end(); ++it) {
		// Apply filter
		if (!filter.isEmpty()) {
			if (!it.value().name.toLower().contains(filter) &&
				!it.value().description.toLower().contains(filter) &&
				!it.key().toLower().contains(filter)) {
				continue;
			}
		}
		
		int row = m_table->rowCount();
		m_table->insertRow(row);
		
		m_table->setItem(row, 0, new QTableWidgetItem(it.value().name));
		m_table->setItem(row, 1, new QTableWidgetItem(it.value().description));
		m_table->setItem(row, 2, new QTableWidgetItem(keySequenceToString(it.value().currentSequence)));
		
		// Store action name in item data
		m_table->item(row, 0)->setData(Qt::UserRole, it.key());
	}
	
	m_table->resizeColumnsToContents();
}

QString KeybindEditor::keySequenceToString(const QKeySequence& seq) const {
	if (seq.isEmpty()) {
		return QStringLiteral("(none)");
	}
	return seq.toString();
}

void KeybindEditor::onItemSelectionChanged() {
	QList<QTableWidgetItem*> items = m_table->selectedItems();
	if (items.isEmpty()) {
		m_currentAction.clear();
		m_assignBtn->setEnabled(false);
		m_resetBtn->setEnabled(false);
		m_statusLabel->clear();
		return;
	}
	
	QTableWidgetItem* item = items.first();
	m_currentAction = item->data(Qt::UserRole).toString();
	m_assignBtn->setEnabled(true);
	m_resetBtn->setEnabled(true);
	
	if (m_keybinds.contains(m_currentAction)) {
		KeybindAction action = m_keybinds[m_currentAction];
		m_statusLabel->setText(QStringLiteral("Default: %1").arg(keySequenceToString(action.defaultSequence)));
	}
}

void KeybindEditor::onAssignKeybind() {
	if (m_currentAction.isEmpty()) return;
	
	QKeySequenceEdit* keyEdit = new QKeySequenceEdit(this);
	keyEdit->setKeySequence(m_keybinds[m_currentAction].currentSequence);
	
	QDialog dlg(this);
	dlg.setWindowTitle(QStringLiteral("Assign Keybind"));
	auto* layout = new QVBoxLayout(&dlg);
	layout->addWidget(new QLabel(QStringLiteral("Press the key combination:")));
	layout->addWidget(keyEdit);
	
	auto* btnLayout = new QHBoxLayout();
	auto* okBtn = new QPushButton(QStringLiteral("OK"), &dlg);
	auto* cancelBtn = new QPushButton(QStringLiteral("Cancel"), &dlg);
	btnLayout->addStretch();
	btnLayout->addWidget(okBtn);
	btnLayout->addWidget(cancelBtn);
	layout->addLayout(btnLayout);
	
	connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
	connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
	
	if (dlg.exec() == QDialog::Accepted) {
		QKeySequence newSeq = keyEdit->keySequence();
		
		// Check for conflicts
		for (auto it = m_keybinds.begin(); it != m_keybinds.end(); ++it) {
			if (it.key() != m_currentAction && it.value().currentSequence == newSeq && !newSeq.isEmpty()) {
				QMessageBox::warning(this, QStringLiteral("Conflict"), 
					QStringLiteral("This keybind is already assigned to: %1").arg(it.value().name));
				return;
			}
		}
		
		m_keybinds[m_currentAction].currentSequence = newSeq;
		updateKeybindDisplay();
	}
}

void KeybindEditor::onResetKeybind() {
	if (m_currentAction.isEmpty()) return;
	
	m_keybinds[m_currentAction].currentSequence = m_keybinds[m_currentAction].defaultSequence;
	updateKeybindDisplay();
}

void KeybindEditor::onResetAll() {
	int ret = QMessageBox::question(this, QStringLiteral("Reset All Keybinds"),
		QStringLiteral("Reset all keybinds to their default values?"),
		QMessageBox::Yes | QMessageBox::No);
	
	if (ret == QMessageBox::Yes) {
		for (auto it = m_keybinds.begin(); it != m_keybinds.end(); ++it) {
			it.value().currentSequence = it.value().defaultSequence;
		}
		updateKeybindDisplay();
	}
}

void KeybindEditor::onFilterChanged(const QString& text) {
	Q_UNUSED(text);
	updateKeybindDisplay();
}
