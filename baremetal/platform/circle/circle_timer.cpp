#include "circle_platform.h"

CircleTimer::CircleTimer(void)
:	m_pTimer(0),
	m_NextFrameUsec(0)
{
}

void CircleTimer::Init(CTimer *pTimer)
{
	m_pTimer = pTimer;
	m_NextFrameUsec = NowUsec();
}

uint64_t CircleTimer::NowUsec(void) const
{
	return CTimer::GetClockTicks64();
}

void CircleTimer::WaitNextFrame(double fps)
{
	if (fps < 1.0)
	{
		fps = 60.0;
	}

	const uint64_t frameUsec = (uint64_t)(1000000.0 / fps + 0.5);
	const uint64_t now = NowUsec();

	if (m_NextFrameUsec == 0 || now > m_NextFrameUsec + frameUsec * 4)
	{
		m_NextFrameUsec = now + frameUsec;
		return;
	}

	m_NextFrameUsec += frameUsec;

	while (NowUsec() < m_NextFrameUsec)
	{
		const uint64_t remaining = m_NextFrameUsec - NowUsec();
		if (remaining > 4000 && m_pTimer)
		{
			m_pTimer->MsDelay(1);
		}
	}
}
