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
 *
 * $FreeBSD$
 */
#if !defined(lint) && defined(SCCSIDS)
static char sccsid[] = "@(#)netnamer.c 1.13 91/03/11 Copyr 1986 Sun Micro";
#endif
/*
 * netname utility routines convert from unix names to network names and
 * vice-versa This module is operating system dependent! What we define here
 * will work with any unix system that has adopted the sun NIS domain
 * architecture.
 */
#include "namespace.h"
#include <sys/param.h>
#include <rpc/rpc.h>
#include <rpc/rpc_com.h>
#ifdef YP
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
#endif
#include <ctype.h>
#include <stdio.h>
#include <grp.h>
#include <pwd.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "un-namespace.h"

static char    *OPSYS = "unix";
static char    *NETID = "netid.byname";
static char    *NETIDFILE = "/etc/netid";

static int getnetid __P(( char *, char * ));
static int _getgroups __P(( char *, gid_t * ));

#ifndef NGROUPS
#define NGROUPS 16
#endif

/*
 * Convert network-name into unix credential
 */
int
netname2user(netname, uidp, gidp, gidlenp, gidlist)
	char            netname[MAXNETNAMELEN + 1];
	uid_t            *uidp;
	gid_t            *gidp;
	int            *gidlenp;
	gid_t	       *gidlist;
{
	char           *p;
	int             gidlen;
	uid_t           uid;
	long		luid;
	struct passwd  *pwd;
	char            val[1024];
	char           *val1, *val2;
	char           *domain;
	int             vallen;
	int             err;

	if (getnetid(netname, val)) {
		char *res = val;

		p = strsep(&res, ":");
		if (p == NULL)
			return (0);
		*uidp = (uid_t) atol(p);
		p = strsep(&res, "\n,");
		if (p == NULL) {
			return (0);
		}
		*gidp = (gid_t) atol(p);
		gidlen = 0;
		for (gidlen = 0; gidlen < NGROUPS; gidlen++) {
			p = strsep(&res, "\n,");
			if (p == NULL)
				break;
			gidlist[gidlen] = (gid_t) atol(p);
		}
		*gidlenp = gidlen;

		return (1);
	}
	val1 = strchr(netname, '.');
	if (val1 == NULL)
		return (0);
	if (strncmp(netname, OPSYS, (val1-netname)))
		return (0);
	val1++;
	val2 = strchr(val1, '@');
	if (val2 == NULL)
		return (0);
	vallen = val2 - val1;
	if (vallen > (1024 - 1))
		vallen = 1024 - 1;
	(void) strncpy(val, val1, 1024);
	val[vallen] = 0;

	err = __rpc_get_default_domain(&domain);	/* change to rpc */
	if (err)
		return (0);

	if (strcmp(val2 + 1, domain))
		return (0);	/* wrong domain */

	if (sscanf(val, "%ld", &luid) != 1)
		return (0);
	uid = luid;

	/* use initgroups method */
	pwd = getpwuid(uid);
	if (pwd == NULL)
		return (0);
	*uidp = pwd->pw_uid;
	*gidp = pwd->pw_gid;
	*gidlenp = _getgroups(pwd->pw_name, gidlist);
	return (1);
}

/*
 * initgroups
 */

static int
_getgroups(uname, groups)
	char           *uname;
	gid_t          groups[NGROUPS];
{
	gid_t           ngroups = 0;
	struct group *grp;
	int    i;
	int    j;
	int             filter;

	setgrent();
	while ((grp = getgrent())) {
		for (i = 0; grp->gr_mem[i]; i++)
			if (!strcmp(grp->gr_mem[i], uname)) {
				if (ngroups == NGROUPS) {
#ifdef DEBUG
					fprintf(stderr,
				"initgroups: %s is in too many groups\n", uname);
#endif
					goto toomany;
				}
				/* filter out duplicate group entries */
				filter = 0;
				for (j = 0; j < ngroups; j++)
					if (groups[j] == grp->gr_gid) {
						filter++;
						break;
					}
				if (!filter)
					groups[ngroups++] = grp->gr_gid;
			}
	}
toomany:
	endgrent();
	return (ngroups);
}

/*
 * Convert network-name to hostname
 */
int
netname2host(netname, hostname, hostlen)
	char            netname[MAXNETNAMELEN + 1];
	char           *hostname;
	int             hostlen;
{
	int             err;
	char            valbuf[1024];
	char           *val;
	char           *val2;
	int             vallen;
	char           *domain;

	if (getnetid(netname, valbuf)) {
		val = valbuf;
		if ((*val == '0') && (val[1] == ':')) {
			(void) strncpy(hostname, val + 2, hostlen);
			return (1);
		}
	}
	val = strchr(netname, '.');
	if (val == NULL)
		return (0);
	if (strncmp(netname, OPSYS, (val - netname)))
		return (0);
	val++;
	val2 = strchr(val, '@');
	if (val2 == NULL)
		return (0);
	vallen = val2 - val;
	if (vallen > (hostlen - 1))
		vallen = hostlen - 1;
	(void) strncpy(hostname, val, vallen);
	hostname[vallen] = 0;

	err = __rpc_get_default_domain(&domain);	/* change to rpc */
	if (err)
		return (0);

	if (strcmp(val2 + 1, domain))
		return (0);	/* wrong domain */
	else
		return (1);
}

/*
 * reads the file /etc/netid looking for a + to optionally go to the
 * network information service.
 */
int
getnetid(key, ret)
	char           *key, *ret;
{
	char            buf[1024];	/* big enough */
	char           *res;
	char           *mkey;
	char           *mval;
	FILE           *fd;
#ifdef YP
	char           *domain;
	int             err;
	char           *lookup;
	int             len;
#endif

	fd = fopen(NETIDFILE, "r");
	if (fd == NULL) {
#ifdef YP
		res = "+";
		goto getnetidyp;
#else
		return (0);
#endif
	}
	for (;;) {
		if (fd == NULL)
			return (0);	/* getnetidyp brings us here */
		res = fgets(buf, sizeof(buf), fd);
		if (res == NULL) {
			fclose(fd);
			return (0);
		}
		if (res[0] == '#')
			continue;
		else if (res[0] == '+') {
#ifdef YP
	getnetidyp:
			err = yp_get_default_domain(&domain);
			if (err) {
				continue;
			}
			lookup = NULL;
			err = yp_match(domain, NETID, key,
				strlen(key), &lookup, &len);
			if (err) {
#ifdef DEBUG
				fprintf(stderr, "match failed error %d\n", err);
#endif
				continue;
			}
			lookup[len] = 0;
			strcpy(ret, lookup);
			free(lookup);
			if (fd != NULL)
				fclose(fd);
			return (2);
#else	/* YP */
#ifdef DEBUG
			fprintf(stderr,
"Bad record in %s '+' -- NIS not supported in this library copy\n",
				NETIDFILE);
#endif
			continue;
#endif	/* YP */
		} else {
			mkey = strsep(&res, "\t ");
			if (mkey == NULL) {
				fprintf(stderr,
		"Bad record in %s -- %s", NETIDFILE, buf);
				continue;
			}
			do {
				mval = strsep(&res, " \t#\n");
			} while (mval != NULL && !*mval);
			if (mval == NULL) {
				fprintf(stderr,
		"Bad record in %s val problem - %s", NETIDFILE, buf);
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
