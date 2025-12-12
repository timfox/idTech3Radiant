#include "mess_dialog.h"
#include "mess_manager.h"
#include "mess_template.h"

#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QComboBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QHeaderView>
#include <QDoubleSpinBox>
#include <QSet>

MESSDialog::MESSDialog(MESSManager* manager, QWidget* parent)
	: QDialog(parent), m_manager(manager) {
	setWindowTitle(QStringLiteral("MESS Template Browser"));
	setModal(true);
	resize(800, 600);
	
	setupUI();
	populateTemplates();
}

void MESSDialog::setupUI() {
	auto* mainLayout = new QVBoxLayout(this);
	
	// Category filter
	auto* categoryLayout = new QHBoxLayout();
	categoryLayout->addWidget(new QLabel(QStringLiteral("Category:"), this));
	m_categoryCombo = new QComboBox(this);
	m_categoryCombo->addItem(QStringLiteral("All"));
	categoryLayout->addWidget(m_categoryCombo);
	categoryLayout->addStretch();
	mainLayout->addLayout(categoryLayout);
	
	// Template list
	m_templateList = new QTreeWidget(this);
	m_templateList->setColumnCount(2);
	m_templateList->setHeaderLabels({QStringLiteral("Template"), QStringLiteral("Description")});
	m_templateList->setRootIsDecorated(false);
	mainLayout->addWidget(m_templateList);
	
	// Variable editor
	mainLayout->addWidget(new QLabel(QStringLiteral("Template Variables:"), this));
	m_variableTable = new QTableWidget(this);
	m_variableTable->setColumnCount(2);
	m_variableTable->setHorizontalHeaderLabels({QStringLiteral("Variable"), QStringLiteral("Value")});
	m_variableTable->horizontalHeader()->setStretchLastSection(true);
	mainLayout->addWidget(m_variableTable);
	
	// Position
	auto* posLayout = new QHBoxLayout();
	posLayout->addWidget(new QLabel(QStringLiteral("Position:"), this));
	m_posXEdit = new QLineEdit(QStringLiteral("0"), this);
	m_posYEdit = new QLineEdit(QStringLiteral("0"), this);
	m_posZEdit = new QLineEdit(QStringLiteral("0"), this);
	posLayout->addWidget(new QLabel("X:", this));
	posLayout->addWidget(m_posXEdit);
	posLayout->addWidget(new QLabel("Y:", this));
	posLayout->addWidget(m_posYEdit);
	posLayout->addWidget(new QLabel("Z:", this));
	posLayout->addWidget(m_posZEdit);
	mainLayout->addLayout(posLayout);
	
	// Buttons
	auto* buttonLayout = new QHBoxLayout();
	buttonLayout->addStretch();
	auto* instantiateBtn = new QPushButton(QStringLiteral("Instantiate"), this);
	auto* cancelBtn = new QPushButton(QStringLiteral("Cancel"), this);
	buttonLayout->addWidget(instantiateBtn);
	buttonLayout->addWidget(cancelBtn);
	mainLayout->addLayout(buttonLayout);
	
	// Connections
	connect(m_templateList, &QTreeWidget::itemSelectionChanged, this, [this]() {
		QList<QTreeWidgetItem*> items = m_templateList->selectedItems();
		if (!items.isEmpty()) {
			m_selectedTemplate = items.first()->text(0);
			updateVariableEditor();
		}
	});
	connect(m_categoryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MESSDialog::onCategoryChanged);
	connect(m_variableTable, &QTableWidget::cellChanged, this, &MESSDialog::onVariableChanged);
	connect(instantiateBtn, &QPushButton::clicked, this, &MESSDialog::onInstantiate);
	connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void MESSDialog::populateTemplates() {
	m_templateList->clear();
	
	QString category = m_categoryCombo->currentText();
	QVector<MESSTemplate*> templates;
	
	if (category == "All") {
		// Get all templates
		QMap<QString, MESSTemplate*> allTemplates = m_manager->templates();
		for (auto it = allTemplates.begin(); it != allTemplates.end(); ++it) {
			templates.append(it.value());
		}
	} else {
		templates = m_manager->getTemplatesByCategory(category);
	}
	
	for (MESSTemplate* template_ : templates) {
		auto* item = new QTreeWidgetItem(m_templateList);
		item->setText(0, template_->name());
		item->setText(1, template_->description());
		item->setData(0, Qt::UserRole, template_->name());
	}
	
	// Populate categories
	m_categoryCombo->clear();
	m_categoryCombo->addItem(QStringLiteral("All"));
	for (const QString& cat : m_manager->getCategories()) {
		if (!cat.isEmpty()) {
			m_categoryCombo->addItem(cat);
		}
	}
}

void MESSDialog::updateVariableEditor() {
	m_variableTable->setRowCount(0);
	
	if (m_selectedTemplate.isEmpty()) return;
	
	MESSTemplate* template_ = m_manager->getTemplate(m_selectedTemplate);
	if (!template_) return;
	
	QMap<QString, QString> vars = template_->variables();
	for (auto it = vars.begin(); it != vars.end(); ++it) {
		int row = m_variableTable->rowCount();
		m_variableTable->insertRow(row);
		m_variableTable->setItem(row, 0, new QTableWidgetItem(it.key()));
		m_variableTable->setItem(row, 1, new QTableWidgetItem(it.value()));
	}
}

QString MESSDialog::selectedTemplate() const {
	return m_selectedTemplate;
}

QMap<QString, QString> MESSDialog::templateVariables() const {
	QMap<QString, QString> vars;
	
	for (int i = 0; i < m_variableTable->rowCount(); ++i) {
		QTableWidgetItem* nameItem = m_variableTable->item(i, 0);
		QTableWidgetItem* valueItem = m_variableTable->item(i, 1);
		if (nameItem && valueItem) {
			vars[nameItem->text()] = valueItem->text();
		}
	}
	
	return vars;
}

QVector3D MESSDialog::instantiationPosition() const {
	return QVector3D(
		m_posXEdit->text().toFloat(),
		m_posYEdit->text().toFloat(),
		m_posZEdit->text().toFloat()
	);
}

void MESSDialog::onTemplateSelected(QTreeWidgetItem* item, int column) {
	Q_UNUSED(item);
	Q_UNUSED(column);
	updateVariableEditor();
}

void MESSDialog::onInstantiate() {
	if (m_selectedTemplate.isEmpty()) {
		return;
	}
	
	accept();
}

void MESSDialog::onVariableChanged() {
	// Variables updated
}

void MESSDialog::onCategoryChanged(int index) {
	Q_UNUSED(index);
	populateTemplates();
}
