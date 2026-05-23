#ifndef RA_BAREMETAL_CIRCLE_PLATFORM_H
#define RA_BAREMETAL_CIRCLE_PLATFORM_H

#include <stddef.h>
#include <stdint.h>

#include <circle/devicenameservice.h>
#include <circle/fs/fat/fatfs.h>
#include <circle/logger.h>
#include <circle/screen.h>
#include <circle/sound/soundbasedevice.h>
#include <circle/timer.h>
#include <circle/types.h>
#include <circle/usb/usbgamepad.h>
#include <circle/usb/usbhcidevice.h>

#include <libretro.h>

class CircleLog
{
public:
	CircleLog(void);
	void Init(CLogger *pLogger);
	void Notice(const char *pMessage);
	void Warn(const char *pMessage);
	void Error(const char *pMessage);

private:
	CLogger *m_pLogger;
};

class CircleTimer
{
public:
	CircleTimer(void);
	void Init(CTimer *pTimer);
	uint64_t NowUsec(void) const;
	void WaitNextFrame(double fps);

private:
	CTimer *m_pTimer;
	uint64_t m_NextFrameUsec;
};

class CircleVideo
{
public:
	CircleVideo(void);
	~CircleVideo(void);
	void Init(CScreenDevice *pScreen);
	bool SetPixelFormat(enum retro_pixel_format format);
	void SetDisplayAspectRatio(float aspectRatio);
	bool SubmitFrame(const void *frame, unsigned width, unsigned height, size_t pitch);

private:
	TScreenColor ConvertPixel(const void *pPixel) const;

private:
	CScreenDevice *m_pScreen;
	enum retro_pixel_format m_Format;
	float m_DisplayAspectRatio;
	unsigned m_LastScreenW;
	unsigned m_LastScreenH;
	unsigned m_LastOutW;
	unsigned m_LastOutH;
	unsigned m_LastOriginX;
	unsigned m_LastOriginY;
	unsigned m_XMapCount;
	unsigned m_XMapSourceWidth;
	uint16_t m_XMap[2048];
	uint16_t *m_pScaleBuffer;
	size_t m_ScaleBufferPixels;
};

class CircleAudio
{
public:
	CircleAudio(void);
	bool Init(CSoundBaseDevice *pSound, unsigned sampleRate);
	void WriteSample(int16_t left, int16_t right);
	size_t WriteFrames(const int16_t *samples, size_t frames);

private:
	CSoundBaseDevice *m_pSound;
};

class CircleInput
{
public:
	CircleInput(void);
	void Init(CDeviceNameService *pNameService, CUSBHCIDevice *pUSBHCI);
	void Init(CDeviceNameService *pNameService, CUSBHCIDevice *pUSBHCI, CircleLog *pLog);
	void Poll(void);
	int16_t State(unsigned port, unsigned device, unsigned index, unsigned id) const;

private:
	void AttachFirstGamePad(void);
	bool DigitalButton(unsigned mask) const;
	bool GenericButton(unsigned index) const;
	bool HatDirection(unsigned id) const;
	bool AxisDirection(unsigned id) const;
	bool ButtonState(unsigned id) const;
	unsigned JoypadMask(void) const;
	static void GamePadStatusHandler(unsigned nDeviceIndex, const TGamePadState *pState);
	static void GamePadRemovedHandler(CDevice *pDevice, void *pContext);

private:
	CDeviceNameService *m_pNameService;
	CUSBHCIDevice *m_pUSBHCI;
	CircleLog *m_pLog;
	CUSBGamePadDevice *m_pGamePad;
	bool m_LoggedNoGamePad;
	bool m_GamePadKnown;
	volatile TGamePadState m_State;

	static CircleInput *s_pThis;
};

class CircleFs
{
public:
	struct Entry
	{
		char name[FS_TITLE_LEN+1];
		unsigned size;
		bool isDirectory;
	};

	CircleFs(void);
	void Init(CFATFileSystem *pFileSystem, CircleLog *pLog);
	bool ReadWholeFile(const char *path, uint8_t **ppData, size_t *pSize, size_t maxSize);
	bool ListDirectory(const char *path, Entry *entries, unsigned maxEntries, unsigned *pCount);
	bool FindFirstWithExtension(const char *extension, char *path, size_t pathSize);
	bool WriteWholeFile(const char *path, const void *pData, size_t size);

private:
	CFATFileSystem *m_pFileSystem;
	CircleLog *m_pLog;
};

#endif
