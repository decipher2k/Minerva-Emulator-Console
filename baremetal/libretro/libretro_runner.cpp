#include "libretro_runner.h"

#include "platform/circle/circle_platform.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifndef RA_BAREMETAL_MAX_ROM_SIZE
#define RA_BAREMETAL_MAX_ROM_SIZE (16 * 1024 * 1024)
#endif

#define RA_ENVIRONMENT_BAREMETAL_PROGRESS (RETRO_ENVIRONMENT_PRIVATE | 0x4242)

#define DECLARE_LIBRETRO_CORE(prefix) \
extern "C" { \
void prefix##_retro_init(void); \
void prefix##_retro_deinit(void); \
unsigned prefix##_retro_api_version(void); \
void prefix##_retro_get_system_info(struct retro_system_info *info); \
void prefix##_retro_get_system_av_info(struct retro_system_av_info *info); \
void prefix##_retro_set_environment(retro_environment_t cb); \
void prefix##_retro_set_video_refresh(retro_video_refresh_t cb); \
void prefix##_retro_set_audio_sample(retro_audio_sample_t cb); \
void prefix##_retro_set_audio_sample_batch(retro_audio_sample_batch_t cb); \
void prefix##_retro_set_input_poll(retro_input_poll_t cb); \
void prefix##_retro_set_input_state(retro_input_state_t cb); \
void prefix##_retro_run(void); \
bool prefix##_retro_load_game(const struct retro_game_info *game); \
void prefix##_retro_unload_game(void); \
}

#define LIBRETRO_CORE_ENTRY(displayName, prefix, maxSize, hasN64Options) \
{ \
	displayName, maxSize, hasN64Options, \
	prefix##_retro_init, \
	prefix##_retro_deinit, \
	prefix##_retro_api_version, \
	prefix##_retro_get_system_info, \
	prefix##_retro_get_system_av_info, \
	prefix##_retro_set_environment, \
	prefix##_retro_set_video_refresh, \
	prefix##_retro_set_audio_sample, \
	prefix##_retro_set_audio_sample_batch, \
	prefix##_retro_set_input_poll, \
	prefix##_retro_set_input_state, \
	prefix##_retro_run, \
	prefix##_retro_load_game, \
	prefix##_retro_unload_game \
}

#ifdef RA_BAREMETAL_MULTI
DECLARE_LIBRETRO_CORE(fceumm)
DECLARE_LIBRETRO_CORE(n64)

static const LibretroCore g_FceummCore = LIBRETRO_CORE_ENTRY("FCEUmm", fceumm, 0x1000000, false);
static const LibretroCore g_N64Core = LIBRETRO_CORE_ENTRY("Mupen64Plus-Next", n64, 0x4000000, true);
#else
extern "C" {
void retro_init(void);
void retro_deinit(void);
unsigned retro_api_version(void);
void retro_get_system_info(struct retro_system_info *info);
void retro_get_system_av_info(struct retro_system_av_info *info);
void retro_set_environment(retro_environment_t cb);
void retro_set_video_refresh(retro_video_refresh_t cb);
void retro_set_audio_sample(retro_audio_sample_t cb);
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb);
void retro_set_input_poll(retro_input_poll_t cb);
void retro_set_input_state(retro_input_state_t cb);
void retro_run(void);
bool retro_load_game(const struct retro_game_info *game);
void retro_unload_game(void);
}

static const LibretroCore g_DefaultCore = {
	"libretro", RA_BAREMETAL_MAX_ROM_SIZE,
#ifdef RA_BAREMETAL_N64
	true,
#else
	false,
#endif
	retro_init,
	retro_deinit,
	retro_api_version,
	retro_get_system_info,
	retro_get_system_av_info,
	retro_set_environment,
	retro_set_video_refresh,
	retro_set_audio_sample,
	retro_set_audio_sample_batch,
	retro_set_input_poll,
	retro_set_input_state,
	retro_run,
	retro_load_game,
	retro_unload_game
};
#endif

