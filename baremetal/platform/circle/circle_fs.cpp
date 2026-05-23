#include "circle_platform.h"

#include <string.h>

static const unsigned FS_ERROR_VALUE = 0xFFFFFFFFU;

static bool CharEqualNoCase(char a, char b)
{
	if (a >= 'A' && a <= 'Z')
	{
		a = (char)(a - 'A' + 'a');
	}
	if (b >= 'A' && b <= 'Z')
	{
		b = (char)(b - 'A' + 'a');
	}

	return a == b;
}

static bool HasExtension(const char *path, const char *extension)
{
	if (!path || !extension)
	{
		return false;
	}

	const char *dot = 0;
	for (const char *p = path; *p; p++)
	{
		if (*p == '.')
		{
			dot = p;
		}
	}

	if (!dot)
	{
		return false;
	}

	dot++;
	while (*dot && *extension)
	{
		if (!CharEqualNoCase(*dot++, *extension++))
		{
			return false;
		}
	}

	return *dot == 0 && *extension == 0;
}

static void CopyPath(char *dst, size_t dstSize, const char *src)
{
	if (!dst || dstSize == 0)
	{
		return;
	}

	size_t i = 0;
	if (src)
	{
		for (; src[i] && i + 1 < dstSize; i++)
		{
			dst[i] = src[i];
		}
	}
	dst[i] = 0;
}

CircleFs::CircleFs(void)
:	m_pFileSystem(0),
	m_pLog(0)
{
}

void CircleFs::Init(CFATFileSystem *pFileSystem, CircleLog *pLog)
{
	m_pFileSystem = pFileSystem;
	m_pLog = pLog;
}

bool CircleFs::ListDirectory(const char *path, Entry *entries, unsigned maxEntries, unsigned *pCount)
{
	if (pCount)
	{
		*pCount = 0;
	}

	if (!m_pFileSystem || !entries || maxEntries == 0 || !pCount)
	{
		return false;
	}

	TDirentry entry;
	TFindCurrentEntry current;
	memset(&entry, 0, sizeof(entry));
	memset(&current, 0, sizeof(current));

	unsigned found = m_pFileSystem->DirectoryFindFirst(path ? path : "", &entry, &current);
	while (found)
	{
		if (strcmp(entry.chTitle, ".") != 0 && strcmp(entry.chTitle, "..") != 0)
		{
			if (*pCount < maxEntries)
			{
				Entry *pOut = &entries[*pCount];
				CopyPath(pOut->name, sizeof(pOut->name), entry.chTitle);
				pOut->size = entry.nSize;
				pOut->isDirectory = (entry.nAttributes & FS_ATTRIB_DIRECTORY) != 0;
				(*pCount)++;
			}
		}

		memset(&entry, 0, sizeof(entry));
		found = m_pFileSystem->DirectoryFindNext(&entry, &current);
	}

	return true;
}

bool CircleFs::ReadWholeFile(const char *path, uint8_t **ppData, size_t *pSize, size_t maxSize)
{
	if (ppData)
	{
		*ppData = 0;
	}

	if (pSize)
	{
		*pSize = 0;
	}

	if (!m_pFileSystem || !path || !ppData || !pSize || maxSize == 0)
	{
		return false;
	}

	if (m_pLog)
	{
		char message[64];
		CopyPath(message, sizeof(message), "Trying ROM: ");
		const size_t prefixLen = strlen(message);
		CopyPath(message + prefixLen, sizeof(message) - prefixLen, path);
		m_pLog->Notice(message);
	}

	unsigned hFile = m_pFileSystem->FileOpen(path);
	if (hFile == 0)
	{
		if (m_pLog)
		{
			char message[64];
			CopyPath(message, sizeof(message), "ROM open failed: ");
			const size_t prefixLen = strlen(message);
			CopyPath(message + prefixLen, sizeof(message) - prefixLen, path);
			m_pLog->Warn(message);
		}
		return false;
	}

	uint8_t *pData = new uint8_t[maxSize];
	if (!pData)
	{
		m_pFileSystem->FileClose(hFile);
		return false;
	}

	size_t used = 0;
	while (used < maxSize)
	{
		const unsigned toRead = (maxSize - used) > 4096 ? 4096 : (unsigned)(maxSize - used);
		const unsigned result = m_pFileSystem->FileRead(hFile, pData + used, toRead);
		if (result == 0)
		{
			break;
		}

		if (result == FS_ERROR_VALUE)
		{
			delete[] pData;
			m_pFileSystem->FileClose(hFile);
			return false;
		}

		used += result;
	}

	m_pFileSystem->FileClose(hFile);

	*ppData = pData;
	*pSize = used;

	if (m_pLog)
	{
		char message[64];
		CopyPath(message, sizeof(message), "ROM loaded: ");
		const size_t prefixLen = strlen(message);
		CopyPath(message + prefixLen, sizeof(message) - prefixLen, path);
		m_pLog->Notice(message);
	}
	return true;
}

bool CircleFs::FindFirstWithExtension(const char *extension, char *path, size_t pathSize)
{
	if (path && pathSize)
	{
		path[0] = 0;
	}

	if (!m_pFileSystem || !extension || !path || pathSize == 0)
	{
		return false;
	}

	TDirentry entry;
	TFindCurrentEntry current;
	memset(&entry, 0, sizeof(entry));
	memset(&current, 0, sizeof(current));

	unsigned found = m_pFileSystem->RootFindFirst(&entry, &current);
	while (found)
	{
		if (m_pLog)
		{
			char message[64];
			CopyPath(message, sizeof(message), "SD root entry: ");
			const size_t prefixLen = strlen(message);
			CopyPath(message + prefixLen, sizeof(message) - prefixLen, entry.chTitle);
			m_pLog->Notice(message);
		}

		if (HasExtension(entry.chTitle, extension))
		{
			CopyPath(path, pathSize, entry.chTitle);
			return true;
		}

		memset(&entry, 0, sizeof(entry));
		found = m_pFileSystem->RootFindNext(&entry, &current);
	}

	return false;
}

bool CircleFs::WriteWholeFile(const char *path, const void *pData, size_t size)
{
	if (!m_pFileSystem || !path || (!pData && size))
	{
		return false;
	}

	unsigned hFile = m_pFileSystem->FileCreate(path);
	if (hFile == 0)
	{
		return false;
	}

	size_t written = 0;
	while (written < size)
	{
		const unsigned chunk = (size - written) > 4096 ? 4096 : (unsigned)(size - written);
		const unsigned result = m_pFileSystem->FileWrite(hFile, (const uint8_t *)pData + written, chunk);
		if (result == FS_ERROR_VALUE || result == 0)
		{
			m_pFileSystem->FileClose(hFile);
			return false;
		}

		written += result;
	}

	return m_pFileSystem->FileClose(hFile) != 0;
}
