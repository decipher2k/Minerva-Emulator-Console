#include <stddef.h>

extern "C" void circle_memory_port_anchor(void);

extern "C" void circle_memory_port_anchor(void)
{
	/*
	 * Circle supplies the initial heap/new/delete implementation. This file is
	 * intentionally kept as the future hook point for:
	 * - hard memory budget accounting per core
	 * - SRAM/state buffer pools
	 * - optional malloc/realloc/free wrappers for hostile cores
	 */
}