LibretroRunner *LibretroRunner::s_pActive = 0;

static bool StringEndsWith(const char *value, const char *suffix)
{
	if (!value || !suffix)
	{
		return false;
	}

	const size_t valueLen = strlen(value);
	const size_t suffixLen = strlen(suffix);
	if (suffixLen > valueLen)
	{
		return false;
	}

	return strcmp(value + valueLen - suffixLen, suffix) == 0;
}

static char ToLowerAscii(char c)
{
	return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c;
}

static bool StringEndsWithNoCase(const char *value, const char *suffix)
{
	if (!value || !suffix)
	{
		return false;
	}

	const size_t valueLen = strlen(value);
	const size_t suffixLen = strlen(suffix);
	if (suffixLen > valueLen)
	{
		return false;
	}

	value += valueLen - suffixLen;
	while (*suffix)
	{
		if (ToLowerAscii(*value++) != ToLowerAscii(*suffix++))
		{
			return false;
		}
	}

	return true;
}

const LibretroCore *LibretroDefaultCore(void)
{
#ifdef RA_BAREMETAL_MULTI
	return &g_FceummCore;
#else
	return &g_DefaultCore;
#endif
}

const LibretroCore *LibretroFindCoreForPath(const char *path)
{
#ifdef RA_BAREMETAL_MULTI
	if (StringEndsWithNoCase(path, ".nes"))
	{
		return &g_FceummCore;
	}
	if (StringEndsWithNoCase(path, ".z64") ||
	    StringEndsWithNoCase(path, ".n64") ||
	    StringEndsWithNoCase(path, ".v64"))
	{
		return &g_N64Core;
	}
	return 0;
#else
#ifdef RA_BAREMETAL_N64
	if (StringEndsWithNoCase(path, ".z64") ||
	    StringEndsWithNoCase(path, ".n64") ||
	    StringEndsWithNoCase(path, ".v64"))
	{
		return &g_DefaultCore;
	}
	return 0;
#elif defined(RA_BAREMETAL_FCEUMM)
	if (StringEndsWithNoCase(path, ".nes"))
	{
		return &g_DefaultCore;
	}
	return 0;
#else
	(void)path;
	return &g_DefaultCore;
#endif
#endif
}

static bool GetN64Variable(struct retro_variable *var)
{
	if (!var || !var->key)
	{
		return false;
	}

	if (StringEndsWith(var->key, "-rdp-plugin"))
	{
		var->value = "angrylion";
		return true;
	}
	if (StringEndsWith(var->key, "-rsp-plugin"))
	{
		var->value = "cxd4";
		return true;
	}
	if (StringEndsWith(var->key, "-cpucore"))
	{
		var->value = "dynamic_recompiler";
		return true;
	}
	if (StringEndsWith(var->key, "-ThreadedRenderer"))
	{
		var->value = "False";
		return true;
	}
	if (StringEndsWith(var->key, "-angrylion-vioverlay"))
	{
		var->value = "Unfiltered";
		return true;
	}
	if (StringEndsWith(var->key, "-angrylion-sync"))
	{
		var->value = "Low";
		return true;
	}
	if (StringEndsWith(var->key, "-angrylion-multithread"))
	{
		var->value = "4";
		return true;
	}
	if (StringEndsWith(var->key, "-angrylion-overscan"))
	{
		var->value = "disabled";
		return true;
	}
	if (StringEndsWith(var->key, "-43screensize"))
	{
		var->value = "320x240";
		return true;
	}
	if (StringEndsWith(var->key, "-FrameDuping"))
	{
		var->value = "False";
		return true;
	}
	if (StringEndsWith(var->key, "-Framerate"))
	{
		var->value = "Fullspeed";
		return true;
	}
	if (StringEndsWith(var->key, "-virefresh"))
	{
		var->value = "Auto";
		return true;
	}
	if (StringEndsWith(var->key, "-CountPerOp"))
	{
		var->value = "1";
		return true;
	}
	if (StringEndsWith(var->key, "-CountPerOpDenomPot"))
	{
		var->value = "0";
		return true;
	}
	if (StringEndsWith(var->key, "-pak1"))
	{
		var->value = "memory";
		return true;
	}
	if (StringEndsWith(var->key, "-astick-deadzone"))
	{
		var->value = "12";
		return true;
	}
	if (StringEndsWith(var->key, "-astick-sensitivity"))
	{
		var->value = "100";
		return true;
	}

	return false;
}

