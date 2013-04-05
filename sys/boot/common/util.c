/*-
 * Copyright (c) 1998 Robert Nordier
 * Copyright (c) 2010 Pawel Jakub Dawidek <pjd@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are freely
 * permitted provided that the above copyright notice and this
 * paragraph and the following disclaimer are duplicated in all
 * such forms.
 *
 * This software is provided "AS IS" and without any express or
 * implied warranties, including, without limitation, the implied
 * warranties of merchantability and fitness for a particular
 * purpose.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>

#include <stdarg.h>

#include "cons.h"
#include "util.h"

void
memcpy(void *dst, const void *src, int len)
{
	const char *s = src;
	char *d = dst;

	while (len--)
		*d++ = *s++;
}

void
memset(void *b, int c, size_t len)
{
	char *bp = b;

	while (len--)
		*bp++ = (unsigned char)c;
}

int
memcmp(const void *b1, const void *b2, size_t len)
{
	const unsigned char *p1, *p2;

	for (p1 = b1, p2 = b2; len > 0; len--, p1++, p2++) {
		if (*p1 != *p2)
			return ((*p1) - (*p2));
	}
	return (0);
}

int
strcmp(const char *s1, const char *s2)
{

	for (; *s1 == *s2 && *s1 != '\0'; s1++, s2++)
		;
	return ((unsigned char)*s1 - (unsigned char)*s2);
}

int
strncmp(const char *s1, const char *s2, size_t len)
{

	for (; len > 0 && *s1 == *s2 && *s1 != '\0'; len--, s1++, s2++)
		;
	return (len == 0 ? 0 : (unsigned char)*s1 - (unsigned char)*s2);
}

void
strcpy(char *dst, const char *src)
{

	while (*src != '\0')
		*dst++ = *src++;
	*dst = '\0';
}

void
strcat(char *dst, const char *src)
{

	while (*dst != '\0')
		dst++;
	while (*src != '\0')
		*dst++ = *src++;
	*dst = '\0';
}

char *
strchr(const char *s, char ch)
{

	for (; *s != '\0'; s++) {
		if (*s == ch)
			return ((char *)(uintptr_t)(const void *)s);
	}
	return (NULL);
}

size_t
strlen(const char *s)
{
	size_t len = 0;

	while (*s++ != '\0')
		len++;
	return (len);
}

void
printf(const char *fmt, ...)
{
	va_list ap;
	const char *hex = "0123456789abcdef";
	char buf[32], *s;
	unsigned long long u;
	int c, l;

	va_start(ap, fmt);
	while ((c = *fmt++) != '\0') {
		if (c != '%') {
			putchar(c);
			continue;
		}
		l = 0;
nextfmt:
		c = *fmt++;
		switch (c) {
		case 'l':
			l++;
			goto nextfmt;
		case 'c':
			putchar(va_arg(ap, int));
			break;
		case 's':
			for (s = va_arg(ap, char *); *s != '\0'; s++)
				putchar(*s);
			break;
		case 'd':	/* A lie, always prints unsigned */
		case 'u':
		case 'x':
			switch (l) {
			case 2:
				u = va_arg(ap, unsigned long long);
				break;
			case 1:
				u = va_arg(ap, unsigned long);
				break;
			default:
				u = va_arg(ap, unsigned int);
				break;
			}
			s = buf;
			if (c == 'd' || c == 'u') {
				do
					*s++ = '0' + (u % 10U);
				while (u /= 10);
			} else {
				do
					*s++ = hex[u & 0xfu];
				while (u >>= 4);
			}
			while (--s >= buf)
				putchar(*s);
			break;
		}
	}
	va_end(ap);
}
