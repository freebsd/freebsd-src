/*-
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
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
static const char copyright[] =
"@(#) Copyright (c) 1988, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)kdump.c	8.1 (Berkeley) 6/6/93";
#endif
#endif /* not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#define _KERNEL
extern int errno;
#include <sys/errno.h>
#undef _KERNEL
#include <sys/param.h>
#include <sys/errno.h>
#define _KERNEL
#include <sys/time.h>
#undef _KERNEL
#include <sys/uio.h>
#include <sys/ktrace.h>
#include <sys/ioctl.h>
#include <sys/ptrace.h>
#include <err.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vis.h>
#include "ktrace.h"

int fread_tail(void *, int, int);
void dumpheader(struct ktr_header *);
void ktrsyscall(struct ktr_syscall *);
void ktrsysret(struct ktr_sysret *);
void ktrnamei(char *, int);
void hexdump(char *, int, int);
void visdump(char *, int, int);
void ktrgenio(struct ktr_genio *, int);
void ktrpsig(struct ktr_psig *);
void ktrcsw(struct ktr_csw *);
void ktruser(int, unsigned char *);
void usage(void);
const char *ioctlname(u_long);

int timestamp, decimal, fancy = 1, suppressdata, tail, maxdata;
const char *tracefile = DEF_TRACEFILE;
struct ktr_header ktr_header;

#define eqs(s1, s2)	(strcmp((s1), (s2)) == 0)

int
main(int argc, char *argv[])
{
	int ch, ktrlen, size;
	void *m;
	int trpoints = ALL_POINTS;
	int drop_logged;
	pid_t pid = 0;

	(void) setlocale(LC_CTYPE, "");

	while ((ch = getopt(argc,argv,"f:dElm:np:RsTt:")) != -1)
		switch((char)ch) {
		case 'f':
			tracefile = optarg;
			break;
		case 'd':
			decimal = 1;
			break;
		case 'l':
			tail = 1;
			break;
		case 'm':
			maxdata = atoi(optarg);
			break;
		case 'n':
			fancy = 0;
			break;
		case 'p':
			pid = atoi(optarg);
			break;
		case 's':
			suppressdata = 1;
			break;
		case 'E':
			timestamp = 3;	/* elapsed timestamp */
			break;
		case 'R':
			timestamp = 2;	/* relative timestamp */
			break;
		case 'T':
			timestamp = 1;
			break;
		case 't':
			trpoints = getpoints(optarg);
			if (trpoints < 0)
				errx(1, "unknown trace point in %s", optarg);
			break;
		default:
			usage();
		}

	if (argc > optind)
		usage();

	m = (void *)malloc(size = 1025);
	if (m == NULL)
		errx(1, "%s", strerror(ENOMEM));
	if (!freopen(tracefile, "r", stdin))
		err(1, "%s", tracefile);
	drop_logged = 0;
	while (fread_tail(&ktr_header, sizeof(struct ktr_header), 1)) {
		if (ktr_header.ktr_type & KTR_DROP) {
			ktr_header.ktr_type &= ~KTR_DROP;
			if (!drop_logged) {
				(void)printf("%6d %-8.*s Events dropped.\n",
				    ktr_header.ktr_pid, MAXCOMLEN,
				    ktr_header.ktr_comm);
				drop_logged = 1;
			}
		}
		if (trpoints & (1<<ktr_header.ktr_type))
			if (pid == 0 || ktr_header.ktr_pid == pid)
				dumpheader(&ktr_header);
		if ((ktrlen = ktr_header.ktr_len) < 0)
			errx(1, "bogus length 0x%x", ktrlen);
		if (ktrlen > size) {
			m = (void *)realloc(m, ktrlen+1);
			if (m == NULL)
				errx(1, "%s", strerror(ENOMEM));
			size = ktrlen;
		}
		if (ktrlen && fread_tail(m, ktrlen, 1) == 0)
			errx(1, "data too short");
		if (pid && ktr_header.ktr_pid != pid)
			continue;
		if ((trpoints & (1<<ktr_header.ktr_type)) == 0)
			continue;
		drop_logged = 0;
		switch (ktr_header.ktr_type) {
		case KTR_SYSCALL:
			ktrsyscall((struct ktr_syscall *)m);
			break;
		case KTR_SYSRET:
			ktrsysret((struct ktr_sysret *)m);
			break;
		case KTR_NAMEI:
			ktrnamei(m, ktrlen);
			break;
		case KTR_GENIO:
			ktrgenio((struct ktr_genio *)m, ktrlen);
			break;
		case KTR_PSIG:
			ktrpsig((struct ktr_psig *)m);
			break;
		case KTR_CSW:
			ktrcsw((struct ktr_csw *)m);
			break;
		case KTR_USER:
			ktruser(ktrlen, m);
			break;
		default:
			printf("\n");
			break;
		}
		if (tail)
			(void)fflush(stdout);
	}
	return 0;
}

