/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
 * The ulimit() builtin has been contributed by Joerg Wunsch.
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
 *
 *	$Id: miscbltin.c,v 1.3 1995/10/19 18:42:10 joerg Exp $
 */

#ifndef lint
static char sccsid[] = "@(#)miscbltin.c	8.2 (Berkeley) 4/16/94";
#endif /* not lint */

/*
 * Miscelaneous builtins.
 */

#include "shell.h"
#include "options.h"
#include "var.h"
#include "output.h"
#include "memalloc.h"
#include "error.h"
#include "mystring.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#if BSD
#include <limits.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#endif

#undef eflag

extern char **argptr;		/* argument list for builtin command */


/*
 * The read builtin.  The -e option causes backslashes to escape the
 * following character.
 *
 * This uses unbuffered input, which may be avoidable in some cases.
 */

readcmd(argc, argv)  char **argv; {
	char **ap;
	int backslash;
	char c;
	int eflag;
	char *prompt;
	char *ifs;
	char *p;
	int startword;
	int status;
	int i;

	eflag = 0;
	prompt = NULL;
	while ((i = nextopt("ep:")) != '\0') {
		if (i == 'p')
			prompt = optarg;
		else
			eflag = 1;
	}
	if (prompt && isatty(0)) {
		out2str(prompt);
		flushall();
	}
	if (*(ap = argptr) == NULL)
		error("arg count");
	if ((ifs = bltinlookup("IFS", 1)) == NULL)
		ifs = nullstr;
	status = 0;
	startword = 1;
	backslash = 0;
	STARTSTACKSTR(p);
	for (;;) {
		if (read(0, &c, 1) != 1) {
			status = 1;
			break;
		}
		if (c == '\0')
			continue;
		if (backslash) {
			backslash = 0;
			if (c != '\n')
				STPUTC(c, p);
			continue;
		}
		if (eflag && c == '\\') {
			backslash++;
			continue;
		}
		if (c == '\n')
			break;
		if (startword && *ifs == ' ' && strchr(ifs, c)) {
			continue;
		}
		startword = 0;
		if (backslash && c == '\\') {
			if (read(0, &c, 1) != 1) {
				status = 1;
				break;
			}
			STPUTC(c, p);
		} else if (ap[1] != NULL && strchr(ifs, c) != NULL) {
			STACKSTRNUL(p);
			setvar(*ap, stackblock(), 0);
			ap++;
			startword = 1;
			STARTSTACKSTR(p);
		} else {
			STPUTC(c, p);
		}
	}
	STACKSTRNUL(p);
	setvar(*ap, stackblock(), 0);
	while (*++ap != NULL)
		setvar(*ap, nullstr, 0);
	return status;
}



umaskcmd(argc, argv)  char **argv; {
	int mask;
	char *p;
	int i;

	if ((p = argv[1]) == NULL) {
		INTOFF;
		mask = umask(0);
		umask(mask);
		INTON;
		out1fmt("%.4o\n", mask);	/* %#o might be better */
	} else {
		mask = 0;
		do {
			if ((unsigned)(i = *p - '0') >= 8)
				error("Illegal number: %s", argv[1]);
			mask = (mask << 3) + i;
		} while (*++p != '\0');
		umask(mask);
	}
	return 0;
}


#if BSD
struct restab {
	int resource;
	int scale;
	char *descript;
};

/* multi-purpose */
#define RLIMIT_UNSPEC (-2)

/* resource */
#define RLIMIT_ALL (-1)

/* mode */
#define RLIMIT_SHOW 0
#define RLIMIT_SET 1

/* what */
#define RLIMIT_SOFT 1
#define RLIMIT_HARD 2

static struct restab restab[] = {
	{RLIMIT_CORE,     512,  "coredump(512-blocks)    "},
	{RLIMIT_CPU,      1,    "time(seconds)           "},
	{RLIMIT_DATA,     1024, "datasize(kilobytes)     "},
	{RLIMIT_FSIZE,    512,  "filesize(512-blocks)    "},
	{RLIMIT_MEMLOCK,  1024, "lockedmem(kilobytes)    "},
	{RLIMIT_NOFILE,   1,    "nofiles(descriptors)    "},
	{RLIMIT_NPROC,    1,    "userprocs(max)          "},
	{RLIMIT_RSS,      1024, "memoryuse(kilobytes)    "},
	{RLIMIT_STACK,    1024, "stacksize(kilobytes)    "}
};

/* get entry into above table */
static struct restab *
find_resource(resource) {
	int i;
	struct restab *rp;

	for(i = 0, rp = restab;
	    i < sizeof restab / sizeof(struct restab);
	    i++, rp++)
		if(rp->resource == resource)
			return rp;
	error("internal error: resource not in table");
	return 0;
}

