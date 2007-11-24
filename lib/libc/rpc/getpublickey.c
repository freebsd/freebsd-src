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

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)publickey.c 1.10 91/03/11 Copyr 1986 Sun Micro";
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * publickey.c
 * Copyright (C) 1986, Sun Microsystems, Inc.
 */

/*
 * Public key lookup routines
 */
#include "namespace.h"
#include <stdio.h>
#include <pwd.h>
#include <rpc/rpc.h>
#include <rpc/key_prot.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
#include <string.h>
#include <stdlib.h>
#include "un-namespace.h"

#define PKFILE "/etc/publickey"

/*
 * Hack to let ypserv/rpc.nisd use AUTH_DES.
 */
int (*__getpublickey_LOCAL)() = 0;

/*
 * Get somebody's public key
 */
int
__getpublickey_real(netname, publickey)
	char *netname;
	char *publickey;
{
	char lookup[3 * HEXKEYBYTES];
	char *p;

	if (publickey == NULL)
		return (0);
	if (!getpublicandprivatekey(netname, lookup))
		return (0);
	p = strchr(lookup, ':');
	if (p == NULL) {
		return (0);
	}
	*p = '\0';
	(void) strncpy(publickey, lookup, HEXKEYBYTES);
	publickey[HEXKEYBYTES] = '\0';
	return (1);
}

/*
 * reads the file /etc/publickey looking for a + to optionally go to the
 * yellow pages
 */

int
getpublicandprivatekey(key, ret)
	char *key;
	char *ret;
{
	char buf[1024];	/* big enough */
	char *res;
	FILE *fd;
	char *mkey;
	char *mval;

	fd = fopen(PKFILE, "r");
	if (fd == NULL)
		return (0);
	for (;;) {
		res = fgets(buf, sizeof(buf), fd);
		if (res == NULL) {
			fclose(fd);
			return (0);
		}
		if (res[0] == '#')
			continue;
		else if (res[0] == '+') {
#ifdef YP
			char *PKMAP = "publickey.byname";
			char *lookup;
			char *domain;
			int err;
			int len;

			err = yp_get_default_domain(&domain);
			if (err) {
				continue;
			}
			lookup = NULL;
			err = yp_match(domain, PKMAP, key, strlen(key), &lookup, &len);
			if (err) {
#ifdef DEBUG
				fprintf(stderr, "match failed error %d\n", err);
#endif
				continue;
			}
			lookup[len] = 0;
			strcpy(ret, lookup);
			fclose(fd);
			free(lookup);
			return (2);
#else /* YP */
#ifdef DEBUG
			fprintf(stderr,
"Bad record in %s '+' -- NIS not supported in this library copy\n", PKFILE);
#endif /* DEBUG */
			continue;
#endif /* YP */
		} else {
			mkey = strsep(&res, "\t ");
			if (mkey == NULL) {
				fprintf(stderr,
				"Bad record in %s -- %s", PKFILE, buf);
				continue;
			}
			do {
				mval = strsep(&res, " \t#\n");
			} while (mval != NULL && !*mval);
			if (mval == NULL) {
				fprintf(stderr,
			"Bad record in %s val problem - %s", PKFILE, buf);
				continue;
			}
			if (strcmp(mkey, key) == 0) {
				strcpy(ret, mval);
				fclose(fd);
				return (1);
			}
		}
	}
}

int getpublickey(netname, publickey)
	const char *netname;
	char *publickey;
{
	if (__getpublickey_LOCAL != NULL)
		return(__getpublickey_LOCAL(netname, publickey));
	else
		return(__getpublickey_real(netname, publickey));
}
