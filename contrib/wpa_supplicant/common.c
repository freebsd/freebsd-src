/*
 * Host AP (software wireless LAN access point) user space daemon for
 * Host AP kernel driver / common helper functions, etc.
 * Copyright (c) 2002-2005, Jouni Malinen <jkmaline@cc.hut.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>
#include <sys/time.h>

#include "common.h"


int wpa_debug_level = MSG_INFO;
int wpa_debug_show_keys = 0;
int wpa_debug_timestamp = 0;


int hostapd_get_rand(u8 *buf, size_t len)
{
#ifdef CONFIG_NATIVE_WINDOWS
	int i;
	/* FIX: use more secure pseudo random number generator */
	for (i = 0; i < len; i++) {
		buf[i] = rand();
	}
	return 0;
#else /* CONFIG_NATIVE_WINDOWS */
	FILE *f;
	size_t rc;

	f = fopen("/dev/urandom", "r");
	if (f == NULL) {
		printf("Could not open /dev/urandom.\n");
		return -1;
	}

	rc = fread(buf, 1, len, f);
	fclose(f);

	return rc != len ? -1 : 0;
#endif /* CONFIG_NATIVE_WINDOWS */
}


void hostapd_hexdump(const char *title, const u8 *buf, size_t len)
{
	size_t i;
	printf("%s - hexdump(len=%lu):", title, (unsigned long) len);
	for (i = 0; i < len; i++)
		printf(" %02x", buf[i]);
	printf("\n");
}


static int hex2num(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}


static int hex2byte(const char *hex)
{
	int a, b;
	a = hex2num(*hex++);
	if (a < 0)
		return -1;
	b = hex2num(*hex++);
	if (b < 0)
		return -1;
	return (a << 4) | b;
}


int hwaddr_aton(const char *txt, u8 *addr)
{
	int i;

	for (i = 0; i < 6; i++) {
		int a, b;

		a = hex2num(*txt++);
		if (a < 0)
			return -1;
		b = hex2num(*txt++);
		if (b < 0)
			return -1;
		*addr++ = (a << 4) | b;
		if (i < 5 && *txt++ != ':')
			return -1;
	}

	return 0;
}


int hexstr2bin(const char *hex, u8 *buf, size_t len)
{
	int i, a;
	const char *ipos = hex;
	u8 *opos = buf;

	for (i = 0; i < len; i++) {
		a = hex2byte(ipos);
		if (a < 0)
			return -1;
		*opos++ = a;
		ipos += 2;
	}
	return 0;
}


char * rel2abs_path(const char *rel_path)
{
	char *buf = NULL, *cwd, *ret;
	size_t len = 128, cwd_len, rel_len, ret_len;

	if (rel_path[0] == '/')
		return strdup(rel_path);

	for (;;) {
		buf = malloc(len);
		if (buf == NULL)
			return NULL;
		cwd = getcwd(buf, len);
		if (cwd == NULL) {
			free(buf);
			if (errno != ERANGE) {
				return NULL;
			}
			len *= 2;
		} else {
			break;
		}
	}

	cwd_len = strlen(cwd);
	rel_len = strlen(rel_path);
	ret_len = cwd_len + 1 + rel_len + 1;
	ret = malloc(ret_len);
	if (ret) {
		memcpy(ret, cwd, cwd_len);
		ret[cwd_len] = '/';
		memcpy(ret + cwd_len + 1, rel_path, rel_len);
		ret[ret_len - 1] = '\0';
	}
	free(buf);
	return ret;
}


void inc_byte_array(u8 *counter, size_t len)
{
	int pos = len - 1;
	while (pos >= 0) {
		counter[pos]++;
		if (counter[pos] != 0)
			break;
		pos--;
	}
}


void print_char(char c)
{
	if (c >= 32 && c < 127)
		printf("%c", c);
	else
		printf("<%02x>", c);
}


void fprint_char(FILE *f, char c)
{
	if (c >= 32 && c < 127)
		fprintf(f, "%c", c);
	else
		fprintf(f, "<%02x>", c);
}


static void wpa_debug_print_timestamp(void)
{
	struct timeval tv;
	char buf[16];

	if (!wpa_debug_timestamp)
		return;

	gettimeofday(&tv, NULL);
	if (strftime(buf, sizeof(buf), "%b %d %H:%M:%S",
		     localtime((const time_t *) &tv.tv_sec)) <= 0) {
		snprintf(buf, sizeof(buf), "%u", (int) tv.tv_sec);
	}
	printf("%s.%06u: ", buf, (unsigned int) tv.tv_usec);
}


void wpa_printf(int level, char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (level >= wpa_debug_level) {
		wpa_debug_print_timestamp();
		vprintf(fmt, ap);
		printf("\n");
	}
	va_end(ap);
}


static void _wpa_hexdump(int level, const char *title, const u8 *buf,
			 size_t len, int show)
{
	size_t i;
	if (level < wpa_debug_level)
		return;
	wpa_debug_print_timestamp();
	printf("%s - hexdump(len=%lu):", title, (unsigned long) len);
	if (show) {
		for (i = 0; i < len; i++)
			printf(" %02x", buf[i]);
	} else {
		printf(" [REMOVED]");
	}
	printf("\n");
}

void wpa_hexdump(int level, const char *title, const u8 *buf, size_t len)
{
	_wpa_hexdump(level, title, buf, len, 1);
}


void wpa_hexdump_key(int level, const char *title, const u8 *buf, size_t len)
{
	_wpa_hexdump(level, title, buf, len, wpa_debug_show_keys);
}


static void _wpa_hexdump_ascii(int level, const char *title, const u8 *buf,
			       size_t len, int show)
{
	int i, llen;
	const u8 *pos = buf;
	const int line_len = 16;

	if (level < wpa_debug_level)
		return;
	wpa_debug_print_timestamp();
	if (!show) {
		printf("%s - hexdump_ascii(len=%lu): [REMOVED]\n",
		       title, (unsigned long) len);
		return;
	}
	printf("%s - hexdump_ascii(len=%lu):\n", title, (unsigned long) len);
	while (len) {
		llen = len > line_len ? line_len : len;
		printf("    ");
		for (i = 0; i < llen; i++)
			printf(" %02x", pos[i]);
		for (i = llen; i < line_len; i++)
			printf("   ");
		printf("   ");
		for (i = 0; i < llen; i++) {
			if (isprint(pos[i]))
				printf("%c", pos[i]);
			else
				printf("_");
		}
		for (i = llen; i < line_len; i++)
			printf(" ");
		printf("\n");
		pos += llen;
		len -= llen;
	}
}


void wpa_hexdump_ascii(int level, const char *title, const u8 *buf, size_t len)
{
	_wpa_hexdump_ascii(level, title, buf, len, 1);
}


void wpa_hexdump_ascii_key(int level, const char *title, const u8 *buf,
			   size_t len)
{
	_wpa_hexdump_ascii(level, title, buf, len, wpa_debug_show_keys);
}


#ifdef CONFIG_NATIVE_WINDOWS

#define EPOCHFILETIME (116444736000000000ULL)

int gettimeofday(struct timeval *tv, struct timezone *tz)
{
	FILETIME ft;
	LARGE_INTEGER li;
	ULONGLONG t;

	GetSystemTimeAsFileTime(&ft);
	li.LowPart = ft.dwLowDateTime;
	li.HighPart = ft.dwHighDateTime;
	t = (li.QuadPart - EPOCHFILETIME) / 10;
	tv->tv_sec = (long) (t / 1000000);
	tv->tv_usec = (long) (t % 1000000);

	return 0;
}
#endif /* CONFIG_NATIVE_WINDOWS */
