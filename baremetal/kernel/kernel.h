#ifndef RA_BAREMETAL_KERNEL_H
#define RA_BAREMETAL_KERNEL_H

#include <circle/actled.h>
#include <circle/cputhrottle.h>
#include <circle/devicenameservice.h>
#include <circle/exceptionhandler.h>
#include <circle/interrupt.h>
#include <circle/koptions.h>
#include <circle/logger.h>
#include <circle/screen.h>
#include <circle/serial.h>
#include <circle/sound/hdmisoundbasedevice.h>
#include <circle/timer.h>
#include <circle/types.h>
#include <circle/usb/usbhcidevice.h>
#include <SDCard/emmc.h>
#include <circle/fs/fat/fatfs.h>

#include "libretro/libretro_runner.h"
#include "platform/circle/circle_platform.h"
#include "platform/circle/circle_parallel.h"

enum TShutdownMode
{
	ShutdownNone,
	ShutdownHalt,
	ShutdownReboot
};

class CKernel
{
public:
	CKernel(void);
	~CKernel(void);

	boolean Initialize(void);
	TShutdownMode Run(void);

private:
	// Circle samples require this construction order.
	CActLED m_ActLED;
	CKernelOptions m_Options;
	CCPUThrottle m_CPUThrottle;
	CDeviceNameService m_DeviceNameService;
	CScreenDevice m_Screen;
	CSerialDevice m_Serial;
	CExceptionHandler m_ExceptionHandler;
	CInterruptSystem m_Interrupt;
	CTimer m_Timer;
	CLogger m_Logger;
#ifndef RA_BAREMETAL_NO_USB
	CUSBHCIDevice *m_pUSBHCI;
#endif
	CEMMCDevice *m_pEMMC;
	CFATFileSystem *m_pFileSystem;
	CHDMISoundBaseDevice *m_pSound;

	CircleLog m_Log;
	CircleTimer m_FrameTimer;
	CircleVideo m_Video;
	CircleAudio m_Audio;
#ifndef RA_BAREMETAL_NO_USB
	CircleInput m_Input;
#endif
	CircleFs m_Fs;
	CircleParallel m_Parallel;
	LibretroRunner m_Runner;
};

#endif
