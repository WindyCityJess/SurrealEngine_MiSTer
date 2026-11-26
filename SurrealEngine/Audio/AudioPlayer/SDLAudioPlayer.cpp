#include "Precomp.h"
#include "SDLAudioPlayer.h"
#include "../AudioSource.h"
#include <SDL2/SDL.h>
#include <stdexcept>
#include <cstring>

SDLAudioPlayer::SDLAudioPlayer(std::unique_ptr<AudioSource> audiosource) : source(std::move(audiosource))
{
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
		throw std::runtime_error(std::string("Could not initialize SDL audio: ") + SDL_GetError());

	SDL_AudioSpec want = {}, have = {};
	want.freq = mixing_frequency;
	want.format = AUDIO_F32SYS;
	want.channels = 2;
	want.samples = 1024;
	want.callback = &SDLAudioPlayer::AudioCallback;
	want.userdata = this;

	// The source is prepared before the device is opened: the callback runs on SDL's thread
	if (source->GetFrequency() != mixing_frequency)
		source = AudioSource::CreateResampler(mixing_frequency, std::move(source));

	device = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
	if (device == 0)
		throw std::runtime_error(std::string("Could not open sound device: ") + SDL_GetError());

	SDL_PauseAudioDevice(device, 0);
}

SDLAudioPlayer::~SDLAudioPlayer()
{
	if (device != 0)
	{
		SDL_CloseAudioDevice(device);
		device = 0;
	}
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void SDLAudioPlayer::AudioCallback(void* userdata, uint8_t* stream, int len)
{
	static_cast<SDLAudioPlayer*>(userdata)->Mix((float*)stream, (size_t)len / sizeof(float));
}

void SDLAudioPlayer::Mix(float* output, size_t count)
{
	size_t written = 0;
	if (source)
	{
		int channels = source->GetChannels();
		if (channels == 2)
		{
			written = source->ReadSamples(output, count);
		}
		else if (channels == 1)
		{
			size_t samplesread = source->ReadSamples(output, count / 2);
			for (size_t i = 0; i < samplesread; i++)
			{
				size_t src = samplesread - 1 - i;
				size_t dst = src * 2;
				output[dst] = output[dst + 1] = output[src];
			}
			written = samplesread * 2;
		}
	}
	if (written < count)
		memset(output + written, 0, (count - written) * sizeof(float));
}
