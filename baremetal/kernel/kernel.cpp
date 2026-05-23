#include "kernel.h"
#include "rom_browser.h"

#include <circle/string.h>
#include <string.h>

#ifndef RA_BAREMETAL_ROM_PATH
#define RA_BAREMETAL_ROM_PATH "GAME.GB"
#endif

static const char FromKernel[] = "ra-circle";
static const unsigned HDMI_AUDIO_CHUNK_SIZE = 384 * 4;

static void ScreenWriteLine(CScreenDevice *pScreen, const char *pText)
{
	if (!pScreen || !pText)
	{
		return;
	}

	pScreen->Write(pText, strlen(pText));
	pScreen->Write("\n", 1);
	pScreen->Update();
}

static void RunnerProgress(void *pContext, const char *pMessage)
{
	ScreenWriteLine((CScreenDevice *)pContext, pMessage);
}

#ifdef RA_BAREMETAL_NO_USB
static void CopyString(char *pDst, size_t nDstSize, const char *pSrc)
{
	if (!pDst || nDstSize == 0)
	{
		return;
	}

	size_t i = 0;
	if (pSrc)
	{
		for (; pSrc[i] && i + 1 < nDstSize; i++)
		{
			pDst[i] = pSrc[i];
		}
	}
	pDst[i] = 0;
}
#endif

static boolean TryMountFatDevice(CDeviceNameService *pDeviceNameService, CLogger *pLogger,
	CFATFileSystem **ppFileSystem, const char *pDeviceName)
{
	if (!pDeviceNameService || !ppFileSystem || !pDeviceName)
	{
		return FALSE;
	}

	CDevice *pDevice = pDeviceNameService->GetDevice(pDeviceName, TRUE);
	if (pDevice == 0)
	{
		if (pLogger)
		{
			pLogger->Write(FromKernel, LogNotice, "FAT mount candidate missing: %s", pDeviceName);
		}
		return FALSE;
	}

	CFATFileSystem *pFileSystem = new CFATFileSystem;
	if (!pFileSystem)
	{
		if (pLogger)
		{
			pLogger->Write(FromKernel, LogError, "FAT filesystem allocation failed");
		}
		return FALSE;
	}

	if (!pFileSystem->Mount(pDevice))
	{
		if (pLogger)
		{
			pLogger->Write(FromKernel, LogWarning, "FAT mount failed on %s", pDeviceName);
		}
		delete pFileSystem;
		return FALSE;
	}

	*ppFileSystem = pFileSystem;
	if (pLogger)
	{
		pLogger->Write(FromKernel, LogNotice, "FAT mounted on %s", pDeviceName);
	}
	return TRUE;
}

static void DrawBootMarker(CScreenDevice *pScreen)
{
	if (!pScreen)
	{
		return;
	}

	const unsigned width = pScreen->GetWidth();
	const unsigned height = pScreen->GetHeight();
	if (width == 0 || height == 0)
	{
		return;
	}

	const TScreenColor colors[] = {
		BRIGHT_RED_COLOR,
		BRIGHT_GREEN_COLOR,
		BRIGHT_BLUE_COLOR,
		BRIGHT_WHITE_COLOR,
	};
	const unsigned markerHeight = height < 96 ? height : 96;
	const unsigned bandWidth = width / 4 ? width / 4 : 1;

	for (unsigned y = 0; y < markerHeight; y++)
	{
		for (unsigned x = 0; x < width; x++)
		{
			unsigned colorIndex = x / bandWidth;
			if (colorIndex >= 4)
			{
				colorIndex = 3;
			}
			pScreen->SetPixel(x, y, colors[colorIndex]);
		}
	}

	pScreen->Update();
}

CKernel::CKernel(void)
:	m_CPUThrottle(CPUSpeedMaximum),
	m_Screen(m_Options.GetWidth(), m_Options.GetHeight()),
	m_Timer(&m_Interrupt),
	m_Logger(m_Options.GetLogLevel(), &m_Timer),
#ifndef RA_BAREMETAL_NO_USB
	m_pUSBHCI(0),
#endif
	m_pEMMC(0),
	m_pFileSystem(0),
	m_pSound(0),
#ifndef RA_BAREMETAL_NO_USB
	m_Runner(&m_Log, &m_FrameTimer, &m_Video, &m_Audio, &m_Input, &m_Fs)
#else
	m_Runner(&m_Log, &m_FrameTimer, &m_Video, &m_Audio, 0, &m_Fs)
#endif
{
	m_ActLED.Blink(5);
}

