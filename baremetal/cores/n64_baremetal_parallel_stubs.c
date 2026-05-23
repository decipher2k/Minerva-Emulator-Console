#include <stdint.h>

void parallel_alinit(uint32_t num)
{
	(void)num;
}

void parallel_run(void task(uint32_t))
{
	if (task)
	{
		task(0);
	}
}

uint32_t parallel_num_workers(void)
{
	return 1;
}

void parallel_close(void)
{
}