int
fread_tail(void *buf, int size, int num)
{
	int i;

	while ((i = fread(buf, size, num, stdin)) == 0 && tail) {
		(void)sleep(1);
		clearerr(stdin);
	}
	return (i);
}

void
dumpheader(struct ktr_header *kth)
{
	static char unknown[64];
	static struct timeval prevtime, temp;
	const char *type;

	switch (kth->ktr_type) {
	case KTR_SYSCALL:
		type = "CALL";
		break;
	case KTR_SYSRET:
		type = "RET ";
		break;
	case KTR_NAMEI:
		type = "NAMI";
		break;
	case KTR_GENIO:
		type = "GIO ";
		break;
	case KTR_PSIG:
		type = "PSIG";
		break;
	case KTR_CSW:
		type = "CSW";
		break;
	case KTR_USER:
		type = "USER";
		break;
	default:
		(void)sprintf(unknown, "UNKNOWN(%d)", kth->ktr_type);
		type = unknown;
	}

	(void)printf("%6d %-8.*s ", kth->ktr_pid, MAXCOMLEN, kth->ktr_comm);
	if (timestamp) {
		if (timestamp == 3) {
			if (prevtime.tv_sec == 0)
				prevtime = kth->ktr_time;
			timevalsub(&kth->ktr_time, &prevtime);
		}
		if (timestamp == 2) {
			temp = kth->ktr_time;
			timevalsub(&kth->ktr_time, &prevtime);
			prevtime = temp;
		}
		(void)printf("%ld.%06ld ",
		    kth->ktr_time.tv_sec, kth->ktr_time.tv_usec);
	}
	(void)printf("%s  ", type);
}

#include <sys/syscall.h>
#define KTRACE
#include <sys/kern/syscalls.c>
#undef KTRACE
int nsyscalls = sizeof (syscallnames) / sizeof (syscallnames[0]);

static const char *ptrace_ops[] = {
	"PT_TRACE_ME",	"PT_READ_I",	"PT_READ_D",	"PT_READ_U",
	"PT_WRITE_I",	"PT_WRITE_D",	"PT_WRITE_U",	"PT_CONTINUE",
	"PT_KILL",	"PT_STEP",	"PT_ATTACH",	"PT_DETACH",
};

