#include "audio_player.h"

#include <QFileInfo>
#include <QStringList>
#include <QDebug>
#include <cmath>
#include <algorithm>

// OpenAL includes would go here
// For now, using stub implementation
#ifdef HAVE_OPENAL
#include <AL/al.h>
#include <AL/alc.h>
#else
// Stub definitions
typedef unsigned int ALuint;
typedef int ALenum;
#define AL_NONE 0
#define AL_SOURCE_STATE 0x1010
#define AL_PLAYING 0x1012
#define AL_STOPPED 0x1014
#define AL_PAUSED 0x1015
#define AL_POSITION 0x1004
#define AL_GAIN 0x100A
#define AL_LOOPING 0x1007
#define AL_TRUE 1
#define AL_FALSE 0
#endif

AudioPlayer::AudioPlayer() {
}

AudioPlayer::~AudioPlayer() {
	shutdown();
}

bool AudioPlayer::initialize() {
	if (m_initialized) return true;
	
#ifdef HAVE_OPENAL
	// Initialize OpenAL device
	m_alDevice = alcOpenDevice(nullptr);
	if (!m_alDevice) {
		return false;
	}
	
	// Create context
	m_alContext = alcCreateContext(static_cast<ALCdevice*>(m_alDevice), nullptr);
	if (!m_alContext) {
		alcCloseDevice(static_cast<ALCdevice*>(m_alDevice));
		m_alDevice = nullptr;
		return false;
	}
	
	// Make context current
	alcMakeContextCurrent(static_cast<ALCcontext*>(m_alContext));
	
	m_initialized = true;
	return true;
#else
	// Stub implementation
	m_initialized = true;
	return true;
#endif
}

void AudioPlayer::shutdown() {
	if (!m_initialized) return;
	
	stopSound();
	cleanupSource();
	
#ifdef HAVE_OPENAL
	if (m_alContext) {
		alcMakeContextCurrent(nullptr);
		alcDestroyContext(static_cast<ALCcontext*>(m_alContext));
		m_alContext = nullptr;
	}
	
	if (m_alDevice) {
		alcCloseDevice(static_cast<ALCdevice*>(m_alDevice));
		m_alDevice = nullptr;
	}
#endif
	
	m_initialized = false;
}

bool AudioPlayer::loadSound(const QString& filePath) {
	if (!m_initialized) return false;
	
	QFileInfo info(filePath);
	QString ext = info.suffix().toLower();
	
	unsigned int bufferId = 0;
	bool success = false;
	
	if (ext == "wav") {
		success = loadWAV(filePath, bufferId);
	} else if (ext == "mp3") {
		success = loadMP3(filePath, bufferId);
	} else if (ext == "ogg") {
		success = loadOGG(filePath, bufferId);
	}
	
	if (success) {
		cleanupSource();
		m_currentBuffer = bufferId;
		m_currentSoundPath = filePath;
		return true;
	}
	
	return false;
}

bool AudioPlayer::playSound(const QString& filePath, const QVector3D& position, bool loop) {
	if (!m_initialized) return false;
	
	// Load sound if not already loaded
	if (m_currentSoundPath != filePath) {
		if (!loadSound(filePath)) {
			return false;
		}
	}
	
#ifdef HAVE_OPENAL
	// Generate source
	alGenSources(1, &m_currentSource);
	
	// Attach buffer to source
	alSourcei(m_currentSource, AL_BUFFER, m_currentBuffer);
	
	// Set position
	alSource3f(m_currentSource, AL_POSITION, position.x(), position.y(), position.z());
	
	// Set looping
	alSourcei(m_currentSource, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);
	
	// Play
	alSourcePlay(m_currentSource);
	m_isPlaying = true;
	return true;
#else
	// Stub implementation
	m_isPlaying = true;
	return true;
#endif
}

void AudioPlayer::stopSound() {
	if (!m_initialized || !m_isPlaying) return;
	
#ifdef HAVE_OPENAL
	if (m_currentSource) {
		alSourceStop(m_currentSource);
	}
#endif
	
	m_isPlaying = false;
}

