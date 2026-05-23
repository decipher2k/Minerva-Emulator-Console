#include "kernel.h"

#include <circle/new.h>
#include <circle/startup.h>

static unsigned char s_KernelStorage[sizeof(CKernel)] __attribute__((aligned(64)));

int main(void)
{
	CKernel *pKernel = new (s_KernelStorage) CKernel;

	if (!pKernel->Initialize())
	{
		halt();
		return EXIT_HALT;
	}

	TShutdownMode ShutdownMode = pKernel->Run();

	switch (ShutdownMode)
	{
	case ShutdownReboot:
		reboot();
		return EXIT_REBOOT;

	case ShutdownHalt:
	default:
		halt();
		return EXIT_HALT;
	}
}
