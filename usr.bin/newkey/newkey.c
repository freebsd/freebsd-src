/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user or with the express written consent of
 * Sun Microsystems, Inc.
 *
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 *
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 *
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 *
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */
#if !defined(lint) && defined(SCCSIDS)
static char sccsid[] = "@(#)newkey.c 1.8 91/03/11 Copyr 1986 Sun Micro";
#endif

/*
 * Copyright (C) 1986, Sun Microsystems, Inc.
 */

/*
 * Administrative tool to add a new user to the publickey database
 */
#include <stdio.h>
#include <rpc/rpc.h>
#include <rpc/key_prot.h>
#ifdef YP
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
#include <sys/wait.h>
#include <netdb.h>
#endif	/* YP */
#include <pwd.h>
#include <string.h>
#include <sys/resource.h>

#ifdef YP
#define MAXMAPNAMELEN 256
#else
#define	YPOP_CHANGE 1			/* change, do not add */
#define	YPOP_INSERT 2			/* add, do not change */
#define	YPOP_DELETE 3			/* delete this entry */
#define	YPOP_STORE  4			/* add, or change */
#define	ERR_ACCESS	1
#define	ERR_MALLOC	2
#define	ERR_READ	3
#define	ERR_WRITE	4
#define	ERR_DBASE	5
#define	ERR_KEY		6
#endif

extern char *getpass();
extern char *malloc();

#ifdef YP
static char *basename();
static char SHELL[] = "/bin/sh";
static char YPDBPATH[]="/var/yp";
static char PKMAP[] = "publickey.byname";
static char UPDATEFILE[] = "updaters";
#else
static char PKFILE[] = "/etc/publickey";
static char *err_string();
#endif	/* YP */

main(argc, argv)
	int argc;
	char *argv[];
{
	char name[MAXNETNAMELEN + 1];
	char public[HEXKEYBYTES + 1];
	char secret[HEXKEYBYTES + 1];
	char crypt1[HEXKEYBYTES + KEYCHECKSUMSIZE + 1];
	char crypt2[HEXKEYBYTES + KEYCHECKSUMSIZE + 1];
	int status;	
	char *pass;
	struct passwd *pw;
#ifdef undef
	struct hostent *h;
#endif

	if (argc != 3 || !(strcmp(argv[1], "-u") == 0 ||
		strcmp(argv[1], "-h") == 0)) {
		(void)fprintf(stderr, "usage: %s [-u username]\n",
				argv[0]);
		(void)fprintf(stderr, "usage: %s [-h hostname]\n",
					argv[0]);
		exit(1);
	}
	if (geteuid() != 0) {
		(void)fprintf(stderr, "must be superuser to run %s\n", argv[0]);
		exit(1);
	}

#ifdef YP
	if (chdir(YPDBPATH) < 0) {
		(void)fprintf(stderr, "cannot chdir to ");
		perror(YPDBPATH);
	}
#endif	/* YP */
	if (strcmp(argv[1], "-u") == 0) {
		pw = getpwnam(argv[2]);
		if (pw == NULL) {
			(void)fprintf(stderr, "unknown user: %s\n", argv[2]);
			exit(1);
		}
		(void)user2netname(name, (int)pw->pw_uid, (char *)NULL);
	} else {
#ifdef undef
		h = gethostbyname(argv[2]);
		if (h == NULL) {
			(void)fprintf(stderr, "unknown host: %s\n", argv[1]);
			exit(1);
		}
		(void)host2netname(name, h->h_name, (char *)NULL);
#else
		(void)host2netname(name, argv[2], (char *)NULL);
#endif
	}

	(void)printf("Adding new key for %s.\n", name);
	pass = getpass("New password:");
	genkeys(public, secret, pass);

	memcpy(crypt1, secret, HEXKEYBYTES);
	memcpy(crypt1 + HEXKEYBYTES, secret, KEYCHECKSUMSIZE);
	crypt1[HEXKEYBYTES + KEYCHECKSUMSIZE] = 0;
	xencrypt(crypt1, pass);

	memcpy(crypt2, crypt1, HEXKEYBYTES + KEYCHECKSUMSIZE + 1);	
	xdecrypt(crypt2, getpass("Retype password:"));
	if (memcmp(crypt2, crypt2 + HEXKEYBYTES, KEYCHECKSUMSIZE) != 0 ||
		memcmp(crypt2, secret, HEXKEYBYTES) != 0) {
		(void)fprintf(stderr, "Password incorrect.\n");
		exit(1);
	}

#ifdef YP
	(void)printf("Please wait for the database to get updated...\n");
#endif
	if (status = setpublicmap(name, public, crypt1)) {
#ifdef YP
		(void)fprintf(stderr, 
			"%s: unable to update NIS database (%u): %s\n", 
				argv[0], status, yperr_string(status));
#else
		(void)fprintf(stderr,
		"%s: unable to update publickey database (%u): %s\n",
			argv[0], status, err_string(status));
#endif
		exit(1);
	}
	(void)printf("Your new key has been successfully stored away.\n");
	exit(0);
	/* NOTREACHED */
}

/*
 * Set the entry in the public key file
 */
setpublicmap(name, public, secret)
	char *name;
	char *public;
	char *secret;
{
	char pkent[1024];

	(void)sprintf(pkent, "%s:%s", public, secret);
#ifdef YP
	return (mapupdate(name, PKMAP, YPOP_STORE,
		strlen(name), name, strlen(pkent), pkent));
#else
	return (localupdate(name, PKFILE, YPOP_STORE,
		strlen(name), name, strlen(pkent), pkent));
#endif
	}
 
#ifndef YP
	/*
	 * This returns a pointer to an error message string appropriate
	 * to an input error code.  An input value of zero will return
	 * a success message.
	 */
static char *
err_string(code)
	int code;
{
	char *pmesg;

	switch (code) {
	case 0:
		pmesg = "update operation succeeded";
		break;
	case ERR_KEY:
		pmesg = "no such key in file";
		break;
	case ERR_READ:
		pmesg = "cannot read the database";
		break;
	case ERR_WRITE:
		pmesg = "cannot write to the database";
		break;
	case ERR_DBASE:
		pmesg = "cannot update database";
		break;
	case ERR_ACCESS:
		pmesg = "permission denied";
		break;
	case ERR_MALLOC:
		pmesg = "malloc failed";
		break;
	default:
		pmesg = "unknown error";
		break;
	}
	return (pmesg);
}
#endif
