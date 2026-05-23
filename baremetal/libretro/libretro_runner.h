#ifndef RA_BAREMETAL_LIBRETRO_RUNNER_H
#define RA_BAREMETAL_LIBRETRO_RUNNER_H

#include <stddef.h>
#include <stdint.h>

#include <libretro.h>

class CircleAudio;
class CircleFs;
class CircleInput;
class CircleLog;
class CircleTimer;
class CircleVideo;

struct LibretroCore
{
	const char *name;
	size_t maxRomSize;
	bool n64Options;

	void (*init)(void);
	void (*deinit)(void);
	unsigned (*api_version)(void);
	void (*get_system_info)(struct retro_system_info *info);
	void (*get_system_av_info)(struct retro_system_av_info *info);
	void (*set_environment)(retro_environment_t cb);
	void (*set_video_refresh)(retro_video_refresh_t cb);
	void (*set_audio_sample)(retro_audio_sample_t cb);
	void (*set_audio_sample_batch)(retro_audio_sample_batch_t cb);
	void (*set_input_poll)(retro_input_poll_t cb);
	void (*set_input_state)(retro_input_state_t cb);
	void (*run)(void);
	bool (*load_game)(const struct retro_game_info *game);
	void (*unload_game)(void);
};

const LibretroCore *LibretroFindCoreForPath(const char *path);
const LibretroCore *LibretroDefaultCore(void);

class LibretroRunner
{
public:
	typedef void (*TProgressCallback)(void *pContext, const char *pMessage);

	LibretroRunner(CircleLog *pLog,
	               CircleTimer *pTimer,
	               CircleVideo *pVideo,
	               CircleAudio *pAudio,
	               CircleInput *pInput,
	               CircleFs *pFs);
	~LibretroRunner(void);

	bool Init(const char *romPath);
	bool Init(const LibretroCore *pCore, const char *romPath);
	void SetProgressCallback(void *pContext, TProgressCallback pCallback);
	void RunFrame(void);
	double FramesPerSecond(void) const;
	unsigned SampleRate(void) const;

private:
	void Progress(const char *pMessage);
	void ProgressValue(const char *pLabel, unsigned nValue);
	bool Environment(unsigned cmd, void *data);
	void PrepareGameInfoExt(const char *romPath);
	void VideoRefresh(const void *data, unsigned width, unsigned height, size_t pitch);
	void AudioSample(int16_t left, int16_t right);
	size_t AudioSampleBatch(const int16_t *data, size_t frames);
	void InputPoll(void);
	int16_t InputState(unsigned port, unsigned device, unsigned index, unsigned id);

	static bool EnvironmentThunk(unsigned cmd, void *data);
	static void LogPrintfThunk(enum retro_log_level level, const char *format, ...);
	static void VideoRefreshThunk(const void *data, unsigned width, unsigned height, size_t pitch);
	static void AudioSampleThunk(int16_t left, int16_t right);
	static size_t AudioSampleBatchThunk(const int16_t *data, size_t frames);
	static void InputPollThunk(void);
	static int16_t InputStateThunk(unsigned port, unsigned device, unsigned index, unsigned id);

private:
	CircleLog *m_pLog;
	CircleTimer *m_pTimer;
	CircleVideo *m_pVideo;
	CircleAudio *m_pAudio;
	CircleInput *m_pInput;
	CircleFs *m_pFs;
	const LibretroCore *m_pCore;
	void *m_pProgressContext;
	TProgressCallback m_pProgressCallback;

	enum retro_pixel_format m_PixelFormat;
	bool m_SupportsNoGame;
	uint8_t *m_pRomData;
	size_t m_RomSize;
	struct retro_system_av_info m_AvInfo;
	struct retro_game_info_ext m_GameInfoExt;
	char m_GameFullPath[256];
	char m_GameDir[64];
	char m_GameName[128];
	char m_GameExt[16];

	static LibretroRunner *s_pActive;
};

#endif
