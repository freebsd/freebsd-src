/*-
 * Copyright (c) 1990, 1993, 1994
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
static char sccsid[] = "@(#)krb_passwd.c	8.3 (Berkeley) 4/2/94";
#endif /* not lint */

#ifdef KERBEROS

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <netinet/in.h>
#include <kerberosIV/des.h>
#include <kerberosIV/krb.h>

#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kpasswd_proto.h"

#include "extern.h"

#define	PROTO	"tcp"

static void	send_update __P((int, char *, char *));
static void	recv_ack __P((int));
static void	cleanup __P((void));
static void	finish __P((void));

static struct timeval timeout = { CLIENT_KRB_TIMEOUT, 0 };
static struct kpasswd_data proto_data;
static des_cblock okey;
static Key_schedule osched;
static KTEXT_ST ticket;
static Key_schedule random_schedule;
static long authopts;
static char realm[REALM_SZ], krbhst[MAX_HSTNM];
static int sock;

int
krb_passwd()
{
	struct servent *se;
	struct hostent *host;
	struct sockaddr_in sin;
	CREDENTIALS cred;
	fd_set readfds;
	int rval;
	char pass[_PASSWORD_LEN], password[_PASSWORD_LEN];
	static void finish();

	static struct rlimit rl = { 0, 0 };

	(void)signal(SIGHUP, SIG_IGN);
	(void)signal(SIGINT, SIG_IGN);
	(void)signal(SIGTSTP, SIG_IGN);

	if (setrlimit(RLIMIT_CORE, &rl) < 0) {
		warn("setrlimit");
		return (1);
	}

	if ((se = getservbyname(SERVICE, PROTO)) == NULL) {
		warnx("couldn't find entry for service %s/%s",
		    SERVICE, PROTO);
		return (1);
	}

	if ((rval = krb_get_lrealm(realm,1)) != KSUCCESS) {
		warnx("couldn't get local Kerberos realm: %s",
		    krb_err_txt[rval]);
		return (1);
	}

	if ((rval = krb_get_krbhst(krbhst, realm, 1)) != KSUCCESS) {
		warnx("couldn't get Kerberos host: %s",
		    krb_err_txt[rval]);
		return (1);
	}

	if ((host = gethostbyname(krbhst)) == NULL) {
		warnx("couldn't get host entry for krb host %s",
		    krbhst);
		return (1);
	}

	sin.sin_family = host->h_addrtype;
	memmove((char *) &sin.sin_addr, host->h_addr, host->h_length);
	sin.sin_port = se->s_port;

	if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		warn("socket");
		return (1);
	}

	if (connect(sock, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
		warn("connect");
		(void)close(sock);
		return (1);
	}

	rval = krb_sendauth(
		authopts,		/* NOT mutual */
		sock,
		&ticket,		/* (filled in) */
		SERVICE,
		krbhst,			/* instance (krbhst) */
		realm,			/* dest realm */
		(u_long) getpid(),	/* checksum */
		NULL,			/* msg data */
		NULL,			/* credentials */ 
		NULL,			/* schedule */
		NULL,			/* local addr */
		NULL,			/* foreign addr */
		"KPWDV0.1"
	);

	if (rval != KSUCCESS) {
		warnx("Kerberos sendauth error: %s", krb_err_txt[rval]);
		return (1);
	}

	krb_get_cred("krbtgt", realm, realm, &cred);

	(void)printf("Changing Kerberos password for %s.%s@%s.\n",
	    cred.pname, cred.pinst, realm);

	if (des_read_pw_string(pass,
	    sizeof(pass)-1, "Old Kerberos password:", 0)) {
		warnx("error reading old Kerberos password");
		return (1);
	}

	(void)des_string_to_key(pass, okey);
	(void)des_key_sched(okey, osched);
	(void)des_set_key(okey, osched);

	/* wait on the verification string */

	FD_ZERO(&readfds);
	FD_SET(sock, &readfds);

	rval =
	    select(sock + 1, &readfds, (fd_set *) 0, (fd_set *) 0, &timeout);

	if ((rval < 1) || !FD_ISSET(sock, &readfds)) {
		if(rval == 0) {
			warnx("timed out (aborted)");
			cleanup();
			return (1);
		}
		warnx("select failed (aborted)");
		cleanup();
		return (1);
	}

	/* read verification string */

	if (des_read(sock, &proto_data, sizeof(proto_data)) !=
	    sizeof(proto_data)) {
		warnx("couldn't read verification string (aborted)");
		cleanup();
		return (1);
	}

	(void)signal(SIGHUP, finish);
	(void)signal(SIGINT, finish);

	if (strcmp(SECURE_STRING, proto_data.secure_msg) != 0) {
		cleanup();
		/* don't complain loud if user just hit return */
		if (pass == NULL || (!*pass))
			return (0);
		(void)fprintf(stderr, "Sorry\n");
		return (1);
	}

	(void)des_key_sched(proto_data.random_key, random_schedule);
	(void)des_set_key(proto_data.random_key, random_schedule);
	(void)memset(pass, 0, sizeof(pass));

	if (des_read_pw_string(pass,
	    sizeof(pass)-1, "New Kerberos password:", 0)) {
		warnx("error reading new Kerberos password (aborted)");
		cleanup();
		return (1);
	}

	if (des_read_pw_string(password,
	    sizeof(password)-1, "Retype new Kerberos password:", 0)) {
		warnx("error reading new Kerberos password (aborted)");
		cleanup();
		return (1);
	}

	if (strcmp(password, pass) != 0) {
		warnx("password mismatch (aborted)");
		cleanup();
		return (1);
	}

	if (strlen(pass) == 0)
		(void)printf("using NULL password\n");

	send_update(sock, password, SECURE_STRING);

	/* wait for ACK */

	FD_ZERO(&readfds);
	FD_SET(sock, &readfds);

	rval =
	    select(sock + 1, &readfds, (fd_set *) 0, (fd_set *) 0, &timeout);
	if ((rval < 1) || !FD_ISSET(sock, &readfds)) {
		if(rval == 0) {
			warnx("timed out reading ACK (aborted)");
			cleanup();
			exit(1);
		}
		warnx("select failed (aborted)");
		cleanup();
		exit(1);
	}
	recv_ack(sock);
	cleanup();
	return (0);
}

static void
send_update(dest, pwd, str)
	int dest;
	char *pwd, *str;
{
	static struct update_data ud;

	(void)strncpy(ud.secure_msg, str, _PASSWORD_LEN);
	(void)strncpy(ud.pw, pwd, sizeof(ud.pw));
	if (des_write(dest, &ud, sizeof(ud)) != sizeof(ud)) {
		warnx("couldn't write pw update (abort)");
		memset((char *)&ud, 0, sizeof(ud));
		cleanup();
		exit(1);
	}
}

static void
recv_ack(remote)
	int remote;
{
	int cc;
	char buf[BUFSIZ];

	cc = des_read(remote, buf, sizeof(buf));
	if (cc <= 0) {
		warnx("error reading acknowledgement (aborted)");
		cleanup();
		exit(1);
	}
	(void)printf("%s", buf);
}

static void
cleanup()
{

	(void)memset((char *)&proto_data, 0, sizeof(proto_data));
	(void)memset((char *)okey, 0, sizeof(okey));
	(void)memset((char *)osched, 0, sizeof(osched));
	(void)memset((char *)random_schedule, 0, sizeof(random_schedule));
}

static void
finish()
{

	(void)close(sock);
	exit(1);
}

#endif /* KERBEROS */