LibretroRunner::LibretroRunner(CircleLog *pLog,
                               CircleTimer *pTimer,
                               CircleVideo *pVideo,
                               CircleAudio *pAudio,
                               CircleInput *pInput,
                               CircleFs *pFs)
:	m_pLog(pLog),
	m_pTimer(pTimer),
	m_pVideo(pVideo),
	m_pAudio(pAudio),
	m_pInput(pInput),
	m_pFs(pFs),
	m_pCore(0),
	m_pProgressContext(0),
	m_pProgressCallback(0),
	m_PixelFormat(RETRO_PIXEL_FORMAT_RGB565),
	m_SupportsNoGame(false),
	m_pRomData(0),
	m_RomSize(0)
{
	memset(&m_AvInfo, 0, sizeof(m_AvInfo));
	memset(&m_GameInfoExt, 0, sizeof(m_GameInfoExt));
	memset(m_GameFullPath, 0, sizeof(m_GameFullPath));
	memset(m_GameDir, 0, sizeof(m_GameDir));
	memset(m_GameName, 0, sizeof(m_GameName));
	memset(m_GameExt, 0, sizeof(m_GameExt));
}

LibretroRunner::~LibretroRunner(void)
{
	if (m_pCore)
	{
		m_pCore->unload_game();
		m_pCore->deinit();
	}

	delete[] m_pRomData;
	m_pRomData = 0;
}

bool LibretroRunner::Init(const char *romPath)
{
	return Init(LibretroDefaultCore(), romPath);
}

void LibretroRunner::SetProgressCallback(void *pContext, TProgressCallback pCallback)
{
	m_pProgressContext = pContext;
	m_pProgressCallback = pCallback;
}

void LibretroRunner::Progress(const char *pMessage)
{
	if (m_pProgressCallback && pMessage)
	{
		m_pProgressCallback(m_pProgressContext, pMessage);
	}
	if (m_pLog && pMessage)
	{
		m_pLog->Notice(pMessage);
	}
}

void LibretroRunner::ProgressValue(const char *pLabel, unsigned nValue)
{
	char message[96];
	snprintf(message, sizeof(message), "%s%u", pLabel ? pLabel : "", nValue);
	Progress(message);
}

