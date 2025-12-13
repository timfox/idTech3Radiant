#include "model_browser.h"

#include <QListWidget>
#include <QListWidgetItem>
#include <QTabWidget>
#include <QLineEdit>
#include <QLabel>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QDirIterator>
#include <QFileInfo>
#include <QStandardPaths>
#include <QSplitter>
#include <QWidget>

ModelBrowser::ModelBrowser(QWidget* parent)
	: QDialog(parent)
{
	setWindowTitle(QStringLiteral("Model Browser"));
	setModal(true);
	setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
	              Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
	setSizeGripEnabled(true);
	resize(800, 600);
	
	auto* mainLayout = new QVBoxLayout(this);
	
	// Filter
	auto* filterLayout = new QHBoxLayout();
	filterLayout->addWidget(new QLabel(QStringLiteral("Filter:")));
	m_filterEdit = new QLineEdit(this);
	m_filterEdit->setPlaceholderText(QStringLiteral("Space-separated terms, use ! for negation (e.g., 'debris 128' or '!player')"));
	filterLayout->addWidget(m_filterEdit);
	mainLayout->addLayout(filterLayout);
	
	// Splitter for multi-panel display
	auto* splitter = new QSplitter(Qt::Horizontal, this);
	
	// Model list
	m_modelList = new QListWidget(this);
	m_modelList->setViewMode(QListWidget::ListMode);
	splitter->addWidget(m_modelList);
	
	// Info panel
	auto* infoWidget = new QWidget(this);
	auto* infoLayout = new QVBoxLayout(infoWidget);
	
	m_tabs = new QTabWidget(this);
	
	// Model info tab
	auto* infoTab = new QWidget(this);
	auto* infoTabLayout = new QVBoxLayout(infoTab);
	m_modelInfoLabel = new QLabel(QStringLiteral("Select a model to view details"), this);
	m_modelInfoLabel->setWordWrap(true);
	infoTabLayout->addWidget(m_modelInfoLabel);
	m_tabs->addTab(infoTab, QStringLiteral("Info"));
	
	// Attachments tab
	auto* attachmentsTab = new QWidget(this);
	auto* attachmentsLayout = new QVBoxLayout(attachmentsTab);
	m_attachmentsText = new QTextEdit(this);
	m_attachmentsText->setReadOnly(true);
	m_attachmentsText->setPlaceholderText(QStringLiteral("Model attachments will appear here"));
	attachmentsLayout->addWidget(m_attachmentsText);
	m_tabs->addTab(attachmentsTab, QStringLiteral("Attachments"));
	
	infoLayout->addWidget(m_tabs);
	splitter->addWidget(infoWidget);
	
	splitter->setStretchFactor(0, 2);
	splitter->setStretchFactor(1, 1);
	
	mainLayout->addWidget(splitter);
	
	// Buttons
	auto* buttonLayout = new QHBoxLayout();
	buttonLayout->addStretch();
	auto* okBtn = new QPushButton(QStringLiteral("OK"), this);
	auto* cancelBtn = new QPushButton(QStringLiteral("Cancel"), this);
	buttonLayout->addWidget(okBtn);
	buttonLayout->addWidget(cancelBtn);
	mainLayout->addLayout(buttonLayout);
	
	// Connections
	connect(m_modelList, &QListWidget::itemClicked, this, &ModelBrowser::onModelSelected);
	connect(m_filterEdit, &QLineEdit::textChanged, this, &ModelBrowser::onFilterChanged);
	connect(m_tabs, &QTabWidget::currentChanged, this, &ModelBrowser::onTabChanged);
	connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
	connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
	
	populateModels();
	updateModelList();
}

