/*
 * Copyright (c) 2000  Peter Wemm <peter@freebsd.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/usr.bin/kenv/kenv.c,v 1.1.2.1 2000/08/08 19:24:23 peter Exp $
 */
#include <sys/types.h>
#include <sys/sysctl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <err.h>
#include <unistd.h>

static char sbuf[1024];

static void
usage(void)
{
	errx(1, "usage: [-h] [variable]");
}

int
main(int argc, char **argv)
{
	int name2oid_oid[2];
	int real_oid[CTL_MAXNAME+4];
	size_t oidlen;
	int ch, error, hflag, i, slen;
	char *env, *eq, *name, *var, *val;

	hflag = 0;
	env = NULL;
	while ((ch = getopt(argc, argv, "h")) != -1) {
		switch (ch) {
		case 'h':
			hflag++;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (argc > 0) {
		env = argv[0];
		argv++;
		argc--;
	}
	if (argc > 0)
		usage();
	name2oid_oid[0] = 0;	/* This is magic & undocumented! */
	name2oid_oid[1] = 3;
	oidlen = sizeof(real_oid);
	name = "kern.environment";
	error = sysctl(name2oid_oid, 2, real_oid, &oidlen, name, strlen(name));
	if (error < 0) 
		err(1, "cannot find kern.environment base sysctl OID");
	oidlen /= sizeof (int);
	if (oidlen >= CTL_MAXNAME)
		errx(1, "kern.environment OID is too large!");
	real_oid[oidlen] = 0;
	for (i = 0; ; i++) {
		real_oid[oidlen + 1] = i;
		slen = sizeof(sbuf) - 1;
		error = sysctl(real_oid, oidlen + 2, sbuf, &slen, NULL, 0);
		if (error < 0) {
			if (errno != ENOENT)
				err(1, "sysctl kern.environment.%d\n", i);
			break;
		}
		sbuf[sizeof(sbuf) - 1] = '\0';
		eq = strchr(sbuf, '=');
		if (eq == NULL)
			err(1, "malformed environment string: %s\n", sbuf);
		var = sbuf;
		*eq = '\0';
		val = eq + 1;
		if (env) {
			if (strcmp(var, env) != 0)
				continue;
			printf("%s\n", val);
			break;
		}
		if (hflag) {
			if (strncmp(var, "hint.", 5) != 0)
				continue;
			/* FALLTHROUGH */
		}
		printf("%s=\"", var);
		while (*val) {
			switch (*val) {
			case '"':
				putchar('\\');
				putchar('"');
				break;
			case '\\':
				putchar('\\');
				putchar('\\');
				break;
			default:
				putchar(*val);
				break;
			}
			val++;
		}
		printf("\"\n");
	}
	exit(0);
}