void
ktrsyscall(struct ktr_syscall *ktr)
{
	int narg = ktr->ktr_narg;
	register_t *ip;

	if (ktr->ktr_code >= nsyscalls || ktr->ktr_code < 0)
		(void)printf("[%d]", ktr->ktr_code);
	else
		(void)printf("%s", syscallnames[ktr->ktr_code]);
	ip = &ktr->ktr_args[0];
	if (narg) {
		char c = '(';
		if (fancy) {
			if (ktr->ktr_code == SYS_ioctl) {
				const char *cp;
				if (decimal)
					(void)printf("(%ld", (long)*ip);
				else
					(void)printf("(%#lx", (long)*ip);
				ip++;
				narg--;
				if ((cp = ioctlname(*ip)) != NULL)
					(void)printf(",%s", cp);
				else {
					if (decimal)
						(void)printf(",%ld", (long)*ip);
					else
						(void)printf(",%#lx ", (long)*ip);
				}
				c = ',';
				ip++;
				narg--;
			} else if (ktr->ktr_code == SYS_ptrace) {
				if ((size_t)*ip < sizeof(ptrace_ops) /
				    sizeof(ptrace_ops[0]) && *ip >= 0)
					(void)printf("(%s", ptrace_ops[*ip]);
#ifdef PT_GETREGS
				else if (*ip == PT_GETREGS)
					(void)printf("(%s", "PT_GETREGS");
#endif
#ifdef PT_SETREGS
				else if (*ip == PT_SETREGS)
					(void)printf("(%s", "PT_SETREGS");
#endif
#ifdef PT_GETFPREGS
				else if (*ip == PT_GETFPREGS)
					(void)printf("(%s", "PT_GETFPREGS");
#endif
#ifdef PT_SETFPREGS
				else if (*ip == PT_SETFPREGS)
					(void)printf("(%s", "PT_SETFPREGS");
#endif
#ifdef PT_GETDBREGS
				else if (*ip == PT_GETDBREGS)
					(void)printf("(%s", "PT_GETDBREGS");
#endif
#ifdef PT_SETDBREGS
				else if (*ip == PT_SETDBREGS)
					(void)printf("(%s", "PT_SETDBREGS");
#endif
				else
					(void)printf("(%ld", (long)*ip);
				c = ',';
				ip++;
				narg--;
			}
		}
		while (narg) {
			if (decimal)
				(void)printf("%c%ld", c, (long)*ip);
			else
				(void)printf("%c%#lx", c, (long)*ip);
			c = ',';
			ip++;
			narg--;
		}
		(void)putchar(')');
	}
	(void)putchar('\n');
}

void
ktrsysret(struct ktr_sysret *ktr)
{
	register_t ret = ktr->ktr_retval;
	int error = ktr->ktr_error;
	int code = ktr->ktr_code;

	if (code >= nsyscalls || code < 0)
		(void)printf("[%d] ", code);
	else
		(void)printf("%s ", syscallnames[code]);

	if (error == 0) {
		if (fancy) {
			(void)printf("%d", ret);
			if (ret < 0 || ret > 9)
				(void)printf("/%#lx", (long)ret);
		} else {
			if (decimal)
				(void)printf("%ld", (long)ret);
			else
				(void)printf("%#lx", (long)ret);
		}
	} else if (error == ERESTART)
		(void)printf("RESTART");
	else if (error == EJUSTRETURN)
		(void)printf("JUSTRETURN");
	else {
		(void)printf("-1 errno %d", ktr->ktr_error);
		if (fancy)
			(void)printf(" %s", strerror(ktr->ktr_error));
	}
	(void)putchar('\n');
}

void
ktrnamei(char *cp, int len)
{
	(void)printf("\"%.*s\"\n", len, cp);
}

void
hexdump(char *p, int len, int screenwidth)
{
	int n, i;
	int width;

	width = 0;
	do {
		width += 2;
		i = 13;			/* base offset */
		i += (width / 2) + 1;	/* spaces every second byte */
		i += (width * 2);	/* width of bytes */
		i += 3;			/* "  |" */
		i += width;		/* each byte */
		i += 1;			/* "|" */
	} while (i < screenwidth);
	width -= 2;

	for (n = 0; n < len; n += width) {
		for (i = n; i < n + width; i++) {
			if ((i % width) == 0) {	/* beginning of line */
				printf("       0x%04x", i);
			}
			if ((i % 2) == 0) {
				printf(" ");
			}
			if (i < len)
				printf("%02x", p[i] & 0xff);
			else
				printf("  ");
		}
		printf("  |");
		for (i = n; i < n + width; i++) {
			if (i >= len)
				break;
			if (p[i] >= ' ' && p[i] <= '~')
				printf("%c", p[i]);
			else
				printf(".");
		}
		printf("|\n");
	}
	if ((i % width) != 0)
		printf("\n");
}

