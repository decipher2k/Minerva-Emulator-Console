#ifndef RA_BAREMETAL_ROM_BROWSER_H
#define RA_BAREMETAL_ROM_BROWSER_H

#include <stddef.h>

class CScreenDevice;
class CTimer;
class CircleFs;
class CircleInput;
struct LibretroCore;

bool SelectRomAtBoot(CScreenDevice *pScreen,
                     CTimer *pTimer,
                     CircleFs *pFs,
                     CircleInput *pInput,
                     char *romPath,
                     size_t romPathSize,
                     const LibretroCore **ppCore);

#endif
