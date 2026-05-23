#include <stdbool.h>
#include <stdint.h>

#include "api/m64p_plugin.h"
#include "api/m64p_types.h"
#include "glsm/glsm_state_ctl.h"

bool threaded_gl_safe_shutdown = true;

bool glsm_ctl(enum glsm_state_ctl state, void *data)
{
	(void)state;
	(void)data;
	return false;
}

void *glsm_get_proc_address(const char *sym)
{
	(void)sym;
	return 0;
}

unsigned int glsm_get_current_framebuffer(void)
{
	return 0;
}

void gln64_thr_gl_invoke_command_loop(void)
{
}

void gln64DestroyGfxContext(void)
{
}

void gln64ReinitGfxContext(void)
{
}

m64p_error gln64PluginGetVersion(m64p_plugin_type *PluginType, int *PluginVersion,
	int *APIVersion, const char **PluginNamePtr, int *Capabilities)
{
	if (PluginType) *PluginType = M64PLUGIN_GFX;
	if (PluginVersion) *PluginVersion = 0;
	if (APIVersion) *APIVersion = 0;
	if (PluginNamePtr) *PluginNamePtr = "GLideN64 disabled on bare metal";
	if (Capabilities) *Capabilities = 0;
	return M64ERR_SUCCESS;
}

void gln64ChangeWindow(void) {}
int gln64InitiateGFX(GFX_INFO Gfx_Info) { (void)Gfx_Info; return 0; }
void gln64MoveScreen(int x, int y) { (void)x; (void)y; }
void gln64ProcessDList(void) {}
void gln64ProcessRDPList(void) {}
void gln64RomClosed(void) {}
int gln64RomOpen(void) { return 0; }
void gln64ShowCFB(void) {}
void gln64UpdateScreen(void) {}
void gln64ViStatusChanged(void) {}
void gln64ViWidthChanged(void) {}
void gln64ReadScreen2(void *dest, int *width, int *height, int front)
{
	(void)dest;
	(void)width;
	(void)height;
	(void)front;
}
void gln64SetRenderingCallback(void (*callback)(int)) { (void)callback; }
void gln64ResizeVideoOutput(int width, int height) { (void)width; (void)height; }
void gln64FBRead(unsigned int addr) { (void)addr; }
void gln64FBWrite(unsigned int addr, unsigned int size) { (void)addr; (void)size; }
void gln64FBGetFrameBufferInfo(void *p) { (void)p; }
