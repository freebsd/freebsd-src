/*
 * Copyright (c) 1988, 1993, 1994
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
static char copyright[] =
"@(#) Copyright (c) 1988, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "From: @(#)passwd.c	8.3 (Berkeley) 4/2/94";
static const char rcsid[] =
	"$Id$";
#endif /* not lint */

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef KERBEROS
#include "krb.h"
#endif

#include "extern.h"

void	usage __P((void));

int use_local_passwd = 0;

int
main(argc, argv)
	int argc;
	char **argv;
{
	int ch;
	char *uname;
	char *iflag = 0, *rflag = 0, *uflag = 0;

#ifdef KERBEROS
	char realm[REALM_SZ];
#define OPTIONS "li:r:u:"
#else
#define OPTIONS "l"
#endif
	while ((ch = getopt(argc, argv, OPTIONS)) != EOF) {
		switch (ch) {
		case 'l':		/* change local password file */
			use_local_passwd = 1;
			break;
#ifdef KERBEROS
		case 'i':
			iflag = optarg;
			break;
		case 'r':
			rflag = optarg;
			break;
		case 'u':
			uflag = optarg;
			break;
#endif /* KERBEROS */

		default:
		case '?':
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if ((uname = getlogin()) == NULL)
		err(1, "getlogin");

	switch(argc) {
	case 0:
		break;
	case 1:
		uname = argv[0];
		break;
	default:
		usage();
	}

	if (!use_local_passwd) {
#ifdef	KERBEROS
		if(krb_get_lrealm(realm, 0) == KSUCCESS) {
			fprintf(stderr, "realm %s\n", realm);
			exit(krb_passwd(argv[0], iflag, rflag, uflag));
		}
#endif
	}
	exit(local_passwd(uname));
}

void
usage()
{

#ifdef	KERBEROS
	fprintf(stderr,
	 "usage: passwd [-l] [-i instance] [-r realm] [-u fullname] [user]\n");
#else
	(void)fprintf(stderr, "usage: passwd user\n");
#endif
	exit(1);
}
