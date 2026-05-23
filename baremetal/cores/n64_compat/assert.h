#ifndef RA_BAREMETAL_N64_ASSERT_H
#define RA_BAREMETAL_N64_ASSERT_H

#include <stdlib.h>

#ifdef NDEBUG
#define assert(expr) ((void)0)
#else
#define assert(expr) ((expr) ? (void)0 : abort())
#endif

#endif
