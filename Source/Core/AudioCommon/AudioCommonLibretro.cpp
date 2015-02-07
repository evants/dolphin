// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.


#include "AudioCommon/AudioCommon.h"
#include "AudioCommon/Mixer.h"
#include "AudioCommon/NullSoundStream.h"

#include "Common/FileUtil.h"

#include "Core/ConfigManager.h"
#include "Core/Movie.h"

// This shouldn't be a global, at least not here.
SoundStream* g_sound_stream = nullptr;

static bool s_audio_dump_start = false;

namespace AudioCommon
{
	static const int AUDIO_VOLUME_MIN = 0;
	static const int AUDIO_VOLUME_MAX = 100;

	SoundStream* InitSoundStream()
	{
		CMixer *mixer = new CMixer(48000);

		// TODO: possible memleak with mixer

		std::string backend = SConfig::GetInstance().sBackend;
      g_sound_stream = new NullSound(mixer);

		if (!g_sound_stream && NullSound::isValid())
		{
			WARN_LOG(DSPHLE, "Could not initialize backend %s, using %s instead.",
				backend.c_str(), BACKEND_NULLSOUND);
			g_sound_stream = new NullSound(mixer);
		}

		if (g_sound_stream)
		{
			UpdateSoundStream();
			if (g_sound_stream->Start())
			{
				if (SConfig::GetInstance().m_DumpAudio && !s_audio_dump_start)
					StartAudioDump();

				return g_sound_stream;
			}
			PanicAlertT("Could not initialize backend %s.", backend.c_str());
		}

		PanicAlertT("Sound backend %s is not valid.", backend.c_str());

		delete g_sound_stream;
		g_sound_stream = nullptr;
		return nullptr;
	}

	void ShutdownSoundStream()
	{
		INFO_LOG(DSPHLE, "Shutting down sound stream");

		if (g_sound_stream)
		{
			g_sound_stream->Stop();
			if (SConfig::GetInstance().m_DumpAudio && s_audio_dump_start)
				StopAudioDump();
			delete g_sound_stream;
			g_sound_stream = nullptr;
		}

		INFO_LOG(DSPHLE, "Done shutting down sound stream");
	}

	std::vector<std::string> GetSoundBackends()
	{
		std::vector<std::string> backends;

      backends.push_back(BACKEND_NULLSOUND);
		return backends;
	}

	void PauseAndLock(bool doLock, bool unpauseOnUnlock)
	{
		if (g_sound_stream)
		{
			// audio typically doesn't maintain its own "paused" state
			// (that's already handled by the CPU and whatever else being paused)
			// so it should be good enough to only lock/unlock here.
			CMixer* pMixer = g_sound_stream->GetMixer();
			if (pMixer)
			{
				std::mutex& csMixing = pMixer->MixerCritical();
				if (doLock)
					csMixing.lock();
				else
					csMixing.unlock();
			}
		}
	}

	void UpdateSoundStream()
	{
		if (g_sound_stream)
		{
			int volume = SConfig::GetInstance().m_IsMuted ? 0 : SConfig::GetInstance().m_Volume;
			g_sound_stream->SetVolume(volume);
		}
	}

	void ClearAudioBuffer(bool mute)
	{
		if (g_sound_stream)
			g_sound_stream->Clear(mute);
	}

	void SendAIBuffer(short *samples, unsigned int num_samples)
	{
		if (!g_sound_stream)
			return;

		if (SConfig::GetInstance().m_DumpAudio && !s_audio_dump_start)
			StartAudioDump();
		else if (!SConfig::GetInstance().m_DumpAudio && s_audio_dump_start)
			StopAudioDump();

		CMixer* pMixer = g_sound_stream->GetMixer();

		if (pMixer && samples)
		{
			pMixer->PushSamples(samples, num_samples);
		}

		g_sound_stream->Update();
	}

	void StartAudioDump()
	{
		std::string audio_file_name_dtk = File::GetUserPath(D_DUMPAUDIO_IDX) + "dtkdump.wav";
		std::string audio_file_name_dsp = File::GetUserPath(D_DUMPAUDIO_IDX) + "dspdump.wav";
		File::CreateFullPath(audio_file_name_dtk);
		File::CreateFullPath(audio_file_name_dsp);
		g_sound_stream->GetMixer()->StartLogDTKAudio(audio_file_name_dtk);
		g_sound_stream->GetMixer()->StartLogDSPAudio(audio_file_name_dsp);
		s_audio_dump_start = true;
	}

	void StopAudioDump()
	{
		g_sound_stream->GetMixer()->StopLogDTKAudio();
		g_sound_stream->GetMixer()->StopLogDSPAudio();
		s_audio_dump_start = false;
	}

	void IncreaseVolume(unsigned short offset)
	{
		SConfig::GetInstance().m_IsMuted = false;
		int& currentVolume = SConfig::GetInstance().m_Volume;
		currentVolume += offset;
		if (currentVolume > AUDIO_VOLUME_MAX)
			currentVolume = AUDIO_VOLUME_MAX;
		UpdateSoundStream();
	}

	void DecreaseVolume(unsigned short offset)
	{
		SConfig::GetInstance().m_IsMuted = false;
		int& currentVolume = SConfig::GetInstance().m_Volume;
		currentVolume -= offset;
		if (currentVolume < AUDIO_VOLUME_MIN)
			currentVolume = AUDIO_VOLUME_MIN;
		UpdateSoundStream();
	}

	void ToggleMuteVolume()
	{
		bool& isMuted = SConfig::GetInstance().m_IsMuted;
		isMuted = !isMuted;
		UpdateSoundStream();
	}
}
