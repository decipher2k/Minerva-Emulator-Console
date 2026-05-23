#include "circle_platform.h"

static const unsigned AudioQueueMilliseconds = 320;

CircleAudio::CircleAudio(void)
:	m_pSound(0)
{
}

bool CircleAudio::Init(CSoundBaseDevice *pSound, unsigned sampleRate)
{
	m_pSound = 0;
	if (!pSound)
	{
		return true;
	}

	if (!pSound->AllocateQueue(AudioQueueMilliseconds))
	{
		return false;
	}

	pSound->SetWriteFormat(SoundFormatSigned16, 2);

	static const int16_t silence[512 * 2] = {0};
	if (sampleRate < 1000)
	{
		sampleRate = 48000;
	}
	unsigned framesToPrime = sampleRate * 60 / 1000;
	const unsigned maxPrime = pSound->GetQueueSizeFrames() / 2;
	if (framesToPrime > maxPrime)
	{
		framesToPrime = maxPrime;
	}
	while (framesToPrime > 0)
	{
		const unsigned chunkFrames = framesToPrime > 512 ? 512 : framesToPrime;
		const int written = pSound->Write(silence, chunkFrames * 2 * sizeof(int16_t));
		if (written <= 0)
		{
			break;
		}

		const unsigned writtenFrames = (unsigned)written / (2 * sizeof(int16_t));
		if (writtenFrames == 0)
		{
			break;
		}

		framesToPrime -= writtenFrames;
	}

	if (!pSound->Start())
	{
		return false;
	}

	m_pSound = pSound;
	return true;
}

void CircleAudio::WriteSample(int16_t left, int16_t right)
{
	int16_t frame[2];
	frame[0] = left;
	frame[1] = right;
	WriteFrames(frame, 1);
}

size_t CircleAudio::WriteFrames(const int16_t *samples, size_t frames)
{
	if (!m_pSound || !samples || frames == 0)
	{
		return frames;
	}

	const size_t bytes = frames * 2 * sizeof(int16_t);
	const int written = m_pSound->Write(samples, bytes);
	if (written <= 0)
	{
		return frames;
	}

	const size_t writtenFrames = (size_t)written / (2 * sizeof(int16_t));
	(void)writtenFrames;
	return frames;
}
