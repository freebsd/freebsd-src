/*-
 * Copyright (c) 2003 Mike Barcroft <mike@FreeBSD.org>
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

#include <arpa/inet.h>

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <login_cap.h>
#include <stdio.h>
#include <stdlib.h>
#include <pwd.h>
#include <unistd.h>
#include <string.h>

static void	usage(void);
int	addr2jid(const char *addr);

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
	char *username;
	ch = uflag = Uflag = 0;
	username = NULL;

	while ((ch = getopt(argc, argv, "u:U:")) != -1) {
		switch (ch) {
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
	if (uflag && Uflag)
		usage();
	if (uflag)
		GET_USER_INFO;
	jid = (int)strtol(argv[0], NULL, 10);
	if (jail_attach(jid) == -1)
		if (jail_attach(addr2jid(argv[0])) == -1)
			errx(1, "jail_attach(): Cant convert %s to jid", argv[0]);
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
		" [jid | hostname | ip-number] command ...");
	exit(1); 
}

int 
addr2jid(const char *addr)
{
	struct xprison *sxp, *xp;
	struct in_addr in;
	size_t i, len, slen;

	if (sysctlbyname("security.jail.list", NULL, &len, NULL, 0) == -1)
		err(1, "sysctlbyname(): security.jail.list");
	for (i = 0; i < 4; i++) {
		if (len <= 0)
			err(1, "sysctlbyname(): len <=0");
		sxp = xp = malloc(len);
		if (sxp == NULL)
			err(1, "malloc()");
		if (sysctlbyname("security.jail.list", xp, &len, NULL, 0) == -1) {
			if (errno == ENOMEM) {
				free(sxp);
				sxp = NULL;
				continue;
			}
			err(1, "sysctlbyname(): security.jail.list");
		}
		break;
	}
	if (sxp == NULL)
		err(1, "sysctlbyname(): security.jail.list");
	if (len < sizeof(*xp) || len % sizeof(*xp) ||
	    xp->pr_version != XPRISON_VERSION)
		errx(1, "Kernel and userland out of sync");
	slen = strlen(addr);
	for (i = 0; i < len / sizeof(*xp); i++) {
		in.s_addr = ntohl(xp->pr_ip);
		if ((strncmp(inet_ntoa(in), addr, slen) == 0) ||
		    (strncmp(xp->pr_host, addr, slen) == 0)) {
			free(sxp);
			return (xp->pr_id);
		}
		xp++;
	}
	free(sxp);
	return 0;
}