bool LibretroRunner::Init(const LibretroCore *pCore, const char *romPath)
{
	Progress("runner: begin init");
	s_pActive = this;
	m_pCore = pCore ? pCore : LibretroDefaultCore();
	if (!m_pCore)
	{
		return false;
	}

	Progress(m_pCore->n64Options ? "runner: core N64" : "runner: core NES");

	if (m_pCore->api_version() != RETRO_API_VERSION && m_pLog)
	{
		m_pLog->Warn("Core reports an unexpected libretro API version");
	}

	Progress("runner: set callbacks");
	m_pCore->set_environment(EnvironmentThunk);
	m_pCore->set_video_refresh(VideoRefreshThunk);
	m_pCore->set_audio_sample(AudioSampleThunk);
	m_pCore->set_audio_sample_batch(AudioSampleBatchThunk);
	m_pCore->set_input_poll(InputPollThunk);
	m_pCore->set_input_state(InputStateThunk);

	Progress("runner: retro_init begin");
	m_pCore->init();
	Progress("runner: retro_init done");

	struct retro_system_info info;
	memset(&info, 0, sizeof(info));
	Progress("runner: system_info begin");
	m_pCore->get_system_info(&info);
	Progress("runner: system_info done");

	struct retro_game_info game;
	memset(&game, 0, sizeof(game));

	bool haveGame = false;
	char loadedPath[256];
	memset(loadedPath, 0, sizeof(loadedPath));
	if (romPath && romPath[0] && m_pFs)
	{
		Progress("runner: read ROM begin");
		haveGame = m_pFs->ReadWholeFile(romPath, &m_pRomData, &m_RomSize, m_pCore->maxRomSize);
		if (haveGame)
		{
			strncpy(loadedPath, romPath, sizeof(loadedPath) - 1);
			ProgressValue("runner: ROM bytes ", (unsigned)m_RomSize);
		}
		else
		{
			Progress("runner: selected ROM read failed");
			const char *extensionsN64[] = { "z64", "n64", "v64", 0 };
			const char *extensionsNES[] = { "nes", 0 };
			const char **extensions = m_pCore->n64Options ? extensionsN64 : extensionsNES;
			char foundPath[256];
			for (unsigned i = 0; extensions[i] && !haveGame; i++)
			{
				if (m_pFs->FindFirstWithExtension(extensions[i], foundPath, sizeof(foundPath)))
				{
					if (m_pLog)
					{
						m_pLog->Notice(m_pCore->n64Options
							? "Falling back to first N64 file in SD root"
							: "Falling back to first NES file in SD root");
					}
					haveGame = m_pFs->ReadWholeFile(foundPath, &m_pRomData, &m_RomSize, m_pCore->maxRomSize);
					if (haveGame)
					{
						strncpy(loadedPath, foundPath, sizeof(loadedPath) - 1);
						ProgressValue("runner: fallback ROM bytes ", (unsigned)m_RomSize);
					}
				}
			}
		}

		if (haveGame)
		{
			Progress("runner: prepare game info");
			PrepareGameInfoExt(loadedPath);
			game.path = loadedPath;
			game.data = m_pRomData;
			game.size = m_RomSize;
			game.meta = 0;
		}
	}

	if (!haveGame && !m_SupportsNoGame)
	{
		if (m_pLog)
		{
			m_pLog->Error("No ROM loaded and core does not support no-game mode");
		}
		return false;
	}

	Progress("runner: retro_load_game begin");
	if (!m_pCore->load_game(haveGame ? &game : 0))
	{
		if (m_pLog)
		{
			m_pLog->Error("retro_load_game failed");
		}
		Progress("runner: retro_load_game failed");
		return false;
	}
	Progress("runner: retro_load_game done");

	Progress("runner: av_info begin");
	m_pCore->get_system_av_info(&m_AvInfo);
	Progress("runner: av_info done");

	if (m_AvInfo.timing.fps < 1.0)
	{
		m_AvInfo.timing.fps = 60.0;
	}

	if (m_pVideo)
	{
		m_pVideo->SetDisplayAspectRatio((float)m_AvInfo.geometry.aspect_ratio);
	}

	Progress("runner: init done");
	return true;
}