static void
print_resource(rp, what, with_descript) struct restab *rp; {
	struct rlimit rlim;	
	quad_t val;

	(void)getrlimit(rp->resource, &rlim);
	val = (what == RLIMIT_SOFT)?
		rlim.rlim_cur: rlim.rlim_max;
	if(with_descript)
		out1str(rp->descript);
	if(val == RLIM_INFINITY)
		out1str("unlimited\n");
	else {
		val /= (quad_t)rp->scale;
		if(val > (quad_t)ULONG_MAX)
			out1fmt("> %lu\n", (unsigned long)ULONG_MAX);
		else
			out1fmt("%lu\n", (unsigned long)val);
	}
}

ulimitcmd(argc, argv)  char **argv; {
	struct rlimit rlim;
	char *p;
	int i;
	int resource = RLIMIT_UNSPEC;
	quad_t val;
	int what = RLIMIT_UNSPEC;
	int mode = RLIMIT_UNSPEC;
	int errs = 0, arg = 1;
	struct restab *rp;
	extern int optreset;	/* XXX should be declared in <stdlib.h> */

	opterr = 0;		/* use own error processing */
	optreset = 1;
	optind = 1;
	while ((i = getopt(argc, argv, "HSacdfnstmlu")) != EOF) {
		arg++;
		switch(i) {
		case 'H':
			if(what == RLIMIT_UNSPEC) what = 0;
			what |= RLIMIT_HARD;
			break;
		case 'S':
			if(what == RLIMIT_UNSPEC) what = 0;
			what |= RLIMIT_SOFT;
			break;
		case 'a':
			if(resource != RLIMIT_UNSPEC) errs++;
			resource = RLIMIT_ALL;
			mode = RLIMIT_SHOW;
			break;
		case 'c':
			if(resource != RLIMIT_UNSPEC) errs++;
			resource = RLIMIT_CORE;
			break;
		case 'd':
			if(resource != RLIMIT_UNSPEC) errs++;
			resource = RLIMIT_DATA;
			break;
		case 'f':
			if(resource != RLIMIT_UNSPEC) errs++;
			resource = RLIMIT_FSIZE;
			break;
		case 'n':
			if(resource != RLIMIT_UNSPEC) errs++;
			resource = RLIMIT_NOFILE;
			break;
		case 's':
			if(resource != RLIMIT_UNSPEC) errs++;
			resource = RLIMIT_STACK;
			break;
		case 't':
			if(resource != RLIMIT_UNSPEC) errs++;
			resource = RLIMIT_CPU;
			break;
		case 'm':
			if(resource != RLIMIT_UNSPEC) errs++;
			resource = RLIMIT_RSS;
			break;
		case 'l':
			if(resource != RLIMIT_UNSPEC) errs++;
			resource = RLIMIT_MEMLOCK;
			break;
		case 'u':
			if(resource != RLIMIT_UNSPEC) errs++;
			resource = RLIMIT_NPROC;
			break;
		case '?':
			error("illegal option -%c", optopt);
		}
	}

	argc -= optind;
	argv += optind;
	if(argc > 1)
		error("too many arguments");
	if(argc == 0)
		mode = RLIMIT_SHOW;
	else if (resource == RLIMIT_ALL)
		errs++;
	else
		mode = RLIMIT_SET;
	if(mode == RLIMIT_UNSPEC)
		mode = RLIMIT_SHOW;
	if(resource == RLIMIT_UNSPEC)
		resource = RLIMIT_FSIZE;
	if(what == RLIMIT_UNSPEC)
		what = (mode == RLIMIT_SHOW)?
			RLIMIT_SOFT: (RLIMIT_SOFT|RLIMIT_HARD);
	if(mode == RLIMIT_SHOW && what == (RLIMIT_SOFT|RLIMIT_HARD))
		errs++;
	if(errs)
		error("Wrong option combination");
	
	if(resource == RLIMIT_ALL)
		for(i = 0; i < sizeof restab / sizeof(struct restab); i++)
			print_resource(restab + i, what, 1);
	else if(mode == RLIMIT_SHOW)
		print_resource(find_resource(resource), what, 0);
	else {
		rp = find_resource(resource);
		if(strcmp(argv[0], "unlimited") == 0)
			val = RLIM_INFINITY;
		else {
			val = 0;
			p = argv[0];
			do {
				if((i = *p - '0') < 0 || i > 9)
					error("Illegal number: %s", argv[0]);
				val = (10 * val) + (quad_t)i;
			} while (*++p != '\0');
			val *= (quad_t)rp->scale;
		}
		(void)getrlimit(resource, &rlim);
		if(what & RLIMIT_HARD)
			rlim.rlim_max = val;
		if(what & RLIMIT_SOFT)
			rlim.rlim_cur = val;
		if(setrlimit(resource, &rlim) == -1) {
			outfmt(&errout, "ulimit: bad limit: %s\n",
			       strerror(errno));
			return 1;
		}
	}
	return 0;
}
#else /* !BSD */
#error ulimit() not implemented
#endif /* BSD */