void AudioPlayer::pauseSound() {
	if (!m_initialized || !m_isPlaying) return;
	
#ifdef HAVE_OPENAL
	if (m_currentSource) {
		alSourcePause(m_currentSource);
	}
#endif
}

void AudioPlayer::resumeSound() {
	if (!m_initialized) return;
	
#ifdef HAVE_OPENAL
	if (m_currentSource) {
		ALint state;
		alGetSourcei(m_currentSource, AL_SOURCE_STATE, &state);
		if (state == AL_PAUSED) {
			alSourcePlay(m_currentSource);
			m_isPlaying = true;
		}
	}
#endif
}

void AudioPlayer::setListenerPosition(const QVector3D& position) {
	if (!m_initialized) return;
	
#ifdef HAVE_OPENAL
	alListener3f(AL_POSITION, position.x(), position.y(), position.z());
#endif
}

void AudioPlayer::setListenerOrientation(const QVector3D& forward, const QVector3D& up) {
	if (!m_initialized) return;
	
#ifdef HAVE_OPENAL
	float orientation[6] = {
		forward.x(), forward.y(), forward.z(),
		up.x(), up.y(), up.z()
	};
	alListenerfv(AL_ORIENTATION, orientation);
#endif
}

void AudioPlayer::setSourcePosition(unsigned int sourceId, const QVector3D& position) {
	if (!m_initialized) return;
	
#ifdef HAVE_OPENAL
	alSource3f(sourceId, AL_POSITION, position.x(), position.y(), position.z());
#endif
}

void AudioPlayer::setMasterVolume(float volume) {
	if (!m_initialized) return;
	
	volume = std::max(0.0f, std::min(1.0f, volume));
	
#ifdef HAVE_OPENAL
	alListenerf(AL_GAIN, volume);
#endif
}

void AudioPlayer::setSourceVolume(unsigned int sourceId, float volume) {
	if (!m_initialized) return;
	
	volume = std::max(0.0f, std::min(1.0f, volume));
	
#ifdef HAVE_OPENAL
	alSourcef(sourceId, AL_GAIN, volume);
#endif
}

float AudioPlayer::getDuration(const QString& filePath) const {
	Q_UNUSED(filePath);
	// In full implementation, would decode file and get duration
	return 0.0f;
}

bool AudioPlayer::isSupportedFormat(const QString& filePath) {
	QFileInfo info(filePath);
	QString ext = info.suffix().toLower();
	return supportedExtensions().contains(ext);
}

QStringList AudioPlayer::supportedExtensions() {
	return QStringList() << "wav" << "mp3" << "ogg";
}

bool AudioPlayer::loadWAV(const QString& filePath, unsigned int& bufferId) {
	Q_UNUSED(filePath);
	Q_UNUSED(bufferId);
	// In full implementation, would load WAV file using OpenAL
	return false;
}

bool AudioPlayer::loadMP3(const QString& filePath, unsigned int& bufferId) {
	Q_UNUSED(filePath);
	Q_UNUSED(bufferId);
	// In full implementation, would decode MP3 and load into OpenAL buffer
	// Would need libmpg123 or similar
	return false;
}

bool AudioPlayer::loadOGG(const QString& filePath, unsigned int& bufferId) {
	Q_UNUSED(filePath);
	Q_UNUSED(bufferId);
	// In full implementation, would decode OGG and load into OpenAL buffer
	// Would need libvorbis/libogg
	return false;
}

void AudioPlayer::cleanupSource() {
#ifdef HAVE_OPENAL
	if (m_currentSource) {
		alSourceStop(m_currentSource);
		alDeleteSources(1, &m_currentSource);
		m_currentSource = 0;
	}
	
	if (m_currentBuffer) {
		alDeleteBuffers(1, &m_currentBuffer);
		m_currentBuffer = 0;
	}
#endif
}
