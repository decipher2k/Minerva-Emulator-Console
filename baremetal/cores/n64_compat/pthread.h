#ifndef RA_BAREMETAL_N64_PTHREAD_H
#define RA_BAREMETAL_N64_PTHREAD_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _PTHREAD_MUTEX_INITIALIZER
#define _PTHREAD_MUTEX_INITIALIZER ((pthread_mutex_t)0)
#endif

#ifndef PTHREAD_MUTEX_INITIALIZER
#define PTHREAD_MUTEX_INITIALIZER _PTHREAD_MUTEX_INITIALIZER
#endif

static inline int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
	(void)attr;
	if (mutex)
	{
		*mutex = 0;
	}
	return 0;
}

static inline int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	(void)mutex;
	return 0;
}

static inline int pthread_mutex_lock(pthread_mutex_t *mutex)
{
	(void)mutex;
	return 0;
}

static inline int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
	(void)mutex;
	return 0;
}

static inline int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg)
{
	(void)thread;
	(void)attr;
	(void)start_routine;
	(void)arg;
	return -1;
}

static inline int pthread_join(pthread_t thread, void **retval)
{
	(void)thread;
	if (retval)
	{
		*retval = 0;
	}
	return 0;
}

#ifdef __cplusplus
}
#endif

#endif
