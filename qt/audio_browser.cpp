#include "audio_browser.h"

#include <QListWidget>
#include <QListWidgetItem>
#include <QLineEdit>
#include <QSlider>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QDirIterator>
#include <QFileInfo>
#include <QStandardPaths>
#include <QVariant>
#include "audio_player.h"

AudioBrowser::AudioBrowser(QWidget* parent)
	: QDialog(parent)
{
	setWindowTitle(QStringLiteral("Audio Browser"));
	setModal(true);
	resize(600, 500);
	
	auto* mainLayout = new QVBoxLayout(this);
	
	// Filter
	auto* filterLayout = new QHBoxLayout();
	filterLayout->addWidget(new QLabel(QStringLiteral("Filter:")));
	m_filterEdit = new QLineEdit(this);
	m_filterEdit->setPlaceholderText(QStringLiteral("Search sounds..."));
	filterLayout->addWidget(m_filterEdit);
	mainLayout->addLayout(filterLayout);
	
	// Volume control
	auto* volumeLayout = new QHBoxLayout();
	volumeLayout->addWidget(new QLabel(QStringLiteral("Volume:")));
	m_volumeSlider = new QSlider(Qt::Horizontal, this);
	m_volumeSlider->setRange(0, 100);
	m_volumeSlider->setValue(75);
	m_volumeLabel = new QLabel(QStringLiteral("75%"), this);
	m_volumeLabel->setMinimumWidth(40);
	volumeLayout->addWidget(m_volumeSlider);
	volumeLayout->addWidget(m_volumeLabel);
	mainLayout->addLayout(volumeLayout);
	
	// Sound list
	m_soundList = new QListWidget(this);
	mainLayout->addWidget(m_soundList);
	
	// Playback controls
	auto* playbackLayout = new QHBoxLayout();
	auto* playBtn = new QPushButton(QStringLiteral("Play"), this);
	auto* stopBtn = new QPushButton(QStringLiteral("Stop"), this);
	playbackLayout->addWidget(playBtn);
	playbackLayout->addWidget(stopBtn);
	playbackLayout->addStretch();
	mainLayout->addLayout(playbackLayout);
	
	// Buttons
	auto* buttonLayout = new QHBoxLayout();
	buttonLayout->addStretch();
	auto* okBtn = new QPushButton(QStringLiteral("OK"), this);
	auto* cancelBtn = new QPushButton(QStringLiteral("Cancel"), this);
	buttonLayout->addWidget(okBtn);
	buttonLayout->addWidget(cancelBtn);
	mainLayout->addLayout(buttonLayout);
	
	// Connections
	connect(m_soundList, &QListWidget::itemClicked, this, &AudioBrowser::onSoundSelected);
	connect(m_filterEdit, &QLineEdit::textChanged, this, &AudioBrowser::onFilterChanged);
	connect(m_volumeSlider, &QSlider::valueChanged, this, &AudioBrowser::onVolumeChanged);
	connect(playBtn, &QPushButton::clicked, this, &AudioBrowser::onPlaySound);
	connect(stopBtn, &QPushButton::clicked, this, &AudioBrowser::onStopSound);
	connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
	connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
	
	populateSounds();
	updateSoundList();
}

AudioBrowser::~AudioBrowser() {
	shutdownAudio();
	delete m_audioPlayer;
}

void AudioBrowser::populateSounds() {
	m_allSounds.clear();
	
	// Search for sounds in typical Quake 3 locations
	QStringList searchPaths = {
		QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.q3a/baseq3/sound",
		"./baseq3/sound",
		"./mymod/sound"
	};
	
	// Use AudioPlayer to get supported extensions
	QStringList extensions = AudioPlayer::supportedExtensions();
	
	for (const QString& path : searchPaths) {
		for (const QString& ext : extensions) {
			QDirIterator it(path, QStringList() << QString("*.%1").arg(ext), QDir::Files, QDirIterator::Subdirectories);
			while (it.hasNext()) {
				QString soundPath = it.next();
				if (!m_allSounds.contains(soundPath)) {
					m_allSounds.append(soundPath);
				}
			}
		}
	}
	
	// Sort sounds
	m_allSounds.sort();
}

void AudioBrowser::updateSoundList() {
	QString filter = m_filterEdit->text().toLower();
	QStringList filtered = filterSounds(m_allSounds, filter);
	
	m_soundList->clear();
	
	for (const QString& soundPath : filtered) {
		QFileInfo info(soundPath);
		auto* item = new QListWidgetItem(info.fileName(), m_soundList);
		item->setData(Qt::UserRole, soundPath);
		item->setToolTip(soundPath);
	}
}

QStringList AudioBrowser::filterSounds(const QStringList& sounds, const QString& filter) const {
	if (filter.isEmpty()) {
		return sounds;
	}
	
	QStringList result;
	for (const QString& soundPath : sounds) {
		if (soundPath.toLower().contains(filter)) {
			result.append(soundPath);
		}
	}
	
	return result;
}

void AudioBrowser::onSoundSelected(QListWidgetItem* item) {
	if (!item) {
		m_selectedSound.clear();
		return;
	}
	
	m_selectedSound = item->data(Qt::UserRole).toString();
}

void AudioBrowser::onFilterChanged(const QString& text) {
	Q_UNUSED(text);
	updateSoundList();
}

void AudioBrowser::onVolumeChanged(int value) {
	m_volumeLabel->setText(QStringLiteral("%1%").arg(value));
	if (m_audioPlayer) {
		float volume = value / 100.0f;
		m_audioPlayer->setMasterVolume(volume);
	}
}

void AudioBrowser::onPlaySound() {
	if (m_selectedSound.isEmpty() || !m_audioPlayer) {
		return;
	}
	
	// Play sound at origin (3D positional audio)
	if (m_audioPlayer->playSound(m_selectedSound, QVector3D(0, 0, 0), false)) {
		m_isPlaying = true;
	}
}

void AudioBrowser::onStopSound() {
	if (m_audioPlayer && m_isPlaying) {
		m_audioPlayer->stopSound();
		m_isPlaying = false;
	}
}

bool AudioBrowser::initializeAudio() {
	if (m_audioPlayer) {
		return m_audioPlayer->initialize();
	}
	return false;
}

void AudioBrowser::shutdownAudio() {
	if (m_audioPlayer) {
		m_audioPlayer->shutdown();
	}
}

QString AudioBrowser::getSound(const QString& initial, QWidget* parent) {
	AudioBrowser dlg(parent);
	if (!initial.isEmpty()) {
		// Try to select initial sound
		for (int i = 0; i < dlg.m_soundList->count(); ++i) {
			QListWidgetItem* item = dlg.m_soundList->item(i);
			if (item->data(Qt::UserRole).toString() == initial) {
				dlg.m_soundList->setCurrentItem(item);
				break;
			}
		}
	}
	
	if (dlg.exec() == QDialog::Accepted) {
		return dlg.selectedSound();
	}
	return QString();
}
