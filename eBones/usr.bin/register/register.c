/*-
 * Copyright (c) 1989, 1993
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
"@(#) Copyright (c) 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)register.c	8.1 (Berkeley) 6/1/93";
#endif /* not lint */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/signal.h>
#include <netinet/in.h>
#include <pwd.h>
#include <stdio.h>
#include <netdb.h>
#include <kerberosIV/des.h>
#include <kerberosIV/krb.h>
#include "pathnames.h"
#include "register_proto.h"

#define	SERVICE	"krbupdate"	/* service to add to KDC's database */
#define	PROTO	"tcp"

char	realm[REALM_SZ];
char	krbhst[MAX_HSTNM];

static	char	pname[ANAME_SZ];
static	char	iname[INST_SZ];
static	char	password[_PASSWORD_LEN];

/* extern char	*sys_errlist; */
void	die();
void	setup_key(), type_info(), cleanup();

main(argc, argv)
	int	argc;
	char	**argv;
{
	struct servent	*se;
	struct hostent	*host;
	struct sockaddr_in	sin, local;
	int		rval;
	int		sock, llen;
	u_char		code;
	static struct rlimit rl = { 0, 0 };

	signal(SIGPIPE, die);

	if (setrlimit(RLIMIT_CORE, &rl) < 0) {
		perror("rlimit");
		exit(1);
	}

	if ((se = getservbyname(SERVICE, PROTO)) == NULL) {
		fprintf(stderr, "couldn't find entry for service %s\n",
			SERVICE);
		exit(1);
	}
	if ((rval = krb_get_lrealm(realm,0)) != KSUCCESS) {
		fprintf(stderr, "couldn't get local Kerberos realm: %s\n",
			krb_err_txt[rval]);
		exit(1);
	}

	if ((rval = krb_get_krbhst(krbhst, realm, 1)) != KSUCCESS) {
		fprintf(stderr, "couldn't get Kerberos host: %s\n",
			krb_err_txt[rval]);
		exit(1);
	}

	if ((host = gethostbyname(krbhst)) == NULL) {
		fprintf(stderr, "couldn't get host entry for host %s\n",
			krbhst);
		exit(1);
	}

	sin.sin_family = host->h_addrtype;
	(void)bcopy(host->h_addr, (char *) &sin.sin_addr, host->h_length);
	sin.sin_port = se->s_port;

	if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		perror("socket");
		exit(1);
	}

	if (connect(sock, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
		perror("connect");
		(void)close(sock);
		exit(1);
	}

	llen = sizeof(local);
	if (getsockname(sock, (struct sockaddr *) &local, &llen) < 0) {
		perror("getsockname");
		(void)close(sock);
		exit(1);
	}

	setup_key(local);

	type_info();

	if (!get_user_info()) {
		code = ABORT;
		(void)des_write(sock, &code, 1);
		cleanup();
		exit(1);
	}

	code = APPEND_DB;
	if (des_write(sock, &code, 1) != 1) {
		perror("write 1");
		cleanup();
		exit(1);
	}

	if (des_write(sock, pname, ANAME_SZ) != ANAME_SZ) {
		perror("write principal name");
		cleanup();
		exit(1);
	}

	if (des_write(sock, iname, INST_SZ) != INST_SZ) {
		perror("write instance name");
		cleanup();
		exit(1);
	}

	if (des_write(sock, password, 255) != 255) {
		perror("write password");
		cleanup();
		exit(1);
	}

	/* get return message */

	{
		int	cc;
		char	msgbuf[BUFSIZ];

		cc = read(sock, msgbuf, BUFSIZ);
		if (cc <= 0) {
			fprintf(stderr, "protocol error during key verification\n");
			cleanup();
			exit(1);
		}
		if (strncmp(msgbuf, GOTKEY_MSG, 6) != 0) {
			fprintf(stderr, "%s: %s", krbhst, msgbuf);
			cleanup();
			exit(1);
		}

		cc = des_read(sock, msgbuf, BUFSIZ);
		if (cc <= 0) {
			fprintf(stderr, "protocol error during read\n");
			cleanup();
			exit(1);
		} else {
			printf("%s: %s", krbhst, msgbuf);
		}
	}

	cleanup();
	(void)close(sock);
}

void
cleanup()
{
	bzero(password, 255);
}

extern	char	*crypt();
extern	char	*getpass();

int
get_user_info()
{
	int	uid = getuid();
	int	valid = 0, i;
	struct	passwd	*pw;
	char	*pas, *namep;

	/* NB: we must run setuid-root to get at the real pw file */

	if ((pw = getpwuid(uid)) == NULL) {
		fprintf(stderr, "Who are you?\n");
		return(0);
	}
	(void)seteuid(uid);
	(void)strcpy(pname, pw->pw_name);	/* principal name */

	for (i = 1; i < 3; i++) {
		pas = getpass("login password:");
		namep = crypt(pas, pw->pw_passwd);
		if (strcmp(namep, pw->pw_passwd)) {
			fprintf(stderr, "Password incorrect\n");
			continue;
		} else {
			valid = 1;
			break;
		}
	}
	if (!valid)
		return(0);
	pas = getpass("Kerberos password (may be the same):");
	while (*pas == NULL) {
		printf("<NULL> password not allowed\n");
		pas = getpass("Kerberos password (may be the same):");
	}
	(void)strcpy(password, pas);		/* password */
	pas = getpass("Retype Kerberos password:");
	if (strcmp(password, pas)) {
		fprintf(stderr, "Password mismatch -- aborted\n");
		return(0);
	}

	iname[0] = NULL;	/* null instance name */
	return(1);
}

void
setup_key(local)
	struct	sockaddr_in	local;
{
	static	struct	keyfile_data	kdata;
	static  Key_schedule		schedule;
	int	fd;
	char	namebuf[MAXPATHLEN];
	extern int errno;

	(void) sprintf(namebuf, "%s%s",
		CLIENT_KEYFILE,
		inet_ntoa(local.sin_addr));

	fd = open(namebuf, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "couldn't open key file %s for local host: ",
			namebuf);
		perror("");
		exit(1);
	}

	if (read(fd, (char *)&kdata, sizeof(kdata)) != sizeof(kdata)) {
		fprintf(stderr,"size error reading key file for local host %s\n",
			inet_ntoa(local.sin_addr));
		exit(1);
	}
	key_sched(kdata.kf_key, schedule);
	des_set_key(kdata.kf_key, schedule);
	return;
}

void
type_info()
{
	printf("Kerberos user registration (realm %s)\n\n", realm);
	printf("Please enter your login password followed by your new Kerberos password.\n");
	printf("The Kerberos password you enter now will be used in the future\n");
	printf("as your Kerberos password for all machines in the %s realm.\n", realm);
	printf("You will only be allowed to perform this operation once, although you may run\n");
	printf("the %s program from now on to change your Kerberos password.\n\n", _PATH_KPASSWD);
}

void
die()
{
	fprintf(stderr, "\nServer no longer listening\n");
	fflush(stderr);
	cleanup();
	exit(1);
}
