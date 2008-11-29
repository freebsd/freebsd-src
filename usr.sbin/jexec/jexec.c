/*-
 * Copyright (c) 2003 Mike Barcroft <mike@FreeBSD.org>
 * Copyright (c) 2008 Bjoern A. Zeeb <bz@FreeBSD.org>
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/jail.h>
#include <sys/sysctl.h>

#include <netinet/in.h>

#include <err.h>
#include <errno.h>
#include <login_cap.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <unistd.h>

static void	usage(void);

#ifdef SUPPORT_OLD_XPRISON
static
char *lookup_xprison_v1(void *p, char *end, int *id)
{
	struct xprison_v1 *xp;

	if (id == NULL)
		errx(1, "Internal error. Invalid ID pointer.");

	if ((char *)p + sizeof(struct xprison_v1) > end)
		errx(1, "Invalid length for jail");

	xp = (struct xprison_v1 *)p;

	*id = xp->pr_id;
	return ((char *)(xp + 1));
}
#endif

static
char *lookup_xprison_v3(void *p, char *end, int *id, char *jailname)
{
	struct xprison *xp;
	char *q;
	int ok;

	if (id == NULL)
		errx(1, "Internal error. Invalid ID pointer.");

	if ((char *)p + sizeof(struct xprison) > end)
		errx(1, "Invalid length for jail");

	xp = (struct xprison *)p;
	ok = 1;

	/* Jail state and name. */
	if (xp->pr_state < 0 || xp->pr_state >
	    (int)((sizeof(prison_states) / sizeof(struct prison_state))))
		errx(1, "Invalid jail state.");
	else if (xp->pr_state != PRISON_STATE_ALIVE)
		ok = 0;
	if (jailname != NULL) {
		if (xp->pr_name == NULL)
			ok = 0;
		else if (strcmp(jailname, xp->pr_name) != 0)
			ok = 0;
	}

	q = (char *)(xp + 1);
	/* IPv4 addresses. */
	q += (xp->pr_ip4s * sizeof(struct in_addr));
	if ((char *)q > end)
		errx(1, "Invalid length for jail");
	/* IPv6 addresses. */
	q += (xp->pr_ip6s * sizeof(struct in6_addr));
	if ((char *)q > end)
		errx(1, "Invalid length for jail");

	if (ok)
		*id = xp->pr_id;
	return (q);
}

static int
lookup_jail(int jid, char *jailname)
{
	size_t i, j, len;
	void *p, *q;
	int version, id, xid, count;

	if (sysctlbyname("security.jail.list", NULL, &len, NULL, 0) == -1)
		err(1, "sysctlbyname(): security.jail.list");

	j = len;
	for (i = 0; i < 4; i++) {
		if (len <= 0)
			exit(0);	
		p = q = malloc(len);
		if (p == NULL)
			err(1, "malloc()");

		if (sysctlbyname("security.jail.list", q, &len, NULL, 0) == -1) {
			if (errno == ENOMEM) {
				free(p);
				p = NULL;
				len += j;
				continue;
			}
			err(1, "sysctlbyname(): security.jail.list");
		}
		break;
	}
	if (p == NULL)
		err(1, "sysctlbyname(): security.jail.list");
	if (len < sizeof(int))
		errx(1, "This is no prison. Kernel and userland out of sync?");
	version = *(int *)p;
	if (version > XPRISON_VERSION)
		errx(1, "Sci-Fi prison. Kernel/userland out of sync?");

	count = 0;
	xid = -1;
	for (; q != NULL && (char *)q + sizeof(int) < (char *)p + len;) {
		version = *(int *)q;
		if (version > XPRISON_VERSION)
			errx(1, "Sci-Fi prison. Kernel/userland out of sync?");
		id = -1;
		switch (version) {
#ifdef SUPPORT_OLD_XPRISON
		case 1:
			if (jailname != NULL)
				errx(1, "Version 1 prisons did not "
				    "support jail names.");
			q = lookup_xprison_v1(q, (char *)p + len, &id);
			break;
		case 2:
			errx(1, "Version 2 was used by multi-IPv4 jail "
			    "implementations that never made it into the "
			    "official kernel.");
			/* NOTREACHED */
			break;
#endif
		case 3:
			q = lookup_xprison_v3(q, (char *)p + len, &id, jailname);
			break;
		default:
			errx(1, "Prison unknown. Kernel/userland out of sync?");
			/* NOTREACHED */
			break;
		}
		/* Possible match. */
		if (id > 0) {
			/* Do we have a jail ID to match as well? */
			if (jid > 0) {
				if (jid == id) {
					xid = id;
					count++;
				}
			} else {
				xid = id;
				count++;
			}
		}
	}

	free(p);

	if (count != 1)
		errx(1, "Could not uniquely identify the jail.");

	return (xid);
}

#define GET_USER_INFO do {						\
	pwd = getpwnam(username);					\
	if (pwd == NULL) {						\
		if (errno)						\
			err(1, "getpwnam: %s", username);		\
		else							\
			errx(1, "%s: no such user", username);		\
	}								\
	lcap = login_getpwclass(pwd);					\
	if (lcap == NULL)						\
		err(1, "getpwclass: %s", username);			\
	ngroups = NGROUPS;						\
	if (getgrouplist(username, pwd->pw_gid, groups, &ngroups) != 0)	\
		err(1, "getgrouplist: %s", username);			\
} while (0)

int
main(int argc, char *argv[])
{
	int jid;
	login_cap_t *lcap = NULL;
	struct passwd *pwd = NULL;
	gid_t groups[NGROUPS];
	int ch, ngroups, uflag, Uflag;
	char *jailname, *username;

	ch = uflag = Uflag = 0;
	jailname = username = NULL;
	jid = -1;

	while ((ch = getopt(argc, argv, "i:n:u:U:")) != -1) {
		switch (ch) {
		case 'n':
			jailname = optarg;
			break;
		case 'u':
			username = optarg;
			uflag = 1;
			break;
		case 'U':
			username = optarg;
			Uflag = 1;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (argc < 2)
		usage();
	if (strlen(argv[0]) > 0) {
		jid = (int)strtol(argv[0], NULL, 10);
		if (errno)
			err(1, "Unable to parse jail ID.");
	}
	if (jid <= 0 && jailname == NULL) {
		fprintf(stderr, "Neither jail ID nor jail name given.\n");
		usage();
	}
	if (uflag && Uflag)
		usage();
	if (uflag)
		GET_USER_INFO;
	jid = lookup_jail(jid, jailname);
	if (jid <= 0)
		errx(1, "Cannot identify jail.");
	if (jail_attach(jid) == -1)
		err(1, "jail_attach(): %d", jid);
	if (chdir("/") == -1)
		err(1, "chdir(): /");
	if (username != NULL) {
		if (Uflag)
			GET_USER_INFO;
		if (setgroups(ngroups, groups) != 0)
			err(1, "setgroups");
		if (setgid(pwd->pw_gid) != 0)
			err(1, "setgid");
		if (setusercontext(lcap, pwd, pwd->pw_uid,
		    LOGIN_SETALL & ~LOGIN_SETGROUP & ~LOGIN_SETLOGIN) != 0)
			err(1, "setusercontext");
		login_close(lcap);
	}
	if (execvp(argv[1], argv + 1) == -1)
		err(1, "execvp(): %s", argv[1]);
	exit(0);
}

static void
usage(void)
{

	fprintf(stderr, "%s%s\n",
		"usage: jexec [-u username | -U username]",
		" [-n jailname] jid command ...");
	exit(1); 
}
