/*
 * Copyright 1987, 1988 by the Massachusetts Institute of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 * Routine to initialize user to Kerberos.  Prompts optionally for
 * user, instance and realm.  Authenticates user and gets a ticket
 * for the Kerberos ticket-granting service for future use.
 *
 * Options are:
 *
 *   -i[instance]
 *   -r[realm]
 *   -v[erbose]
 *   -l[ifetime]
 *
 *	from: kinit.c,v 4.12 90/03/20 16:11:15 jon Exp $
 *	$Id$
 */

#if 0
#ifndef	lint
static char rcsid[] =
"$Id$";
#endif	lint
#endif

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pwd.h>
#include <krb.h>

#ifndef ORGANIZATION
#define ORGANIZATION "MIT Project Athena"
#endif /*ORGANIZATION*/

#ifdef	PC
#define	LEN	64		/* just guessing */
#endif	PC

#if defined(BSD42) || defined(__FreeBSD__) || defined(__NetBSD__)
#include <strings.h>
#include <sys/param.h>
#if 	defined(ultrix) || defined(sun)
#define LEN	64
#else
#define	LEN	MAXHOSTNAMELEN
#endif	/* defined(ultrix) || defined(sun) */
#endif	/* BSD42 */

#define	LIFE	96	/* lifetime of ticket in 5-minute units */

char   *progname;

void usage(void);

void
get_input(s, size, stream)
char *s;
int size;
FILE *stream;
{
	char *p;

	if (fgets(s, size, stream) == NULL)
	  exit(1);
	if ((p = index(s, '\n')) != NULL)
		*p = '\0';
}

int
main(argc, argv)
    int argc;
    char   *argv[];
{
    char    aname[ANAME_SZ];
    char    inst[INST_SZ];
    char    realm[REALM_SZ];
    char    buf[LEN];
    char   *username = NULL;
    int     iflag, rflag, vflag, lflag, lifetime, k_errno;
    register char *cp;
    register i;

    *inst = *realm = '\0';
    iflag = rflag = vflag = lflag = 0;
    lifetime = LIFE;
    progname = (cp = rindex(*argv, '/')) ? cp + 1 : *argv;

    while (--argc) {
	if ((*++argv)[0] != '-') {
	    if (username)
		usage();
	    username = *argv;
	    continue;
	}
	for (i = 1; (*argv)[i] != '\0'; i++)
	    switch ((*argv)[i]) {
	    case 'i':		/* Instance */
		++iflag;
		continue;
	    case 'r':		/* Realm */
		++rflag;
		continue;
	    case 'v':		/* Verbose */
		++vflag;
		continue;
	    case 'l':
		++lflag;
		continue;
	    default:
		usage();
		exit(1);
	    }
    }
    if (username &&
	(k_errno = kname_parse(aname, inst, realm, username))
	!= KSUCCESS) {
	fprintf(stderr, "%s: %s\n", progname, krb_err_txt[k_errno]);
	iflag = rflag = 1;
	username = NULL;
    }
    if (k_gethostname(buf, LEN)) {
	fprintf(stderr, "%s: k_gethostname failed\n", progname);
	exit(1);
    }
    printf("%s (%s)\n", ORGANIZATION, buf);
    if (username) {
	printf("Kerberos Initialization for \"%s", aname);
	if (*inst)
	    printf(".%s", inst);
	if (*realm)
	    printf("@%s", realm);
	printf("\"\n");
    } else {
	if (iflag) {
		printf("Kerberos Initialization\n");
		printf("Kerberos name: ");
		get_input(aname, sizeof(aname), stdin);
	} else {
		int uid = getuid();
		char *getenv();
		struct passwd *pwd;

		/* default to current user name unless running as root */
		if (uid == 0 && (username = getenv("USER")) &&
		    strcmp(username, "root") != 0) {
			strncpy(aname, username, sizeof(aname));
			strncpy(inst, "root", sizeof(inst));
		} else {
			pwd = getpwuid(uid);

			if (pwd == (struct passwd *) NULL) {
				fprintf(stderr, "Unknown name for your uid\n");
				printf("Kerberos name: ");
				get_input(aname, sizeof(aname), stdin);
			} else
				strncpy(aname, pwd->pw_name, sizeof(aname));
		}
	}

	if (!*aname)
	    exit(0);
	if (!k_isname(aname)) {
	    fprintf(stderr, "%s: bad Kerberos name format\n",
		    progname);
	    exit(1);
	}
    }
    /* optional instance */
    if (iflag) {
	printf("Kerberos instance: ");
	get_input(inst, sizeof(inst), stdin);
	if (!k_isinst(inst)) {
	    fprintf(stderr, "%s: bad Kerberos instance format\n",
		    progname);
	    exit(1);
	}
    }
    if (rflag) {
	printf("Kerberos realm: ");
	get_input(realm, sizeof(realm), stdin);
	if (!k_isrealm(realm)) {
	    fprintf(stderr, "%s: bad Kerberos realm format\n",
		    progname);
	    exit(1);
	}
    }
    if (lflag) {
	 printf("Kerberos ticket lifetime (minutes): ");
	 get_input(buf, sizeof(buf), stdin);
	 lifetime = atoi(buf);
	 if (lifetime < 5)
	      lifetime = 1;
	 else
	      lifetime /= 5;
	 /* This should be changed if the maximum ticket lifetime */
	 /* changes */
	 if (lifetime > 255)
	      lifetime = 255;
    }
    if (!*realm && krb_get_lrealm(realm, 1)) {
	fprintf(stderr, "%s: krb_get_lrealm failed\n", progname);
	exit(1);
    }
    k_errno = krb_get_pw_in_tkt(aname, inst, realm, "krbtgt", realm,
				lifetime, 0);
    if (vflag) {
	printf("Kerberos realm %s:\n", realm);
	printf("%s\n", krb_err_txt[k_errno]);
    } else if (k_errno) {
	fprintf(stderr, "%s: %s\n", progname, krb_err_txt[k_errno]);
	exit(1);
    }
    return 0;
}

void
usage()
{
    fprintf(stderr, "Usage: %s [-irvl] [name]\n", progname);
    exit(1);
}
