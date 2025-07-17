/*
 * wpa_supplicant/hostapd / OS specific functions for Win32 systems
 * Copyright (c) 2005-2006, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"
#include <time.h>
#include <winsock2.h>
#include <wincrypt.h>

#include "os.h"
#include "common.h"

void os_sleep(os_time_t sec, os_time_t usec)
{
	if (sec)
		Sleep(sec * 1000);
	if (usec)
		Sleep(usec / 1000);
}


int os_get_time(struct os_time *t)
{
#define EPOCHFILETIME (116444736000000000ULL)
	FILETIME ft;
	LARGE_INTEGER li;
	ULONGLONG tt;

#ifdef _WIN32_WCE
	SYSTEMTIME st;

	GetSystemTime(&st);
	SystemTimeToFileTime(&st, &ft);
#else /* _WIN32_WCE */
	GetSystemTimeAsFileTime(&ft);
#endif /* _WIN32_WCE */
	li.LowPart = ft.dwLowDateTime;
	li.HighPart = ft.dwHighDateTime;
	tt = (li.QuadPart - EPOCHFILETIME) / 10;
	t->sec = (os_time_t) (tt / 1000000);
	t->usec = (os_time_t) (tt % 1000000);

	return 0;
}


int os_get_reltime(struct os_reltime *t)
{
	/* consider using performance counters or so instead */
	struct os_time now;
	int res = os_get_time(&now);
	t->sec = now.sec;
	t->usec = now.usec;
	return res;
}


int os_mktime(int year, int month, int day, int hour, int min, int sec,
	      os_time_t *t)
{
	struct tm tm, *tm1;
	time_t t_local, t1, t2;
	os_time_t tz_offset;

	if (year < 1970 || month < 1 || month > 12 || day < 1 || day > 31 ||
	    hour < 0 || hour > 23 || min < 0 || min > 59 || sec < 0 ||
	    sec > 60)
		return -1;

	memset(&tm, 0, sizeof(tm));
	tm.tm_year = year - 1900;
	tm.tm_mon = month - 1;
	tm.tm_mday = day;
	tm.tm_hour = hour;
	tm.tm_min = min;
	tm.tm_sec = sec;

	t_local = mktime(&tm);

	/* figure out offset to UTC */
	tm1 = localtime(&t_local);
	if (tm1) {
		t1 = mktime(tm1);
		tm1 = gmtime(&t_local);
		if (tm1) {
			t2 = mktime(tm1);
			tz_offset = t2 - t1;
		} else
			tz_offset = 0;
	} else
		tz_offset = 0;

	*t = (os_time_t) t_local - tz_offset;
	return 0;
}


int os_gmtime(os_time_t t, struct os_tm *tm)
{
	struct tm *tm2;
	time_t t2 = t;

	tm2 = gmtime(&t2);
	if (tm2 == NULL)
		return -1;
	tm->sec = tm2->tm_sec;
	tm->min = tm2->tm_min;
	tm->hour = tm2->tm_hour;
	tm->day = tm2->tm_mday;
	tm->month = tm2->tm_mon + 1;
	tm->year = tm2->tm_year + 1900;
	return 0;
}


int os_daemonize(const char *pid_file)
{
	/* TODO */
	return -1;
}


void os_daemonize_terminate(const char *pid_file)
{
}


int os_get_random(unsigned char *buf, size_t len)
{
	HCRYPTPROV prov;
	BOOL ret;

	if (!CryptAcquireContext(&prov, NULL, NULL, PROV_RSA_FULL,
				 CRYPT_VERIFYCONTEXT))
		return -1;

	ret = CryptGenRandom(prov, len, buf);
	CryptReleaseContext(prov, 0);

	return ret ? 0 : -1;
}


unsigned long os_random(void)
{
	return rand();
}


char * os_rel2abs_path(const char *rel_path)
{
	return _strdup(rel_path);
}


int os_program_init(void)
{
#ifdef CONFIG_NATIVE_WINDOWS
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 0), &wsaData)) {
		printf("Could not find a usable WinSock.dll\n");
		return -1;
	}
#endif /* CONFIG_NATIVE_WINDOWS */
	return 0;
}


void os_program_deinit(void)
{
#ifdef CONFIG_NATIVE_WINDOWS
	WSACleanup();
#endif /* CONFIG_NATIVE_WINDOWS */
}


int os_setenv(const char *name, const char *value, int overwrite)
{
	return -1;
}


int os_unsetenv(const char *name)
{
	return -1;
}


char * os_readfile(const char *name, size_t *len)
{
	FILE *f;
	char *buf;

	f = fopen(name, "rb");
	if (f == NULL)
		return NULL;

	fseek(f, 0, SEEK_END);
	*len = ftell(f);
	fseek(f, 0, SEEK_SET);

	buf = malloc(*len);
	if (buf == NULL) {
		fclose(f);
		return NULL;
	}

	fread(buf, 1, *len, f);
	fclose(f);

	return buf;
}


int os_fdatasync(FILE *stream)
{
	HANDLE h;

	if (stream == NULL)
		return -1;

	h = (HANDLE) _get_osfhandle(_fileno(stream));
	if (h == INVALID_HANDLE_VALUE)
		return -1;

	if (!FlushFileBuffers(h))
		return -1;

	return 0;
}


void * os_zalloc(size_t size)
{
	return calloc(1, size);
}


size_t os_strlcpy(char *dest, const char *src, size_t siz)
{
	const char *s = src;
	size_t left = siz;

	if (left) {
		/* Copy string up to the maximum size of the dest buffer */
		while (--left != 0) {
			if ((*dest++ = *s++) == '\0')
				break;
		}
	}

	if (left == 0) {
		/* Not enough room for the string; force NUL-termination */
		if (siz != 0)
			*dest = '\0';
		while (*s++)
			; /* determine total src string length */
	}

	return s - src - 1;
}


int os_memcmp_const(const void *a, const void *b, size_t len)
{
	const u8 *aa = a;
	const u8 *bb = b;
	size_t i;
	u8 res;

	for (res = 0, i = 0; i < len; i++)
		res |= aa[i] ^ bb[i];

	return res;
}


int os_exec(const char *program, const char *arg, int wait_completion)
{
	return -1;
}


void * os_memdup(const void *src, size_t len)
{
	void *r = os_malloc(len);

	if (r)
		os_memcpy(r, src, len);
	return r;
}
