/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1991 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)dmesg.c	5.9 (Berkeley) 5/2/91";
#endif /* not lint */

#include <sys/cdefs.h>
#include <sys/msgbuf.h>
#include <time.h>
#include <nlist.h>
#include <kvm.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

struct nlist nl[] = {
#define	X_MSGBUFP	0
	{ "_msgbufp" },
	{ NULL },
};

void usage(), vputc();
void err __P((const char *, ...));

main(argc, argv)
	int argc;
	char **argv;
{
	register int ch, newl, skip;
	register char *p, *ep;
	struct msgbuf cur;
	int msgbufat;
	char *core, *namelist;

	core = namelist = NULL;
	while ((ch = getopt(argc, argv, "M:N:")) != EOF)
		switch(ch) {
		case 'M':
			core = optarg;
			break;
		case 'N':
			namelist = optarg;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	/* Read in kernel message buffer, do sanity checks. */
	if (kvm_openfiles(namelist, core, NULL) == -1)
		err("kvm_openfiles: %s", kvm_geterr());
	if (kvm_nlist(nl) == -1)
		err("kvm_nlist: %s", kvm_geterr());
	if (nl[X_MSGBUFP].n_type == 0)
		err("msgbufp not found namelist");

        kvm_read((void *)nl[X_MSGBUFP].n_value, (void *)&msgbufat, sizeof(msgbufat));
        kvm_read((void *)msgbufat, (void *)&cur, sizeof(cur));
	if (cur.msg_magic != MSG_MAGIC)
		err("magic number incorrect");
	if (cur.msg_bufx >= MSG_BSIZE)
		cur.msg_bufx = 0;

	/*
	 * The message buffer is circular; start at the read pointer, and
	 * go to the write pointer - 1.
	 */
	p = cur.msg_bufc + cur.msg_bufx;
	ep = cur.msg_bufc + cur.msg_bufx - 1;
	for (newl = skip = 0; p != ep; ++p) {
		if (p == cur.msg_bufc + MSG_BSIZE)
			p = cur.msg_bufc;
		ch = *p;
		/* Skip "\n<.*>" syslog sequences. */
		if (skip) {
			if (ch == '>')
				newl = skip = 0;
			continue;
		}
		if (newl && ch == '<') {
			skip = 1;
			continue;
		}
		if (ch == '\0')
			continue;
		newl = (ch = *p) == '\n';
		vputc(ch);
	}
	if (!newl)
		(void)putchar('\n');
	exit(0);
}

void
vputc(ch)
	register int ch;
{
	int meta;

	if (!isascii(ch)) {
		(void)putchar('M');
		(void)putchar('-');
		ch = toascii(ch);
		meta = 1;
	} else
		meta = 0;
	if (isprint(ch) || !meta && (ch == ' ' || ch == '\t' || ch == '\n'))
		(void)putchar(ch);
	else {
		(void)putchar('^');
		(void)putchar(ch == '\177' ? '?' : ch | 0100);
	}
}

#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

void
#if __STDC__
err(const char *fmt, ...)
#else
err(fmt, va_alist)
	char *fmt;
        va_dcl
#endif
{
	va_list ap;
#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	(void)fprintf(stderr, "dmesg: ");
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void)fprintf(stderr, "\n");
	exit(1);
	/* NOTREACHED */
}

void
usage()
{
	(void)fprintf(stderr, "usage: dmesg [-M core] [-N system]\n");
	exit(1);
}