void LibretroRunner::PrepareGameInfoExt(const char *romPath)
{
	const char *path = romPath ? romPath : "GAME.NES";
	const char *base = path;
	const char *dot = 0;

	for (const char *p = path; *p; p++)
	{
		if (*p == '/' || *p == '\\')
		{
			base = p + 1;
		}
	}

	for (const char *p = base; *p; p++)
	{
		if (*p == '.')
		{
			dot = p;
		}
	}

	strncpy(m_GameFullPath, path, sizeof(m_GameFullPath) - 1);
	strncpy(m_GameDir, "/", sizeof(m_GameDir) - 1);

	if (dot && dot > base)
	{
		const size_t nameLen = (size_t)(dot - base) < sizeof(m_GameName) - 1
			? (size_t)(dot - base)
			: sizeof(m_GameName) - 1;
		memcpy(m_GameName, base, nameLen);
		m_GameName[nameLen] = 0;

		size_t extLen = 0;
		for (const char *p = dot + 1; *p && extLen < sizeof(m_GameExt) - 1; p++)
		{
			char c = *p;
			if (c >= 'A' && c <= 'Z')
			{
				c = (char)(c - 'A' + 'a');
			}
			m_GameExt[extLen++] = c;
		}
		m_GameExt[extLen] = 0;
	}
	else
	{
		strncpy(m_GameName, base, sizeof(m_GameName) - 1);
		m_GameExt[0] = 0;
	}

	m_GameInfoExt.full_path = m_GameFullPath;
	m_GameInfoExt.archive_path = 0;
	m_GameInfoExt.archive_file = 0;
	m_GameInfoExt.dir = m_GameDir;
	m_GameInfoExt.name = m_GameName;
	m_GameInfoExt.ext = m_GameExt;
	m_GameInfoExt.meta = 0;
	m_GameInfoExt.data = m_pRomData;
	m_GameInfoExt.size = m_RomSize;
	m_GameInfoExt.file_in_archive = false;
	m_GameInfoExt.persistent_data = true;
}

void LibretroRunner::RunFrame(void)
{
	if (m_pCore)
	{
		m_pCore->run();
	}
}

double LibretroRunner::FramesPerSecond(void) const
{
	return m_AvInfo.timing.fps > 1.0 ? m_AvInfo.timing.fps : 60.0;
}

unsigned LibretroRunner::SampleRate(void) const
{
	return m_AvInfo.timing.sample_rate > 1000.0
		? (unsigned)(m_AvInfo.timing.sample_rate + 0.5)
		: 48000;
}

bool LibretroRunner::Environment(unsigned cmd, void *data)
{
	switch (cmd)
	{
	case RA_ENVIRONMENT_BAREMETAL_PROGRESS:
		if (data)
		{
			Progress((const char *)data);
			return true;
		}
		return false;

	case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
		if (!data || !m_pVideo)
		{
			return false;
		}

		m_PixelFormat = *(const enum retro_pixel_format *)data;
		return m_pVideo->SetPixelFormat(m_PixelFormat);

	case RETRO_ENVIRONMENT_GET_CAN_DUPE:
		if (data)
		{
			*(bool *)data = true;
			return true;
		}
		return false;

	case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
		if (data)
		{
			m_SupportsNoGame = *(const bool *)data;
			return true;
		}
		return false;

	case RETRO_ENVIRONMENT_GET_GAME_INFO_EXT:
		if (data && m_pRomData && m_RomSize)
		{
			*(const struct retro_game_info_ext **)data = &m_GameInfoExt;
			return true;
		}
		return false;

	case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
		if (data)
		{
			((struct retro_log_callback *)data)->log = LogPrintfThunk;
			return true;
		}
		return false;

	case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
		if (data)
		{
			*(unsigned *)data = 2;
			return true;
		}
		return false;

	case RETRO_ENVIRONMENT_SET_CORE_OPTIONS:
	case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL:
	case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2:
	case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL:
	case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY:
	case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_UPDATE_DISPLAY_CALLBACK:
	case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
	case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
	case RETRO_ENVIRONMENT_SET_CONTENT_INFO_OVERRIDE:
	case RETRO_ENVIRONMENT_SET_MEMORY_MAPS:
	case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
	case RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS:
	case RETRO_ENVIRONMENT_SET_MESSAGE:
	case RETRO_ENVIRONMENT_SET_MESSAGE_EXT:
	case RETRO_ENVIRONMENT_SET_GEOMETRY:
	case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
		return true;

	case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
		if (data)
		{
			*(bool *)data = false;
			return true;
		}
		return false;

	case RETRO_ENVIRONMENT_GET_MESSAGE_INTERFACE_VERSION:
		if (data)
		{
			*(unsigned *)data = 1;
			return true;
		}
		return false;

	case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
		return true;

	case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
	case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
	case RETRO_ENVIRONMENT_GET_CONTENT_DIRECTORY:
		if (data)
		{
			*(const char **)data = "/";
			return true;
		}
		return false;

	case RETRO_ENVIRONMENT_GET_VARIABLE:
		return m_pCore && m_pCore->n64Options
			? GetN64Variable((struct retro_variable *)data)
			: false;

	case RETRO_ENVIRONMENT_SET_VARIABLES:
		return true;

	default:
		return false;
	}
}

