#include <circle/actled.h>
#include <circle/devicenameservice.h>
#include <circle/exceptionhandler.h>
#include <circle/interrupt.h>
#include <circle/koptions.h>
#include <circle/logger.h>
#include <circle/screen.h>
#include <circle/serial.h>
#include <circle/startup.h>
#include <circle/timer.h>

#include <string.h>

static void WriteLine(CScreenDevice &screen, const char *text)
{
	screen.Write(text, strlen(text));
	screen.Write("\n", 1);
	screen.Update();
}

static void DrawBands(CScreenDevice &screen, unsigned phase)
{
	const TScreenColor colors[] = {
		BRIGHT_RED_COLOR,
		BRIGHT_GREEN_COLOR,
		BRIGHT_BLUE_COLOR,
		BRIGHT_WHITE_COLOR,
	};
	const unsigned width = screen.GetWidth();
	const unsigned height = screen.GetHeight();
	const unsigned bandWidth = width / 4 ? width / 4 : 1;

	for (unsigned y = 0; y < height; y++)
	{
		for (unsigned x = 0; x < width; x++)
		{
			unsigned index = ((x / bandWidth) + phase) & 3;
			screen.SetPixel(x, y, colors[index]);
		}
	}

	screen.Update();
}

static void BlinkForever(CActLED &actLED, unsigned count)
{
	while (1)
	{
		actLED.Blink(count, 120, 120);
		CTimer::SimpleMsDelay(900);
	}
}

int main(void)
{
	CActLED actLED;
	actLED.Blink(3, 100, 100);

	CKernelOptions options;
	CDeviceNameService deviceNameService;
	CScreenDevice screen(options.GetWidth(), options.GetHeight());
	CSerialDevice serial;
	CExceptionHandler exceptionHandler;
	CInterruptSystem interrupt;
	CTimer timer(&interrupt);
	CLogger logger(options.GetLogLevel(), &timer);

	if (!screen.Initialize())
	{
		BlinkForever(actLED, 1);
	}

	DrawBands(screen, 0);
	WriteLine(screen, "Circle boot test: screen initialized");

	if (!serial.Initialize(115200))
	{
		WriteLine(screen, "serial init failed");
		BlinkForever(actLED, 2);
	}
	WriteLine(screen, "serial initialized");

	if (!logger.Initialize(&screen))
	{
		WriteLine(screen, "logger init failed");
		BlinkForever(actLED, 3);
	}
	logger.Write("boottest", LogNotice, "logger initialized");

	if (!interrupt.Initialize())
	{
		WriteLine(screen, "interrupt init failed");
		BlinkForever(actLED, 4);
	}
	WriteLine(screen, "interrupt initialized");

	if (!timer.Initialize())
	{
		WriteLine(screen, "timer init failed");
		BlinkForever(actLED, 5);
	}
	WriteLine(screen, "timer initialized");
	WriteLine(screen, "Boot test alive. LED should blink and colors should move.");

	for (unsigned phase = 0;; phase++)
	{
		DrawBands(screen, phase);
		WriteLine(screen, "Circle boot test alive");
		actLED.Blink(1, 40, 40);
		timer.MsDelay(1000);
	}

	return EXIT_HALT;
}