CKernel::~CKernel(void)
{
	if (m_pSound)
	{
		m_pSound->Cancel();
		delete m_pSound;
		m_pSound = 0;
	}

	delete m_pFileSystem;
	m_pFileSystem = 0;

	delete m_pEMMC;
	m_pEMMC = 0;

#ifndef RA_BAREMETAL_NO_USB
	delete m_pUSBHCI;
	m_pUSBHCI = 0;
#endif
}

boolean CKernel::Initialize(void)
{
	boolean bOK = TRUE;

	if (bOK)
	{
		bOK = m_Screen.Initialize();
		if (bOK)
		{
			DrawBootMarker(&m_Screen);
			ScreenWriteLine(&m_Screen, "RetroArch bare-metal: screen ok");
		}
	}

	if (bOK)
	{
		ScreenWriteLine(&m_Screen, "serial init...");
		bOK = m_Serial.Initialize(115200);
		ScreenWriteLine(&m_Screen, bOK ? "serial ok" : "serial failed");
	}

	if (bOK)
	{
		CDevice *pTarget = m_DeviceNameService.GetDevice(m_Options.GetLogDevice(), FALSE);
		if (pTarget == 0)
		{
			pTarget = &m_Screen;
		}

		ScreenWriteLine(&m_Screen, "logger init...");
		bOK = m_Logger.Initialize(pTarget);
		ScreenWriteLine(&m_Screen, bOK ? "logger ok" : "logger failed");
	}

	if (bOK)
	{
		ScreenWriteLine(&m_Screen, "interrupt init...");
		bOK = m_Interrupt.Initialize();
		ScreenWriteLine(&m_Screen, bOK ? "interrupt ok" : "interrupt failed");
	}

	if (bOK)
	{
		ScreenWriteLine(&m_Screen, "timer init...");
		bOK = m_Timer.Initialize();
		ScreenWriteLine(&m_Screen, bOK ? "timer ok" : "timer failed");
	}

	if (bOK)
	{
		ScreenWriteLine(&m_Screen, "early init ok");
		m_CPUThrottle.DumpStatus(FALSE);
	}

	if (bOK)
	{
		ScreenWriteLine(&m_Screen, "multicore init...");
		if (m_Parallel.Initialize())
		{
			ScreenWriteLine(&m_Screen, "multicore ok");
		}
		else
		{
			ScreenWriteLine(&m_Screen, "multicore failed; n64 single-core RDP");
		}
	}

	return bOK;
}

