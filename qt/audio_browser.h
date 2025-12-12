#pragma once

#include <QDialog>
#include <QString>
#include <QStringList>

QT_BEGIN_NAMESPACE
class QListWidget;
class QListWidgetItem;
class QLineEdit;
class QSlider;
class QLabel;
QT_END_NAMESPACE

class AudioPlayer;

class AudioBrowser : public QDialog {
	Q_OBJECT
public:
	explicit AudioBrowser(QWidget* parent = nullptr);
	~AudioBrowser();
	QString selectedSound() const { return m_selectedSound; }
	
	static QString getSound(const QString& initial = QString(), QWidget* parent = nullptr);

private Q_SLOTS:
	void onSoundSelected(QListWidgetItem* item);
	void onFilterChanged(const QString& text);
	void onVolumeChanged(int value);
	void onPlaySound();
	void onStopSound();

private:
	void populateSounds();
	void updateSoundList();
	QStringList filterSounds(const QStringList& sounds, const QString& filter) const;
	
	QListWidget* m_soundList = nullptr;
	QLineEdit* m_filterEdit = nullptr;
	QSlider* m_volumeSlider = nullptr;
	QLabel* m_volumeLabel = nullptr;
	
	QStringList m_allSounds;
	QString m_selectedSound;
	
	// Audio playback with OpenAL
	AudioPlayer* m_audioPlayer = nullptr;
	bool m_isPlaying{false};
	
	// Initialize audio system
	bool initializeAudio();
	void shutdownAudio();
};