void LibretroRunner::VideoRefresh(const void *data, unsigned width, unsigned height, size_t pitch)
{
	if (m_pVideo)
	{
		m_pVideo->SubmitFrame(data, width, height, pitch);
	}
}

void LibretroRunner::AudioSample(int16_t left, int16_t right)
{
	if (m_pAudio)
	{
		m_pAudio->WriteSample(left, right);
	}
}

size_t LibretroRunner::AudioSampleBatch(const int16_t *data, size_t frames)
{
	if (!m_pAudio)
	{
		return frames;
	}

	return m_pAudio->WriteFrames(data, frames);
}

void LibretroRunner::InputPoll(void)
{
#ifndef RA_BAREMETAL_NO_USB
	if (m_pInput)
	{
		m_pInput->Poll();
	}
#endif
}

int16_t LibretroRunner::InputState(unsigned port, unsigned device, unsigned index, unsigned id)
{
#ifdef RA_BAREMETAL_NO_USB
	(void)port;
	(void)device;
	(void)index;
	(void)id;
	return 0;
#else
	if (!m_pInput)
	{
		return 0;
	}

	return m_pInput->State(port, device, index, id);
#endif
}

bool LibretroRunner::EnvironmentThunk(unsigned cmd, void *data)
{
	return s_pActive ? s_pActive->Environment(cmd, data) : false;
}

void LibretroRunner::LogPrintfThunk(enum retro_log_level level, const char *format, ...)
{
	if (!s_pActive || !s_pActive->m_pLog || !format)
	{
		return;
	}

	char message[512];
	va_list args;
	va_start(args, format);
	vsnprintf(message, sizeof(message), format, args);
	va_end(args);
	message[sizeof(message) - 1] = 0;
	for (char *p = message; *p; p++)
	{
		if (*p == '\r' || *p == '\n')
		{
			*p = 0;
			break;
		}
	}

	const bool n64Log = s_pActive->m_pCore && s_pActive->m_pCore->n64Options;
	const bool noteworthyN64Message = strstr(message, "M64CMD") || strstr(message, "failed");
	if (n64Log && level < RETRO_LOG_WARN && !noteworthyN64Message)
	{
		return;
	}

	switch (level)
	{
	case RETRO_LOG_ERROR:
		s_pActive->m_pLog->Error(message);
		break;
	case RETRO_LOG_WARN:
		s_pActive->m_pLog->Warn(message);
		break;
	default:
		s_pActive->m_pLog->Notice(message);
		break;
	}

	if (n64Log && noteworthyN64Message)
	{
		s_pActive->Progress(message);
	}
}

void LibretroRunner::VideoRefreshThunk(const void *data, unsigned width, unsigned height, size_t pitch)
{
	if (s_pActive)
	{
		s_pActive->VideoRefresh(data, width, height, pitch);
	}
}

void LibretroRunner::AudioSampleThunk(int16_t left, int16_t right)
{
	if (s_pActive)
	{
		s_pActive->AudioSample(left, right);
	}
}

size_t LibretroRunner::AudioSampleBatchThunk(const int16_t *data, size_t frames)
{
	return s_pActive ? s_pActive->AudioSampleBatch(data, frames) : frames;
}

void LibretroRunner::InputPollThunk(void)
{
	if (s_pActive)
	{
		s_pActive->InputPoll();
	}
}

int16_t LibretroRunner::InputStateThunk(unsigned port, unsigned device, unsigned index, unsigned id)
{
	return s_pActive ? s_pActive->InputState(port, device, index, id) : 0;
}
