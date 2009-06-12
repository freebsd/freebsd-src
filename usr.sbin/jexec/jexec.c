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
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/uio.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <login_cap.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <unistd.h>

static void	usage(void);

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
	struct iovec params[2];
	int jid;
	login_cap_t *lcap = NULL;
	struct passwd *pwd = NULL;
	gid_t groups[NGROUPS];
	int ch, ngroups, uflag, Uflag;
	char *ep, *username;
	ch = uflag = Uflag = 0;
	username = NULL;

	while ((ch = getopt(argc, argv, "nu:U:")) != -1) {
		switch (ch) {
		case 'n':
			/* Specified name, now unused */
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
	if (uflag && Uflag)
		usage();
	if (uflag)
		GET_USER_INFO;
	jid = strtoul(argv[0], &ep, 10);
	if (!*argv[0] || *ep) {
		*(const void **)&params[0].iov_base = "name";
		params[0].iov_len = sizeof("name");
		params[1].iov_base = argv[0];
		params[1].iov_len = strlen(argv[0]) + 1;
		jid = jail_get(params, 2, 0);
		if (jid < 0)
			errx(1, "Unknown jail: %s", argv[0]);
	}
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

	fprintf(stderr, "%s\n",
		"usage: jexec [-u username | -U username] jail command ...");
	exit(1); 
}
