#pragma once

// Sound output through SDL2 (MiSTer hybrid core: SDL's "mister" audio driver,
// paced by the core's sample clock)

#include "AudioPlayer.h"
#include <mutex>
#include <vector>
#include <cstdint>

class SDLAudioPlayer : public AudioPlayer
{
public:
	SDLAudioPlayer(std::unique_ptr<AudioSource> audiosource);
	~SDLAudioPlayer();

	int GetMixingFrequency() const override { return mixing_frequency; }

private:
	static void AudioCallback(void* userdata, uint8_t* stream, int len);
	void Mix(float* output, size_t count);

	std::unique_ptr<AudioSource> source;
	uint32_t device = 0;
	int mixing_frequency = 44100;
	std::vector<float> monobuffer;
};
