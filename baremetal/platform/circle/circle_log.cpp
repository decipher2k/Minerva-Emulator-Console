#include "circle_platform.h"

static const char FromCircleLog[] = "ra-circle";

CircleLog::CircleLog(void)
:	m_pLogger(0)
{
}

void CircleLog::Init(CLogger *pLogger)
{
	m_pLogger = pLogger;
}

void CircleLog::Notice(const char *pMessage)
{
	if (m_pLogger)
	{
		m_pLogger->Write(FromCircleLog, LogNotice, "%s", pMessage ? pMessage : "");
	}
}

void CircleLog::Warn(const char *pMessage)
{
	if (m_pLogger)
	{
		m_pLogger->Write(FromCircleLog, LogWarning, "%s", pMessage ? pMessage : "");
	}
}

void CircleLog::Error(const char *pMessage)
{
	if (m_pLogger)
	{
		m_pLogger->Write(FromCircleLog, LogError, "%s", pMessage ? pMessage : "");
	}
}
