/* $FreeBSD$ */
/* $OpenBSD: main.c,v 1.2 1999/10/04 21:46:28 espie Exp $ */
/*-
 * Copyright (c) 1999 Marc Espie.
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
 *	This product includes software developed by Marc Espie for the OpenBSD
 * Project.
 *
 * THIS SOFTWARE IS PROVIDED BY THE OPENBSD PROJECT AND CONTRIBUTORS 
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OPENBSD
 * PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "stand.h"
#include "gzip.h"
#include "pgp.h"
#include "extern.h"

#ifdef __OpenBSD__
extern char *__progname;
#define argv0	__progname
#else
static char *argv0;
#endif

#define NM_SIGN	"pkg_sign"

int verbose = 0;
int quiet = 0;
char *userkey = NULL;

static void 
usage()
{
	fprintf(stderr, "usage: %s [-sc] [-t type] [-u userid] [-k keyfile] pkg1 ...\n", argv0);
	exit(EXIT_FAILURE);
}

#define SIGN 0
#define CHECK 1

/* wrapper for the check_signature function (open file if needed) */
static int
check(filename, type, userid, envp)
	/*@observer@*/const char *filename;
	int type;
	/*@null@*/const char *userid;
	char *envp[];
{
	int result;
	FILE *file;

	if (strcmp(filename, "-") == 0)
		return check_signature(stdin, userid, envp, "stdin");
	file = fopen(filename, "r");
	if (file == NULL) {
		fprintf(stderr, "Can't open %s\n", filename);
		return 0;
	}
	result = check_signature(file, userid, envp, filename);
	if (fclose(file) == 0) {
		if (result == PKG_BADSIG || result == PKG_SIGERROR)
			return 0;
		else
			return 1;
	} else
		return 0;
}

int 
main(argc, argv, envp)
	int argc; 
	char *argv[];
	char *envp[];
{
	int success = 1;
	int ch;
	char *userid = NULL;
	int mode;
	int i;
	int type = TAG_ANY;

/* #ifndef BSD4_4 */
	set_program_name(argv[0]);
/* #endif */
#ifdef CHECKER_ONLY
	mode = CHECK;
#else
#ifndef __OpenBSD__
	if ((argv0 = strrchr(argv[0], '/')) != NULL)
		argv0++;
	else
		argv0 = argv[0];
#endif
	if (strcmp(argv0, NM_SIGN) == 0)
		mode = SIGN;
	else
		mode = CHECK;
#endif

	while ((ch = getopt(argc, argv, "t:u:k:qscv")) != -1) {
		switch(ch) {
		case 't':
			if (strcmp(optarg, "pgp") == 0) 
				type = TAG_PGP;
			else if (strcmp(optarg, "sha1") == 0)
				type = TAG_SHA1;
			else if (strcmp(optarg, "x509") == 0)
				type = TAG_X509;
			else
				usage();
			break;
		case 'u':
			userid = strdup(optarg);
			break;

		case 'k':
		    	userkey = optarg;
			break;

		case 'q':
		    	quiet = 1;
			break;

#ifndef CHECKER_ONLY
		case 's':
			mode = SIGN;
			break;
#endif
		case 'c':
			mode = CHECK;
			break;

		case 'v':
			verbose = 1;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (argc == 0) {
		if (mode == CHECK)
			success &= check("-", 0, userid, envp);
		else
			usage();
	}
	
#ifndef CHECKER_ONLY
	if (mode == SIGN && type == TAG_ANY)
		type = TAG_PGP;
	if (mode == SIGN && type == TAG_PGP)
		handle_pgp_passphrase();
#endif
	for (i = 0; i < argc; i++)
		success &= (mode == SIGN ? sign : check)(argv[i], type, userid, envp);
	exit(success == 1 ? EXIT_SUCCESS : EXIT_FAILURE);
}
