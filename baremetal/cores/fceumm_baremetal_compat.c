#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <file/file_path.h>
#include <streams/file_stream.h>

struct RFILE
{
	int unused;
};

const char _ctype_[257] =
{
	[1 ... 8] = 040,
	['\t' + 1] = 010 | 040,
	['\n' + 1] = 010 | 040,
	['\v' + 1] = 010 | 040,
	['\f' + 1] = 010 | 040,
	['\r' + 1] = 010 | 040,
	[14 ... 32] = 040,
	[' ' + 1] = 010 | 0200,
	['!' + 1 ... '/' + 1] = 020,
	['0' + 1 ... '9' + 1] = 04 | 0100,
	[':' + 1 ... '@' + 1] = 020,
	['A' + 1 ... 'F' + 1] = 01 | 0100,
	['G' + 1 ... 'Z' + 1] = 01,
	['[' + 1 ... '`' + 1] = 020,
	['a' + 1 ... 'f' + 1] = 02 | 0100,
	['g' + 1 ... 'z' + 1] = 02,
	['{' + 1 ... '~' + 1] = 020,
	[127 + 1] = 040,
};

static int is_space(int c)
{
	return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

double atof(const char *s)
{
	double value = 0.0;
	double frac = 0.1;
	int sign = 1;

	if (!s)
	{
		return 0.0;
	}

	while (is_space((unsigned char)*s))
	{
		s++;
	}

	if (*s == '-')
	{
		sign = -1;
		s++;
	}
	else if (*s == '+')
	{
		s++;
	}

	while (*s >= '0' && *s <= '9')
	{
		value = value * 10.0 + (double)(*s++ - '0');
	}

	if (*s == '.')
	{
		s++;
		while (*s >= '0' && *s <= '9')
		{
			value += (double)(*s++ - '0') * frac;
			frac *= 0.1;
		}
	}

	return sign < 0 ? -value : value;
}

int toupper(int c)
{
	return c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c;
}

int tolower(int c)
{
	return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c;
}

char *strrchr(const char *s, int c)
{
	const char *last = 0;

	if (!s)
	{
		return 0;
	}

	do
	{
		if (*s == (char)c)
		{
			last = s;
		}
	} while (*s++);

	return (char *)last;
}

char *string_trim_whitespace_left(char *const s)
{
	char *p = s;
	char *out = s;

	if (!s)
	{
		return 0;
	}

	while (*p && is_space((unsigned char)*p))
	{
		p++;
	}

	if (p != s)
	{
		while ((*out++ = *p++))
		{
		}
	}

	return s;
}

char *string_trim_whitespace_right(char *const s)
{
	size_t len;

	if (!s)
	{
		return 0;
	}

	len = strlen(s);
	while (len && is_space((unsigned char)s[len - 1]))
	{
		s[--len] = 0;
	}

	return s;
}

char *string_trim_whitespace(char *const s)
{
	string_trim_whitespace_right(s);
	return string_trim_whitespace_left(s);
}

static void out_char(char *dst, size_t size, size_t *pos, char c)
{
	if (dst && size && *pos + 1 < size)
	{
		dst[*pos] = c;
	}

	(*pos)++;
}

static void out_string(char *dst, size_t size, size_t *pos, const char *s, int width, char pad)
{
	size_t len;

	if (!s)
	{
		s = "(null)";
	}

	len = strlen(s);
	while (width > (int)len)
	{
		out_char(dst, size, pos, pad);
		width--;
	}

	while (*s)
	{
		out_char(dst, size, pos, *s++);
	}
}

static void out_unsigned(char *dst, size_t size, size_t *pos, unsigned long long value,
                         unsigned base, bool upper, bool negative, int width, char pad)
{
	char tmp[32];
	unsigned n = 0;
	const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
	int len;

	do
	{
		tmp[n++] = digits[value % base];
		value /= base;
	} while (value && n < sizeof(tmp));

	len = (int)n + (negative ? 1 : 0);
	if (negative && pad == '0')
	{
		out_char(dst, size, pos, '-');
		negative = false;
	}

	while (width > len)
	{
		out_char(dst, size, pos, pad);
		width--;
	}

	if (negative)
	{
		out_char(dst, size, pos, '-');
	}

	while (n)
	{
		out_char(dst, size, pos, tmp[--n]);
	}
}

int vsnprintf(char *dst, size_t size, const char *fmt, va_list ap)
{
	size_t pos = 0;

	if (!fmt)
	{
		if (dst && size)
		{
			dst[0] = 0;
		}
		return 0;
	}

	while (*fmt)
	{
		if (*fmt != '%')
		{
			out_char(dst, size, &pos, *fmt++);
			continue;
		}

		fmt++;
		if (*fmt == '%')
		{
			out_char(dst, size, &pos, *fmt++);
			continue;
		}

		char pad = ' ';
		int width = 0;
		int long_count = 0;
		bool size_t_arg = false;

		if (*fmt == '0')
		{
			pad = '0';
			fmt++;
		}

		while (*fmt >= '0' && *fmt <= '9')
		{
			width = width * 10 + (*fmt++ - '0');
		}

		if (*fmt == 'z')
		{
			size_t_arg = true;
			fmt++;
		}
		else
		{
			while (*fmt == 'l')
			{
				long_count++;
				fmt++;
			}
		}

		switch (*fmt)
		{
		case 's':
			out_string(dst, size, &pos, va_arg(ap, const char *), width, pad);
			break;
		case 'c':
			out_char(dst, size, &pos, (char)va_arg(ap, int));
			break;
		case 'd':
		case 'i':
		{
			long long value;
			if (size_t_arg)
			{
				value = (long long)va_arg(ap, size_t);
			}
			else if (long_count >= 2)
			{
				value = va_arg(ap, long long);
			}
			else if (long_count == 1)
			{
				value = va_arg(ap, long);
			}
			else
			{
				value = va_arg(ap, int);
			}

			if (value < 0)
			{
				out_unsigned(dst, size, &pos, (unsigned long long)-value, 10, false, true, width, pad);
			}
			else
			{
				out_unsigned(dst, size, &pos, (unsigned long long)value, 10, false, false, width, pad);
			}
			break;
		}
		case 'u':
		case 'x':
		case 'X':
		{
			unsigned long long value;
			if (size_t_arg)
			{
				value = (unsigned long long)va_arg(ap, size_t);
			}
			else if (long_count >= 2)
			{
				value = va_arg(ap, unsigned long long);
			}
			else if (long_count == 1)
			{
				value = va_arg(ap, unsigned long);
			}
			else
			{
				value = va_arg(ap, unsigned int);
			}
			out_unsigned(dst, size, &pos, value, *fmt == 'u' ? 10U : 16U, *fmt == 'X', false, width, pad);
			break;
		}
		case 'p':
			out_string(dst, size, &pos, "0x", 0, ' ');
			out_unsigned(dst, size, &pos, (uintptr_t)va_arg(ap, void *), 16, false, false, width, '0');
			break;
		default:
			out_char(dst, size, &pos, '%');
			if (*fmt)
			{
				out_char(dst, size, &pos, *fmt);
			}
			break;
		}

		if (*fmt)
		{
			fmt++;
		}
	}

	if (dst && size)
	{
		dst[pos < size ? pos : size - 1] = 0;
	}

	return (int)pos;
}

int snprintf(char *dst, size_t size, const char *fmt, ...)
{
	int ret;
	va_list ap;
	va_start(ap, fmt);
	ret = vsnprintf(dst, size, fmt, ap);
	va_end(ap);
	return ret;
}

int sprintf(char *dst, const char *fmt, ...)
{
	int ret;
	va_list ap;
	va_start(ap, fmt);
	ret = vsnprintf(dst, (size_t)-1, fmt, ap);
	va_end(ap);
	return ret;
}

static size_t copy_part(char *dst, size_t size, size_t pos, const char *src)
{
	size_t total = pos;

	if (!src)
	{
		return total;
	}

	while (*src)
	{
		if (pos + 1 < size)
		{
			dst[pos] = *src;
		}

		pos++;
		total++;
		src++;
	}

	if (size)
	{
		dst[pos < size ? pos : size - 1] = 0;
	}

	return total;
}

size_t fill_pathname_join(char *out_path, const char *dir, const char *file, size_t size)
{
	size_t total = 0;
	size_t dir_len = 0;

	if (!out_path || !size)
	{
		return 0;
	}

	out_path[0] = 0;

	if (dir)
	{
		dir_len = strlen(dir);
		total = copy_part(out_path, size, 0, dir);
	}

	if (dir_len && dir[dir_len - 1] != '/' && dir[dir_len - 1] != '\\')
	{
		const char slash[2] = {'/', 0};
		total = copy_part(out_path, size, total, slash);
	}

	return copy_part(out_path, size, total, file);
}

bool path_is_valid(const char *path)
{
	(void)path;
	return false;
}

void filestream_vfs_init(const struct retro_vfs_interface_info *vfs_info)
{
	(void)vfs_info;
}

RFILE *filestream_open(const char *path, unsigned mode, unsigned hints)
{
	(void)path;
	(void)mode;
	(void)hints;
	return NULL;
}

int64_t filestream_get_size(RFILE *stream)
{
	(void)stream;
	return -1;
}

int64_t filestream_truncate(RFILE *stream, int64_t length)
{
	(void)stream;
	(void)length;
	return -1;
}

int64_t filestream_seek(RFILE *stream, int64_t offset, int seek_position)
{
	(void)stream;
	(void)offset;
	(void)seek_position;
	return -1;
}

int64_t filestream_read(RFILE *stream, void *data, int64_t len)
{
	(void)stream;
	(void)data;
	(void)len;
	return -1;
}

int64_t filestream_write(RFILE *stream, const void *data, int64_t len)
{
	(void)stream;
	(void)data;
	(void)len;
	return -1;
}

int64_t filestream_tell(RFILE *stream)
{
	(void)stream;
	return -1;
}

void filestream_rewind(RFILE *stream)
{
	(void)stream;
}

int filestream_close(RFILE *stream)
{
	(void)stream;
	return -1;
}
