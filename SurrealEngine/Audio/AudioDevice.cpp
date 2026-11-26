
#include "Precomp.h"
#include "AudioDevice.h"
#ifndef MISTER_HYBRID
#include "OpenAL/OpenALAudioDevice.h"
#endif
#include "AudioPlayer/AudioPlayerDevice.h"
#include "NullAudio/NullAudioDevice.h"
#include "LauncherSettings.h"

std::unique_ptr<AudioDevice> AudioDevice::Create(int frequency, int numVoices, int musicBufferCount, int musicBufferSize)
{
#ifdef MISTER_HYBRID
	// the engine's own mixer, played through SDL (the core's audio)
	if (LauncherSettings::Get().AudioDevice.Type == AudioDeviceType::NullAudio)
		return std::make_unique<NullAudioDevice>();
	return std::make_unique<AudioPlayerDevice>();
#else
	const auto devName = LauncherSettings::Get().AudioDevice.Type;

	if (devName == AudioDeviceType::OpenAL)
		return std::make_unique<OpenALAudioDevice>(frequency, numVoices, musicBufferCount, musicBufferSize);
	else if (devName == AudioDeviceType::AudioPlayer)
		return std::make_unique<AudioPlayerDevice>();
	else
		return std::make_unique<NullAudioDevice>();
#endif
}
