/*-
 * Copyright (c) 1990 The Regents of the University of California.
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
static char sccsid[] = "@(#)krb_passwd.c	5.4 (Berkeley) 3/1/91";
#endif /* not lint */

#ifdef KERBEROS

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <netinet/in.h>
#include <kerberosIV/des.h>
#include <kerberosIV/krb.h>
#include <netdb.h>
#include <signal.h>
#include <pwd.h>
#include <errno.h>
#include <stdio.h>
#include "kpasswd_proto.h"
#include <string.h>
#include <stdlib.h>

#define	PROTO	"tcp"

static struct timeval timeout = { CLIENT_KRB_TIMEOUT, 0 };
static struct kpasswd_data proto_data;
static des_cblock okey;
static Key_schedule osched;
KTEXT_ST ticket;
Key_schedule random_schedule;
long authopts;
char realm[REALM_SZ], krbhst[MAX_HSTNM];
int sock;

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
		(void)fprintf(stderr,
		    "passwd: setrlimit: %s\n", strerror(errno));
		return(1);
	}

	if ((se = getservbyname(SERVICE, PROTO)) == NULL) {
		(void)fprintf(stderr,
		    "passwd: couldn't find entry for service %s/%s\n",
		    SERVICE, PROTO);
		return(1);
	}

	if ((rval = krb_get_lrealm(realm,1)) != KSUCCESS) {
		(void)fprintf(stderr,
		    "passwd: couldn't get local Kerberos realm: %s\n",
		    krb_err_txt[rval]);
		return(1);
	}

	if ((rval = krb_get_krbhst(krbhst, realm, 1)) != KSUCCESS) {
		(void)fprintf(stderr,
		    "passwd: couldn't get Kerberos host: %s\n",
		    krb_err_txt[rval]);
		return(1);
	}

	if ((host = gethostbyname(krbhst)) == NULL) {
		(void)fprintf(stderr,
		    "passwd: couldn't get host entry for krb host %s\n",
		    krbhst);
		return(1);
	}

	sin.sin_family = host->h_addrtype;
	bcopy(host->h_addr, (char *) &sin.sin_addr, host->h_length);
	sin.sin_port = se->s_port;

	if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		(void)fprintf(stderr, "passwd: socket: %s\n", strerror(errno));
		return(1);
	}

	if (connect(sock, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
		(void)fprintf(stderr, "passwd: connect: %s\n", strerror(errno));
		(void)close(sock);
		return(1);
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
		(void)fprintf(stderr, "passwd: Kerberos sendauth error: %s\n",
		    krb_err_txt[rval]);
		return(1);
	}

	krb_get_cred("krbtgt", realm, realm, &cred);

	(void)printf("Changing Kerberos password for %s.%s@%s.\n",
	    cred.pname, cred.pinst, realm);

	if (des_read_pw_string(pass,
	    sizeof(pass)-1, "Old Kerberos password:", 0)) {
		(void)fprintf(stderr,
		    "passwd: error reading old Kerberos password\n");
		return(1);
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
			(void)fprintf(stderr, "passwd: timed out (aborted)\n");
			cleanup();
			return(1);
		}
		(void)fprintf(stderr, "passwd: select failed (aborted)\n");
		cleanup();
		return(1);
	}

	/* read verification string */

	if (des_read(sock, &proto_data, sizeof(proto_data)) !=
	    sizeof(proto_data)) {
		(void)fprintf(stderr,
		    "passwd: couldn't read verification string (aborted)\n");
		cleanup();
		return(1);
	}

	(void)signal(SIGHUP, finish);
	(void)signal(SIGINT, finish);

	if (strcmp(SECURE_STRING, proto_data.secure_msg) != 0) {
		cleanup();
		/* don't complain loud if user just hit return */
		if (pass == NULL || (!*pass))
			return(0);
		(void)fprintf(stderr, "Sorry\n");
		return(1);
	}

	(void)des_key_sched(proto_data.random_key, random_schedule);
	(void)des_set_key(proto_data.random_key, random_schedule);
	(void)bzero(pass, sizeof(pass));

	if (des_read_pw_string(pass,
	    sizeof(pass)-1, "New Kerberos password:", 0)) {
		(void)fprintf(stderr,
		    "passwd: error reading new Kerberos password (aborted)\n");
		cleanup();
		return(1);
	}

	if (des_read_pw_string(password,
	    sizeof(password)-1, "Retype new Kerberos password:", 0)) {
		(void)fprintf(stderr,
		    "passwd: error reading new Kerberos password (aborted)\n");
		cleanup();
		return(1);
	}

	if (strcmp(password, pass) != 0) {
		(void)fprintf(stderr,
		    "passwd: password mismatch (aborted)\n");
		cleanup();
		return(1);
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
			(void)fprintf(stderr,
			    "passwd: timed out reading ACK (aborted)\n");
			cleanup();
			exit(1);
		}
		(void)fprintf(stderr, "passwd: select failed (aborted)\n");
		cleanup();
		exit(1);
	}
	recv_ack(sock);
	cleanup();
	exit(0);
}

send_update(dest, pwd, str)
	int dest;
	char *pwd, *str;
{
	static struct update_data ud;

	(void)strncpy(ud.secure_msg, str, _PASSWORD_LEN);
	(void)strncpy(ud.pw, pwd, sizeof(ud.pw));
	if (des_write(dest, &ud, sizeof(ud)) != sizeof(ud)) {
		(void)fprintf(stderr,
		    "passwd: couldn't write pw update (abort)\n");
		bzero((char *)&ud, sizeof(ud));
		cleanup();
		exit(1);
	}
}

recv_ack(remote)
	int remote;
{
	int cc;
	char buf[BUFSIZ];

	cc = des_read(remote, buf, sizeof(buf));
	if (cc <= 0) {
		(void)fprintf(stderr,
		    "passwd: error reading acknowledgement (aborted)\n");
		cleanup();
		exit(1);
	}
	(void)printf("%s", buf);
}

cleanup()
{
	(void)bzero((char *)&proto_data, sizeof(proto_data));
	(void)bzero((char *)okey, sizeof(okey));
	(void)bzero((char *)osched, sizeof(osched));
	(void)bzero((char *)random_schedule, sizeof(random_schedule));
}

static void
finish()
{
	(void)close(sock);
	exit(1);
}

#endif /* KERBEROS */
