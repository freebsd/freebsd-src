/*-
 * Copyright (c) 2002 Juli Mallett.
 * Copyright (c) 1993
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

#include <sys/cdefs.h>

__FBSDID("$FreeBSD$");

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif

#ifndef lint
static const char sccsid[] = "@(#)uname.c	8.2 (Berkeley) 5/4/95";
#endif

#include <sys/param.h>
#include <sys/sysctl.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define	MFLAG	0x01
#define	NFLAG	0x02
#define	PFLAG	0x04
#define	RFLAG	0x08
#define	SFLAG	0x10
#define	VFLAG	0x20

typedef void (*get_t)(void);
get_t get_platform, get_hostname, get_arch, get_release, get_sysname, get_version;

void native_platform(void);
void native_hostname(void);
void native_arch(void);
void native_release(void);
void native_sysname(void);
void native_version(void);
void print_uname(u_int);
void setup_get(void);
void usage(void);

char *platform, *hostname, *arch, *release, *sysname, *version;
const char *prefix;

int
main(int argc, char *argv[])
{
	u_int flags;
	int ch;

	prefix = "";

	setup_get();

	flags = 0;
	while ((ch = getopt(argc, argv, "amnprsv")) != -1)
		switch(ch) {
		case 'a':
			flags |= (MFLAG | NFLAG | RFLAG | SFLAG | VFLAG);
			break;
		case 'm':
			flags |= MFLAG;
			break;
		case 'n':
			flags |= NFLAG;
			break;
		case 'p':
			flags |= PFLAG;
			break;
		case 'r':
			flags |= RFLAG;
			break;
		case 's':
			flags |= SFLAG;
			break;
		case 'v':
			flags |= VFLAG;
			break;
		case '?':
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	if (argc)
		usage();

	if (!flags)
		flags |= SFLAG;

	print_uname(flags);
	exit(0);
}

#define	CHECK_ENV(opt,var)			\
do {						\
	if ((var = getenv("UNAME_" opt)) == NULL) {	\
		get_##var = native_##var;	\
	} else {				\
		get_##var = (get_t)NULL;	\
	}					\
} while (0)

void
setup_get(void)
{
	CHECK_ENV("s", sysname);
	CHECK_ENV("n", hostname);
	CHECK_ENV("r", release);
	CHECK_ENV("v", version);
	CHECK_ENV("m", platform);
	CHECK_ENV("p", arch);
}

#define	PRINT_FLAG(flags,flag,var)		\
	if ((flags & flag) == flag) {		\
		if (get_##var != NULL)		\
			(*get_##var)();		\
		printf("%s%s", prefix, var);	\
		prefix = " ";			\
	}

void
print_uname(u_int flags)
{
	PRINT_FLAG(flags, SFLAG, sysname);
	PRINT_FLAG(flags, NFLAG, hostname);
	PRINT_FLAG(flags, RFLAG, release);
	PRINT_FLAG(flags, VFLAG, version);
	PRINT_FLAG(flags, MFLAG, platform);
	PRINT_FLAG(flags, PFLAG, arch);
	printf("\n");
}

void
native_sysname(void)
{
	int mib[2];
	size_t len;
	static char buf[1024];

	mib[0] = CTL_KERN;
	mib[1] = KERN_OSTYPE;
	len = sizeof(buf);
	if (sysctl(mib, 2, &buf, &len, NULL, 0) == -1)
		err(1, "sysctl");
	sysname = buf;
}

void
native_hostname(void)
{
	int mib[2];
	size_t len;
	static char buf[1024];

	mib[0] = CTL_KERN;
	mib[1] = KERN_HOSTNAME;
	len = sizeof(buf);
	if (sysctl(mib, 2, &buf, &len, NULL, 0) == -1)
		err(1, "sysctl");
	hostname = buf;
}

void
native_release(void)
{
	int mib[2];
	size_t len;
	static char buf[1024];

	mib[0] = CTL_KERN;
	mib[1] = KERN_OSRELEASE;
	len = sizeof(buf);
	if (sysctl(mib, 2, &buf, &len, NULL, 0) == -1)
		err(1, "sysctl");
	release = buf;
}

void
native_version(void)
{
	int mib[2];
	size_t len, tlen;
	char *p;
	static char buf[1024];

	mib[0] = CTL_KERN;
	mib[1] = KERN_VERSION;
	len = sizeof(buf);
	if (sysctl(mib, 2, &buf, &len, NULL, 0) == -1)
		err(1, "sysctl");
	for (p = buf, tlen = len; tlen--; ++p)
		if (*p == '\n' || *p == '\t')
			*p = ' ';
	version = buf;
}

void
native_platform(void)
{
	int mib[2];
	size_t len;
	static char buf[1024];

	mib[0] = CTL_HW;
	mib[1] = HW_MACHINE;
	len = sizeof(buf);
	if (sysctl(mib, 2, &buf, &len, NULL, 0) == -1)
		err(1, "sysctl");
	platform = buf;
}

void
native_arch(void)
{
	int mib[2];
	size_t len;
	static char buf[1024];

	mib[0] = CTL_HW;
	mib[1] = HW_MACHINE_ARCH;
	len = sizeof(buf);
	if (sysctl(mib, 2, &buf, &len, NULL, 0) == -1)
		err(1, "sysctl");
	arch = buf;
}

void
usage(void)
{
	fprintf(stderr, "usage: uname [-amnprsv]\n");
	exit(1);
}