void
visdump(char *dp, int datalen, int screenwidth)
{
	int col = 0;
	char *cp;
	int width;
	char visbuf[5];

	(void)printf("       \"");
	col = 8;
	for (;datalen > 0; datalen--, dp++) {
		(void) vis(visbuf, *dp, VIS_CSTYLE, *(dp+1));
		cp = visbuf;
		/*
		 * Keep track of printables and
		 * space chars (like fold(1)).
		 */
		if (col == 0) {
			(void)putchar('\t');
			col = 8;
		}
		switch(*cp) {
		case '\n':
			col = 0;
			(void)putchar('\n');
			continue;
		case '\t':
			width = 8 - (col&07);
			break;
		default:
			width = strlen(cp);
		}
		if (col + width > (screenwidth-2)) {
			(void)printf("\\\n\t");
			col = 8;
		}
		col += width;
		do {
			(void)putchar(*cp++);
		} while (*cp);
	}
	if (col == 0)
		(void)printf("       ");
	(void)printf("\"\n");
}

void
ktrgenio(struct ktr_genio *ktr, int len)
{
	int datalen = len - sizeof (struct ktr_genio);
	char *dp = (char *)ktr + sizeof (struct ktr_genio);
	static int screenwidth = 0;
	int i, binary;

	if (screenwidth == 0) {
		struct winsize ws;

		if (fancy && ioctl(fileno(stderr), TIOCGWINSZ, &ws) != -1 &&
		    ws.ws_col > 8)
			screenwidth = ws.ws_col;
		else
			screenwidth = 80;
	}
	printf("fd %d %s %d byte%s\n", ktr->ktr_fd,
		ktr->ktr_rw == UIO_READ ? "read" : "wrote", datalen,
		datalen == 1 ? "" : "s");
	if (suppressdata)
		return;
	if (maxdata && datalen > maxdata)
		datalen = maxdata;

	for (i = 0, binary = 0; i < datalen && binary == 0; i++)  {
		if (dp[i] >= 32 && dp[i] < 127)
			continue;
		if (dp[i] == 10 || dp[i] == 13 || dp[i] == 0 || dp[i] == 9)
			continue;
		binary = 1;
	}
	if (binary)
		hexdump(dp, datalen, screenwidth);
	else
		visdump(dp, datalen, screenwidth);
}

const char *signames[] = {
	"NULL", "HUP", "INT", "QUIT", "ILL", "TRAP", "IOT",	/*  1 - 6  */
	"EMT", "FPE", "KILL", "BUS", "SEGV", "SYS",		/*  7 - 12 */
	"PIPE", "ALRM",  "TERM", "URG", "STOP", "TSTP",		/* 13 - 18 */
	"CONT", "CHLD", "TTIN", "TTOU", "IO", "XCPU",		/* 19 - 24 */
	"XFSZ", "VTALRM", "PROF", "WINCH", "29", "USR1",	/* 25 - 30 */
	"USR2", NULL,						/* 31 - 32 */
};

void
ktrpsig(struct ktr_psig *psig)
{
	(void)printf("SIG%s ", signames[psig->signo]);
	if (psig->action == SIG_DFL)
		(void)printf("SIG_DFL\n");
	else {
		(void)printf("caught handler=0x%lx mask=0x%x code=0x%x\n",
		    (u_long)psig->action, psig->mask.__bits[0], psig->code);
	}
}

void
ktrcsw(struct ktr_csw *cs)
{
	(void)printf("%s %s\n", cs->out ? "stop" : "resume",
		cs->user ? "user" : "kernel");
}

void
ktruser(int len, unsigned char *p)
{
	(void)printf("%d ", len);
	while (len--)
		if (decimal)
			(void)printf(" %d", *p++);
		else
			(void)printf(" %02x", *p++);
	(void)printf("\n");
		
}

void
usage(void)
{
	(void)fprintf(stderr,
   "usage: kdump [-dEnlRsT] [-f trfile] [-m maxdata] [-p pid] [-t [cnisuw]]\n");
	exit(1);
}