TShutdownMode CKernel::Run(void)
{
	m_Log.Init(&m_Logger);
	m_FrameTimer.Init(&m_Timer);
	m_Video.Init(&m_Screen);

	m_Logger.Write(FromKernel, LogNotice, "Compile time: " __DATE__ " " __TIME__);

#ifndef RA_BAREMETAL_NO_USB
	ScreenWriteLine(&m_Screen, "alloc usb...");
	m_pUSBHCI = new CUSBHCIDevice(&m_Interrupt, &m_Timer, TRUE);
	if (!m_pUSBHCI)
	{
		ScreenWriteLine(&m_Screen, "usb allocation failed; continuing without input");
	}
	else
	{
		ScreenWriteLine(&m_Screen, "usb init...");
		if (m_pUSBHCI->Initialize())
		{
			ScreenWriteLine(&m_Screen, "usb ok");
			m_Input.Init(&m_DeviceNameService, m_pUSBHCI, &m_Log);
		}
		else
		{
			ScreenWriteLine(&m_Screen, "usb failed; continuing without input");
			delete m_pUSBHCI;
			m_pUSBHCI = 0;
		}
	}
#else
	ScreenWriteLine(&m_Screen, "usb disabled for diagnosis");
#endif

	ScreenWriteLine(&m_Screen, "alloc emmc...");
	m_pEMMC = new CEMMCDevice(&m_Interrupt, &m_Timer, &m_ActLED);
	if (!m_pEMMC)
	{
		ScreenWriteLine(&m_Screen, "emmc allocation failed");
		return ShutdownHalt;
	}

	ScreenWriteLine(&m_Screen, "emmc init...");
	if (!m_pEMMC->Initialize())
	{
		ScreenWriteLine(&m_Screen, "emmc failed");
		return ShutdownHalt;
	}
	ScreenWriteLine(&m_Screen, "emmc ok");

	m_Logger.Write(FromKernel, LogNotice, "Searching for FAT partition on SD");
	ScreenWriteLine(&m_Screen, "mount sd fat...");

	static const char *MountCandidates[] = {
		"emmc1-1", "emmc1-2", "emmc1-3", "emmc1-4",
		"emmc1",
		"emmc2-1", "emmc2-2", "emmc2-3", "emmc2-4",
		"emmc2",
		0
	};

	for (unsigned i = 0; MountCandidates[i] != 0; i++)
	{
		m_Logger.Write(FromKernel, LogNotice, "Trying FAT mount: %s", MountCandidates[i]);
		if (TryMountFatDevice(&m_DeviceNameService, &m_Logger, &m_pFileSystem, MountCandidates[i]))
		{
			CString Message;
			Message.Format("mount ok: %s", MountCandidates[i]);
			ScreenWriteLine(&m_Screen, Message);
			break;
		}
	}

	if (!m_pFileSystem)
	{
		ScreenWriteLine(&m_Screen, "no FAT16/FAT32 partition");
		ScreenWriteLine(&m_Screen, "format SD boot partition as MBR FAT32");
		m_Logger.Write(FromKernel, LogPanic, "No mountable FAT16/FAT32 SD partition found");
		return ShutdownHalt;
	}

	m_Fs.Init(m_pFileSystem, &m_Log);

	char selectedRom[256];
	const LibretroCore *pSelectedCore = 0;
#ifndef RA_BAREMETAL_NO_USB
	ScreenWriteLine(&m_Screen, "select ROM...");
	if (!SelectRomAtBoot(&m_Screen, &m_Timer, &m_Fs, &m_Input,
		selectedRom, sizeof(selectedRom), &pSelectedCore))
	{
		ScreenWriteLine(&m_Screen, "ROM selection failed");
		m_Logger.Write(FromKernel, LogPanic, "ROM selection failed");
		return ShutdownHalt;
	}
#else
	CopyString(selectedRom, sizeof(selectedRom), RA_BAREMETAL_ROM_PATH);
	pSelectedCore = LibretroFindCoreForPath(selectedRom);
	if (!pSelectedCore)
	{
		pSelectedCore = LibretroDefaultCore();
	}
#endif

	ScreenWriteLine(&m_Screen, "loading selected ROM");
	m_Timer.MsDelay(1000);
	m_Runner.SetProgressCallback(&m_Screen, RunnerProgress);
	if (!m_Runner.Init(pSelectedCore, selectedRom))
	{
		ScreenWriteLine(&m_Screen, "libretro runner init failed");
		m_Logger.Write(FromKernel, LogPanic, "libretro runner init failed");
		return ShutdownHalt;
	}

	const unsigned audioSampleRate = m_Runner.SampleRate();
	m_Logger.Write(FromKernel, LogNotice, "Initializing HDMI audio at %u Hz", audioSampleRate);
	ScreenWriteLine(&m_Screen, "audio hdmi init...");
	m_pSound = new CHDMISoundBaseDevice(&m_Interrupt, audioSampleRate, HDMI_AUDIO_CHUNK_SIZE);
	if (!m_pSound)
	{
		ScreenWriteLine(&m_Screen, "audio allocation failed; muted");
		m_Logger.Write(FromKernel, LogError, "HDMI audio allocation failed");
	}
	else if (!m_Audio.Init(m_pSound, audioSampleRate))
	{
		ScreenWriteLine(&m_Screen, "audio hdmi failed; muted");
		m_Logger.Write(FromKernel, LogError, "HDMI audio initialization failed");
		delete m_pSound;
		m_pSound = 0;
	}
	else
	{
		CString Message;
		Message.Format("audio ok: %u Hz", audioSampleRate);
		ScreenWriteLine(&m_Screen, Message);
	}

	ScreenWriteLine(&m_Screen, "libretro ok; entering frame loop");
	m_Logger.Write(FromKernel, LogNotice, "Entering libretro frame loop");
	m_Timer.MsDelay(1000);

	const bool uncapFrontendPacing = pSelectedCore && pSelectedCore->n64Options;
	if (uncapFrontendPacing)
	{
		m_Logger.Write(FromKernel, LogNotice, "N64 frontend frame pacing disabled");
	}

	while (1)
	{
#ifndef RA_BAREMETAL_NO_USB
		m_Input.Poll();
#endif
		m_Runner.RunFrame();
		if (!uncapFrontendPacing)
		{
			m_FrameTimer.WaitNextFrame(m_Runner.FramesPerSecond());
		}
		m_CPUThrottle.Update();
	}

	return ShutdownHalt;
}
