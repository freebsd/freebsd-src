/*
 * Copyright (c) 1988-1990 The Regents of the University of California.
 * All rights reserved.
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
    "@(#) $Header: util.c,v 1.12 91/10/28 22:09:31 mccanne Exp $ (LBL)";
#endif

#include <stdio.h>
#ifdef __STDC__
#include <stdlib.h>
#endif
#include <sys/types.h>
#include <sys/time.h>
#include <ctype.h>
#include <varargs.h>
#include <sys/file.h>
#include <sys/stat.h>

#include "interface.h"

/* Hex digit to integer. */
static inline int
xdtoi(c)
{
	if (isdigit(c))
		return c - '0';
	else if (islower(c))
		return c - 'a' + 10;
	else
		return c - 'A' + 10;
}

/*
 * Convert string to integer.  Just like atoi(), but checks for 
 * preceding 0x or 0 and uses hex or octal instead of decimal.
 */
int
stoi(s)
	char *s;
{
	int base = 10;
	int n = 0;

	if (*s == '0') {
		if (s[1] == 'x' || s[1] == 'X') {
			s += 2;
			base = 16;
		}
		else {
			base = 8;
			s += 1;
		}
	}
	while (*s)
		n = n * base + xdtoi(*s++);

	return n;
}

/*
 * Print out a filename (or other ascii string).
 * Return true if truncated.
 */
int
printfn(s, ep)
	register u_char *s, *ep;
{
	register u_char c;

	putchar('"');
	while (c = *s++) {
		if (s > ep) {
			putchar('"');
			return(1);
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
	return(0);
}

/*
 * Print the timestamp
 */
void
ts_print(tvp)
	register struct timeval *tvp;
{
	register int i;

	if (tflag > 0) {
		/* Default */
		i = (tvp->tv_sec + thiszone) % 86400;
		(void)printf("%02d:%02d:%02d.%06d ",
		    i / 3600, (i % 3600) / 60, i % 60, tvp->tv_usec);
	} else if (tflag < 0) {
		/* Unix timeval style */
		(void)printf("%d.%06d ", tvp->tv_sec, tvp->tv_usec);
	}
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

static char *
stripdir(s)
	register char *s;
{
	register char *cp;
	char *rindex();

	cp = rindex(s, '/');
	return (cp != 0) ? cp + 1 : s;
}

/* VARARGS */
void
error(va_alist)
	va_dcl
{
	register char *cp;
	va_list ap;

	(void)fprintf(stderr, "%s: ", stripdir(program_name));

	va_start(ap);
	cp = va_arg(ap, char *);
	(void)vfprintf(stderr, cp, ap);
	va_end(ap);
	if (*cp) {
		cp += strlen(cp);
		if (cp[-1] != '\n')
			(void)fputc('\n', stderr);
	}
	exit(1);
	/* NOTREACHED */
}

/* VARARGS */
void
warning(va_alist)
	va_dcl
{
	register char *cp;
	va_list ap;

	(void)fprintf(stderr, "%s: warning: ", stripdir(program_name));

	va_start(ap);
	cp = va_arg(ap, char *);
	(void)vfprintf(stderr, cp, ap);
	va_end(ap);
	if (*cp) {
		cp += strlen(cp);
		if (cp[-1] != '\n')
			(void)fputc('\n', stderr);
	}
}


/*
 * Copy arg vector into a new buffer, concatenating arguments with spaces.
 */
char *
copy_argv(argv)
	register char **argv;
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
	while (src = *p++) {
		while (*dst++ = *src++)
			;
		dst[-1] = ' ';
	}
	dst[-1] = '\0';

	return buf;
}

char *
read_infile(fname)
	char *fname;
{
	struct stat buf;
	int fd;
	char *p;

	fd = open(fname, O_RDONLY);
	if (fd < 0)
		error("can't open '%s'", fname);

	if (fstat(fd, &buf) < 0)
		error("can't state '%s'", fname);

	p = malloc((unsigned)buf.st_size);
	if (read(fd, p, (int)buf.st_size) != buf.st_size)
		error("problem reading '%s'", fname);
	
	return p;
}

/*
 * Left justify 'addr' and return its resulting network mask.
 */
u_long
net_mask(addr)
	u_long *addr;
{
	register u_long m = 0xffffffff;

	if (*addr)
		while ((*addr & 0xff000000) == 0)
			*addr <<= 8, m <<= 8;

	return m;
}