void ModelBrowser::populateModels() {
	m_allModels.clear();
	
	// Search for models in typical Quake 3 locations
	QStringList searchPaths = {
		QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.q3a/baseq3/models",
		"./baseq3/models",
		"./mymod/models"
	};
	
	for (const QString& path : searchPaths) {
		QDirIterator it(path, QStringList() << "*.md3" << "*.md2" << "*.obj", QDir::Files, QDirIterator::Subdirectories);
		while (it.hasNext()) {
			QString modelPath = it.next();
			if (!m_allModels.contains(modelPath)) {
				m_allModels.append(modelPath);
			}
		}
	}
	
	// Sort models
	m_allModels.sort();
}

void ModelBrowser::updateModelList() {
	QString filter = m_filterEdit->text();
	QStringList filtered = filterModels(m_allModels, filter);
	
	m_modelList->clear();
	
	for (const QString& modelPath : filtered) {
		QFileInfo info(modelPath);
		auto* item = new QListWidgetItem(info.fileName(), m_modelList);
		item->setData(Qt::UserRole, modelPath);
		item->setToolTip(modelPath);
	}
}

QStringList ModelBrowser::filterModels(const QStringList& models, const QString& filter) const {
	if (filter.isEmpty()) {
		return models;
	}
	
	QStringList terms = filter.split(' ', Qt::SkipEmptyParts);
	QStringList result;
	
	for (const QString& modelPath : models) {
		QString lowerPath = modelPath.toLower();
		bool matches = true;
		
		for (const QString& term : terms) {
			bool negated = term.startsWith('!');
			QString searchTerm = negated ? term.mid(1).toLower() : term.toLower();
			bool contains = lowerPath.contains(searchTerm);
			
			if (negated) {
				if (contains) {
					matches = false;
					break;
				}
			} else {
				if (!contains) {
					matches = false;
					break;
				}
			}
		}
		
		if (matches) {
			result.append(modelPath);
		}
	}
	
	return result;
}

void ModelBrowser::onModelSelected(QListWidgetItem* item) {
	Q_UNUSED(item);
	if (!item) {
		m_selectedModel.clear();
		return;
	}
	
	m_selectedModel = item->data(Qt::UserRole).toString();
	updateModelInfo(m_selectedModel);
}

void ModelBrowser::onFilterChanged(const QString& text) {
	Q_UNUSED(text);
	updateModelList();
}

void ModelBrowser::onTabChanged(int index) {
	if (index == 1 && !m_selectedModel.isEmpty()) {
		// Attachments tab - update with model attachments
		// In a full implementation, we'd parse the model file for attachments
		QFileInfo info(m_selectedModel);
		m_attachmentsText->setPlainText(QStringLiteral("Attachments for: %1\n\n(In full implementation, this would show model attachment points)").arg(info.fileName()));
	}
}

void ModelBrowser::updateModelInfo(const QString& modelPath) {
	if (modelPath.isEmpty()) {
		m_modelInfoLabel->setText(QStringLiteral("Select a model to view details"));
		return;
	}
	
	QFileInfo info(modelPath);
	QString infoText = QStringLiteral(
		"<b>Model:</b> %1<br>"
		"<b>Path:</b> %2<br>"
		"<b>Size:</b> %3 bytes<br>"
		"<b>Modified:</b> %4<br>"
		"<b>Type:</b> %5"
	).arg(info.fileName())
	 .arg(info.absolutePath())
	 .arg(info.size())
	 .arg(info.lastModified().toString())
	 .arg(info.suffix().toUpper());
	
	m_modelInfoLabel->setText(infoText);
}

QString ModelBrowser::getModel(const QString& initial, QWidget* parent) {
	ModelBrowser dlg(parent);
	if (!initial.isEmpty()) {
		// Try to select initial model
		for (int i = 0; i < dlg.m_modelList->count(); ++i) {
			QListWidgetItem* item = dlg.m_modelList->item(i);
			if (item->data(Qt::UserRole).toString() == initial) {
				dlg.m_modelList->setCurrentItem(item);
				break;
			}
		}
	}
	
	if (dlg.exec() == QDialog::Accepted) {
		return dlg.selectedModel();
	}
	return QString();
}
