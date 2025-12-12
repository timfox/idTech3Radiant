#pragma once

#include <QString>
#include <QVector3D>

// OpenAL audio player for 3D positional audio in editor
class AudioPlayer {
public:
	AudioPlayer();
	~AudioPlayer();
	
	// Initialization
	bool initialize();
	void shutdown();
	bool isInitialized() const { return m_initialized; }
	
	// Playback
	bool loadSound(const QString& filePath);
	bool playSound(const QString& filePath, const QVector3D& position = QVector3D(), bool loop = false);
	void stopSound();
	void pauseSound();
	void resumeSound();
	
	// 3D positioning
	void setListenerPosition(const QVector3D& position);
	void setListenerOrientation(const QVector3D& forward, const QVector3D& up);
	void setSourcePosition(unsigned int sourceId, const QVector3D& position);
	
	// Volume control
	void setMasterVolume(float volume); // 0.0 to 1.0
	void setSourceVolume(unsigned int sourceId, float volume);
	
	// Status
	bool isPlaying() const { return m_isPlaying; }
	float getDuration(const QString& filePath) const; // Returns duration in seconds
	
	// Supported formats
	static bool isSupportedFormat(const QString& filePath);
	static QStringList supportedExtensions(); // Returns list like ["wav", "mp3", "ogg"]

private:
	bool m_initialized{false};
	bool m_isPlaying{false};
	
	// OpenAL context and device
	void* m_alDevice{nullptr};
	void* m_alContext{nullptr};
	
	// Current sound
	unsigned int m_currentSource{0};
	unsigned int m_currentBuffer{0};
	QString m_currentSoundPath;
	
	// Helper functions
	bool loadWAV(const QString& filePath, unsigned int& bufferId);
	bool loadMP3(const QString& filePath, unsigned int& bufferId);
	bool loadOGG(const QString& filePath, unsigned int& bufferId);
	
	// Cleanup
	void cleanupSource();
};
