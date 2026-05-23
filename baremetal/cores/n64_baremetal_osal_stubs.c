#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

int osal_mkdirp(const void *dirpath, ...)
{
	(void)dirpath;
	return 0;
}

const char *osal_get_shared_filepath(const char *filename, const char *firstsearch, const char *secondsearch)
{
	static char path[1024];
	const char *base = firstsearch && firstsearch[0] ? firstsearch : secondsearch;
	if (!base || !base[0])
	{
		base = "/";
	}

	strncpy(path, base, sizeof(path) - 1);
	path[sizeof(path) - 1] = 0;

	if (filename && filename[0])
	{
		const size_t len = strlen(path);
		if (len > 0 && path[len - 1] != '/' && len < sizeof(path) - 1)
		{
			strcat(path, "/");
		}
		strncat(path, filename, sizeof(path) - strlen(path) - 1);
	}

	return path;
}

const char *osal_get_user_configpath(void)
{
	return "/";
}

const char *osal_get_user_datapath(void)
{
	return "/";
}

const char *osal_get_user_cachepath(void)
{
	return "/";
}

int osal_path_existsA(const char *path)
{
	(void)path;
	return 0;
}

int osal_path_existsW(const wchar_t *path)
{
	(void)path;
	return 0;
}

int osal_is_absolute_path(const wchar_t *name)
{
	return name && name[0] == L'/';
}

int osal_is_directory(const wchar_t *name)
{
	(void)name;
	return 0;
}

void *osal_search_dir_open(const wchar_t *pathname)
{
	(void)pathname;
	return 0;
}

const wchar_t *osal_search_dir_read_next(void *dir_handle)
{
	(void)dir_handle;
	return 0;
}

void osal_search_dir_close(void *dir_handle)
{
	(void)dir_handle;
}
