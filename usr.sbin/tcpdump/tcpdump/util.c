/*
 * Copyright (c) 1990, 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
static char rcsid[] =
    "@(#) $Header: /home/ncvs/src/usr.sbin/tcpdump/tcpdump/util.c,v 1.3 1995/09/28 15:28:40 wollman Exp $ (LBL)";
#endif

#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/stat.h>

#include <ctype.h>
#ifdef SOLARIS
#include <fcntl.h>
#endif
#ifdef __STDC__
#include <stdlib.h>
#endif
#include <stdio.h>
#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include <string.h>
#include <unistd.h>

#include "interface.h"

/*
 * Print out a filename (or other ascii string).
 * If ep is NULL, assume no truncation check is needed.
 * Return true if truncated.
 */
int
fn_print(register const u_char *s, register const u_char *ep)
{
	register int ret;
	register u_char c;

	ret = 1;			/* assume truncated */
	putchar('"');
	while (ep == NULL || s < ep) {
		c = *s++;
		if (c == '\0') {
			ret = 0;
			break;
		}
		if (!isascii(c)) {
			c = toascii(c);
			putchar('M');
			putchar('-');
		}
		if (!isprint(c)) {
			c ^= 0x40;	/* DEL to ?, others to alpha */
			putchar('^');
		}
		putchar(c);
	}
	putchar('"');
	return(ret);
}

/*
 * Print out a counted filename (or other ascii string).
 * If ep is NULL, assume no truncation check is needed.
 * Return true if truncated.
 */
int
fn_printn(register const u_char *s, register u_int n,
	  register const u_char *ep)
{
	register int ret;
	register u_char c;

	ret = 1;			/* assume truncated */
	putchar('"');
	while (ep == NULL || s < ep) {
		if (n-- <= 0) {
			ret = 0;
			break;
		}
		c = *s++;
		if (!isascii(c)) {
			c = toascii(c);
			putchar('M');
			putchar('-');
		}
		if (!isprint(c)) {
			c ^= 0x40;	/* DEL to ?, others to alpha */
			putchar('^');
		}
		putchar(c);
	}
	putchar('"');
	return(ret);
}

/*
 * Print the timestamp
 */
void
ts_print(register const struct timeval *tvp)
{
	register int s;
	extern int32 thiszone;

	if (tflag > 0) {
		/* Default */
		s = (tvp->tv_sec + thiszone) % 86400;
		(void)printf("%02d:%02d:%02d.%06d ",
		    s / 3600, (s % 3600) / 60, s % 60, tvp->tv_usec);
	} else if (tflag < 0) {
		/* Unix timeval style */
		(void)printf("%d.%06d ", tvp->tv_sec, tvp->tv_usec);
	}
}

/*
 * Convert a token value to a string; use "fmt" if not found.
 */
const char *
tok2str(register const struct token *lp, register const char *fmt,
	register int v)
{
	static char buf[128];

	while (lp->s != NULL) {
		if (lp->v == v)
			return (lp->s);
		++lp;
	}
	if (fmt == NULL)
		fmt = "#%d";
	(void)sprintf(buf, fmt, v);
	return (buf);
}

/* A replacement for strdup() that cuts down on malloc() overhead */
char *
savestr(register const char *str)
{
	register u_int size;
	register char *p;
	static char *strptr = NULL;
	static u_int strsize = 0;

	size = strlen(str) + 1;
	if (size > strsize) {
		strsize = 1024;
		if (strsize < size)
			strsize = size;
		strptr = malloc(strsize);
		if (strptr == NULL)
			error("savestr: malloc");
	}
	(void)strcpy(strptr, str);
	p = strptr;
	strptr += size;
	strsize -= size;
	return (p);
}

#ifdef NOVFPRINTF
/*
 * Stock 4.3 doesn't have vfprintf.
 * This routine is due to Chris Torek.
 */
vfprintf(f, fmt, args)
	FILE *f;
	char *fmt;
	va_list args;
{
	int ret;

	if ((f->_flag & _IOWRT) == 0) {
		if (f->_flag & _IORW)
			f->_flag |= _IOWRT;
		else
			return EOF;
	}
	ret = _doprnt(fmt, args, f);
	return ferror(f) ? EOF : ret;
}
#endif

/* VARARGS */
__dead void
#if __STDC__ || defined(SOLARIS)
error(char *fmt, ...)
#else
error(fmt, va_alist)
	char *fmt;
	va_dcl
#endif
{
	va_list ap;

	(void)fprintf(stderr, "%s: ", program_name);
#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	if (*fmt) {
		fmt += strlen(fmt);
		if (fmt[-1] != '\n')
			(void)fputc('\n', stderr);
	}
	exit(1);
	/* NOTREACHED */
}

/* VARARGS */
void
#if __STDC__ || defined(SOLARIS)
warning(char *fmt, ...)
#else
warning(fmt, va_alist)
	char *fmt;
	va_dcl
#endif
{
	va_list ap;

	(void)fprintf(stderr, "%s: warning: ", program_name);
#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	if (*fmt) {
		fmt += strlen(fmt);
		if (fmt[-1] != '\n')
			(void)fputc('\n', stderr);
	}
}

/*
 * Copy arg vector into a new buffer, concatenating arguments with spaces.
 */
char *
copy_argv(register char **argv)
{
	register char **p;
	register int len = 0;
	char *buf;
	char *src, *dst;

	p = argv;
	if (*p == 0)
		return 0;

	while (*p)
		len += strlen(*p++) + 1;

	buf = malloc(len);

	p = argv;
	dst = buf;
	while ((src = *p++) != NULL) {
		while ((*dst++ = *src++) != '\0')
			;
		dst[-1] = ' ';
	}
	dst[-1] = '\0';

	return buf;
}

char *
read_infile(char *fname)
{
	struct stat buf;
	int fd;
	char *p;

	fd = open(fname, O_RDONLY);
	if (fd < 0)
		error("can't open '%s'", fname);

	if (fstat(fd, &buf) < 0)
		error("can't state '%s'", fname);

	p = malloc((u_int)buf.st_size);
	if (read(fd, p, (int)buf.st_size) != buf.st_size)
		error("problem reading '%s'", fname);

	return p;
}

int
gmt2local()
{
	time_t now;
	struct tm *tmp;

	time(&now);
	tmp = localtime(&now);
	return tmp->tm_gmtoff;
}
