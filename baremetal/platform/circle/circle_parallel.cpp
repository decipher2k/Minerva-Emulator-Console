#include "circle_parallel.h"

#include <circle/memorymap.h>
#include <circle/synchronize.h>

static CircleParallel *s_pCircleParallel = 0;

static inline void CircleWorkerWaitForEvent(void)
{
	asm volatile ("wfe" ::: "memory");
}

static inline void CircleWorkerSendEvent(void)
{
	asm volatile ("sev" ::: "memory");
}

static inline void YieldCpu(void)
{
	asm volatile ("yield" ::: "memory");
}

CircleParallel::CircleParallel(void)
#ifdef ARM_ALLOW_MULTI_CORE
:	CMultiCoreSupport(CMemorySystem::Get()),
	m_Started(FALSE),
#else
:	m_Started(FALSE),
#endif
	m_WorkerCount(1),
	m_Generation(0),
	m_DoneMask(0),
	m_pTask(0)
{
	s_pCircleParallel = this;
}

boolean CircleParallel::Initialize(void)
{
#ifdef ARM_ALLOW_MULTI_CORE
	if (m_Started)
	{
		return TRUE;
	}

	if (!CMultiCoreSupport::Initialize())
	{
		m_WorkerCount = 1;
		return FALSE;
	}

	m_Started = TRUE;
	return TRUE;
#else
	m_WorkerCount = 1;
	return TRUE;
#endif
}

uint32_t CircleParallel::ClampWorkerCount(uint32_t requestedWorkers) const
{
#ifdef ARM_ALLOW_MULTI_CORE
	if (!m_Started)
	{
		return 1;
	}

	if (requestedWorkers == 0)
	{
		requestedWorkers = CORES;
	}

	if (requestedWorkers > CORES)
	{
		requestedWorkers = CORES;
	}

	return requestedWorkers > 0 ? requestedWorkers : 1;
#else
	(void)requestedWorkers;
	return 1;
#endif
}

void CircleParallel::ConfigureWorkers(uint32_t requestedWorkers)
{
	const uint32_t workerCount = ClampWorkerCount(requestedWorkers);
	__atomic_store_n(&m_WorkerCount, workerCount, __ATOMIC_RELEASE);
	__atomic_store_n(&m_DoneMask, 0, __ATOMIC_RELEASE);
	__atomic_store_n(&m_pTask, 0, __ATOMIC_RELEASE);
	DataMemBarrier();
	WakeWorkers();
}

void CircleParallel::RunTask(TTask pTask)
{
	if (!pTask)
	{
		return;
	}

	const uint32_t workerCount = WorkerCount();
	if (workerCount <= 1)
	{
		pTask(0);
		return;
	}

	const uint32_t doneMask = ((1U << workerCount) - 1U) & ~1U;

	__atomic_store_n(&m_DoneMask, 0, __ATOMIC_RELEASE);
	__atomic_store_n(&m_pTask, pTask, __ATOMIC_RELEASE);
	DataMemBarrier();
	__atomic_add_fetch(&m_Generation, 1U, __ATOMIC_RELEASE);
	WakeWorkers();

	pTask(0);

	while ((__atomic_load_n(&m_DoneMask, __ATOMIC_ACQUIRE) & doneMask) != doneMask)
	{
		YieldCpu();
	}

	__atomic_store_n(&m_pTask, 0, __ATOMIC_RELEASE);
}

uint32_t CircleParallel::WorkerCount(void) const
{
	return __atomic_load_n(&m_WorkerCount, __ATOMIC_ACQUIRE);
}

void CircleParallel::Close(void)
{
	__atomic_store_n(&m_WorkerCount, 1U, __ATOMIC_RELEASE);
	__atomic_store_n(&m_pTask, 0, __ATOMIC_RELEASE);
	__atomic_add_fetch(&m_Generation, 1U, __ATOMIC_RELEASE);
	WakeWorkers();
}

#ifdef ARM_ALLOW_MULTI_CORE
void CircleParallel::Run(unsigned nCore)
{
	WorkerLoop(nCore);
}
#endif

void CircleParallel::WakeWorkers(void) const
{
	CircleWorkerSendEvent();
}

void CircleParallel::WorkerLoop(unsigned nCore)
{
#ifdef ARM_ALLOW_MULTI_CORE
	uint32_t seenGeneration = __atomic_load_n(&m_Generation, __ATOMIC_ACQUIRE);

	while (1)
	{
		uint32_t generation;
		do
		{
			CircleWorkerWaitForEvent();
			generation = __atomic_load_n(&m_Generation, __ATOMIC_ACQUIRE);
		}
		while (generation == seenGeneration);

		seenGeneration = generation;

		const uint32_t workerCount = __atomic_load_n(&m_WorkerCount, __ATOMIC_ACQUIRE);
		TTask pTask = __atomic_load_n(&m_pTask, __ATOMIC_ACQUIRE);
		if (pTask && nCore < workerCount)
		{
			pTask(nCore);
			__atomic_fetch_or(&m_DoneMask, 1U << nCore, __ATOMIC_RELEASE);
			WakeWorkers();
		}
	}
#else
	(void)nCore;
#endif
}

extern "C" void parallel_alinit(uint32_t num)
{
	if (s_pCircleParallel)
	{
		s_pCircleParallel->ConfigureWorkers(num);
	}
}

extern "C" void parallel_run(void task(uint32_t))
{
	if (s_pCircleParallel)
	{
		s_pCircleParallel->RunTask(task);
		return;
	}

	if (task)
	{
		task(0);
	}
}

extern "C" uint32_t parallel_num_workers(void)
{
	return s_pCircleParallel ? s_pCircleParallel->WorkerCount() : 1;
}

extern "C" void parallel_close(void)
{
	if (s_pCircleParallel)
	{
		s_pCircleParallel->Close();
	}
}
