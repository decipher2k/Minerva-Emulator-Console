#ifndef RA_BAREMETAL_CIRCLE_PARALLEL_H
#define RA_BAREMETAL_CIRCLE_PARALLEL_H

#include <stdint.h>

#include <circle/sysconfig.h>
#include <circle/types.h>

#ifdef ARM_ALLOW_MULTI_CORE
#include <circle/memory.h>
#include <circle/multicore.h>
#endif

class CircleParallel
#ifdef ARM_ALLOW_MULTI_CORE
	: public CMultiCoreSupport
#endif
{
public:
	typedef void (*TTask)(uint32_t workerId);

	CircleParallel(void);

	boolean Initialize(void);
	void ConfigureWorkers(uint32_t requestedWorkers);
	void RunTask(TTask pTask);
	uint32_t WorkerCount(void) const;
	void Close(void);

#ifdef ARM_ALLOW_MULTI_CORE
	void Run(unsigned nCore);
#endif

private:
	uint32_t ClampWorkerCount(uint32_t requestedWorkers) const;
	void WakeWorkers(void) const;
	void WorkerLoop(unsigned nCore);

private:
	boolean m_Started;
	uint32_t m_WorkerCount;
	uint32_t m_Generation;
	uint32_t m_DoneMask;
	TTask m_pTask;
};

#endif
