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
	"$Id: passwd.c,v 1.2 1995/01/20 22:03:36 wollman Exp $";
#endif /* not lint */

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef YP
#include <pwd.h>
#include <limits.h>
#include <db.h>
#include <fcntl.h>
#include <utmp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#endif

#ifdef KERBEROS
#include "krb.h"
#endif

#include "extern.h"

void	usage __P((void));

int use_local_passwd = 0;

#ifdef YP
#define PERM_SECURE (S_IRUSR|S_IWUSR)
int use_yp_passwd = 0, opt_shell = 0, opt_fullname = 0;
char *prog_name;
HASHINFO openinfo = {
        4096,           /* bsize */
        32,             /* ffactor */
        256,            /* nelem */
        2048 * 1024,    /* cachesize */
        NULL,           /* hash */
        0,              /* lorder */
};
#endif

int
main(argc, argv)
	int argc;
	char **argv;
{
	int ch;
	char *uname;
	char *iflag = 0, *rflag = 0, *uflag = 0;

#ifdef YP
#ifdef KERBEROS
	char realm[REALM_SZ];
#define OPTIONS "lysfi:r:u:"
#else
#define OPTIONS "lysf"
#endif
#else
#ifdef KERBEROS
	char realm[REALM_SZ];
#define OPTIONS "li:r:u:"
#else
#define OPTIONS "l"
#endif
#endif

#ifdef YP
	DB *dbp;
	DBT key,data;
	char bf[UT_NAMESIZE + 2];

	if (strstr(argv[0], (prog_name = "ypchpass")))
		use_yp_passwd = opt_shell = opt_fullname = 1;
	if (strstr(argv[0], (prog_name = "ypchsh"))) opt_shell = 1;
	if (strstr(argv[0], (prog_name = "ypchfn"))) opt_fullname = 1;
	if (strstr(argv[0], (prog_name = "yppasswd"))) use_yp_passwd = 1;
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
#ifdef	YP
		case 'y':			/* Change NIS password */
			use_yp_passwd = 1;
			break;
		case 's':			/* Change NIS shell field */
			opt_shell = 1;
			break;
		case 'f':			/* Change NIS GECOS field */
			opt_fullname = 1;
			break;
#endif
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

#ifdef YP
	/*
	 * If the user isn't in the local database file, he must
	 * be in the NIS database.
	 */
#ifdef KERBEROS
	if (!use_yp_passwd && !opt_shell && !opt_fullname &&
		iflag == NULL && rflag == NULL && uflag == NULL) {
#else
	if (!use_yp_passwd && !opt_shell && !opt_fullname) {
#endif
		if ((dbp = dbopen(_PATH_MP_DB, O_RDONLY, PERM_SECURE,
				DB_HASH, &openinfo)) == NULL)
			errx(1, "error opening database: %s.", _PATH_MP_DB);

		bf[0] = _PW_KEYBYNAME;
		bcopy(uname, bf + 1, MIN(strlen(uname), UT_NAMESIZE));
		key.data = (u_char *)bf;
		key.size = strlen(uname) + 1;
		if ((dbp->get)(dbp,&key,&data,0))
			use_yp_passwd = 1;
		(dbp->close)(dbp);
	}

	if (!use_local_passwd && (use_yp_passwd || opt_shell || opt_fullname))
		exit(yp_passwd(uname));
#endif

	if (!use_local_passwd) {
#ifdef	KERBEROS
		if(krb_get_lrealm(realm, 0) == KSUCCESS) {
			fprintf(stderr, "realm %s\n", realm);
			exit(krb_passwd(argv[0], iflag, rflag, uflag));
		}
#endif
	}
#ifdef YP
	if (use_local_passwd && use_yp_passwd)
		errx(1,"unknown local user: %s.",uname);
#endif
	exit(local_passwd(uname));
}

void
usage()
{

#ifdef	YP
#ifdef	KERBEROS
	fprintf(stderr,
	 "usage: passwd [-l] [-i instance] [-r realm] [-u fullname]\n");
	fprintf(stderr,
	"        [-l] [-y] [-f] [-s] [user]\n");
#else
	(void)fprintf(stderr, "usage: passwd [-y] [-f] [-s] [user] \n");
#endif
#else
#ifdef	KERBEROS
	fprintf(stderr,
	 "usage: passwd [-l] [-i instance] [-r realm] [-u fullname] [user]\n");
#else
	(void)fprintf(stderr, "usage: passwd user\n");
#endif
#endif
	exit(1);
}
