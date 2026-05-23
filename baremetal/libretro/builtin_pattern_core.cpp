#include <libretro.h>

#include <stdint.h>
#include <string.h>

static retro_environment_t environ_cb;
static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;

static uint16_t frame[320 * 240];
static unsigned frame_count;
static int cursor_x = 160;
static int cursor_y = 120;

static uint16_t rgb565(unsigned r, unsigned g, unsigned b)
{
	return (uint16_t)(((r & 31) << 11) | ((g & 63) << 5) | (b & 31));
}

extern "C" void retro_set_environment(retro_environment_t cb)
{
	environ_cb = cb;

	enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
	bool supports_no_game = true;

	environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt);
	environ_cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &supports_no_game);
}

extern "C" void retro_set_video_refresh(retro_video_refresh_t cb)
{
	video_cb = cb;
}

extern "C" void retro_set_audio_sample(retro_audio_sample_t cb)
{
	audio_cb = cb;
}

extern "C" void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
	audio_batch_cb = cb;
}

extern "C" void retro_set_input_poll(retro_input_poll_t cb)
{
	input_poll_cb = cb;
}

extern "C" void retro_set_input_state(retro_input_state_t cb)
{
	input_state_cb = cb;
}

extern "C" void retro_init(void)
{
	frame_count = 0;
}

extern "C" void retro_deinit(void)
{
}

extern "C" unsigned retro_api_version(void)
{
	return RETRO_API_VERSION;
}

extern "C" void retro_get_system_info(struct retro_system_info *info)
{
	memset(info, 0, sizeof(*info));
	info->library_name = "circle-pattern";
	info->library_version = "0.1";
	info->need_fullpath = false;
	info->valid_extensions = "";
	info->block_extract = false;
}

extern "C" void retro_get_system_av_info(struct retro_system_av_info *info)
{
	memset(info, 0, sizeof(*info));
	info->geometry.base_width = 320;
	info->geometry.base_height = 240;
	info->geometry.max_width = 320;
	info->geometry.max_height = 240;
	info->geometry.aspect_ratio = 4.0f / 3.0f;
	info->timing.fps = 60.0;
	info->timing.sample_rate = 48000.0;
}

extern "C" void retro_set_controller_port_device(unsigned port, unsigned device)
{
	(void)port;
	(void)device;
}

extern "C" void retro_reset(void)
{
	cursor_x = 160;
	cursor_y = 120;
}

extern "C" bool retro_load_game(const struct retro_game_info *game)
{
	(void)game;
	return true;
}

extern "C" bool retro_load_game_special(unsigned game_type,
                                         const struct retro_game_info *info,
                                         size_t num_info)
{
	(void)game_type;
	(void)info;
	(void)num_info;
	return false;
}

extern "C" void retro_unload_game(void)
{
}

extern "C" unsigned retro_get_region(void)
{
	return RETRO_REGION_NTSC;
}

extern "C" void retro_run(void)
{
	if (input_poll_cb)
	{
		input_poll_cb();

		if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT))  cursor_x--;
		if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT)) cursor_x++;
		if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP))    cursor_y--;
		if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN))  cursor_y++;
	}

	if (cursor_x < 0) cursor_x = 0;
	if (cursor_y < 0) cursor_y = 0;
	if (cursor_x > 319) cursor_x = 319;
	if (cursor_y > 239) cursor_y = 239;

	for (unsigned y = 0; y < 240; y++)
	{
		for (unsigned x = 0; x < 320; x++)
		{
			const unsigned r = (x + frame_count) & 31;
			const unsigned g = ((y / 2) + frame_count) & 63;
			const unsigned b = ((x ^ y ^ frame_count) >> 1) & 31;
			frame[y * 320 + x] = rgb565(r, g, b);
		}
	}

	for (int dy = -4; dy <= 4; dy++)
	{
		for (int dx = -4; dx <= 4; dx++)
		{
			const int x = cursor_x + dx;
			const int y = cursor_y + dy;
			if (x >= 0 && x < 320 && y >= 0 && y < 240)
			{
				frame[y * 320 + x] = rgb565(31, 63, 31);
			}
		}
	}

	if (video_cb)
	{
		video_cb(frame, 320, 240, 320 * sizeof(uint16_t));
	}

	if (audio_batch_cb)
	{
		int16_t silence[256 * 2];
		memset(silence, 0, sizeof(silence));
		audio_batch_cb(silence, 256);
	}
	else if (audio_cb)
	{
		audio_cb(0, 0);
	}

	frame_count++;
}

extern "C" size_t retro_serialize_size(void)
{
	return 0;
}

extern "C" bool retro_serialize(void *data, size_t size)
{
	(void)data;
	(void)size;
	return false;
}

extern "C" bool retro_unserialize(const void *data, size_t size)
{
	(void)data;
	(void)size;
	return false;
}

extern "C" void retro_cheat_reset(void)
{
}

extern "C" void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
	(void)index;
	(void)enabled;
	(void)code;
}

extern "C" void *retro_get_memory_data(unsigned id)
{
	(void)id;
	return 0;
}

extern "C" size_t retro_get_memory_size(unsigned id)
{
	(void)id;
	return 0;
}
