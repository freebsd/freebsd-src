/*
 * Copyright (c) 1983, 1993
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

#include "bsd_locl.h"

RCSID("$Id: kcmd.c,v 1.20.4.1 2000/10/10 12:55:55 assar Exp $");

#define	START_PORT	5120	 /* arbitrary */

static int
getport(int *alport)
{
	struct sockaddr_in sin;
	int s;

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0)
		return (-1);
	for (;;) {
		sin.sin_port = htons((u_short)*alport);
		if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) >= 0)
			return (s);
		if (errno != EADDRINUSE) {
			close(s);
			return (-1);
		}
		(*alport)--;
#ifdef ATHENA_COMPAT
		if (*alport == IPPORT_RESERVED/2) {
#else
		if (*alport == IPPORT_RESERVED) {
#endif
			close(s);
			errno = EAGAIN;		/* close */
			return (-1);
		}
	}
}

int
kcmd(int *sock,
     char **ahost,
     u_int16_t rport, 
     char *locuser,
     char *remuser,
     char *cmd,
     int *fd2p,
     KTEXT ticket,
     char *service,
     char *realm,
     CREDENTIALS *cred,
     Key_schedule schedule,
     MSG_DAT *msg_data,
     struct sockaddr_in *laddr,
     struct sockaddr_in *faddr,
     int32_t authopts)
{
	int s, timo = 1;
	pid_t pid;
	struct sockaddr_in sin, from;
	char c;
#ifdef ATHENA_COMPAT
	int lport = IPPORT_RESERVED - 1;
#else
	int lport = START_PORT;
#endif
	struct hostent *hp;
	int rc;
	char *host_save;
	int status;
	char **h_addr_list;

	pid = getpid();
	hp = gethostbyname(*ahost);
	if (hp == NULL) {
		/* fprintf(stderr, "%s: unknown host\n", *ahost); */
		return (-1);
	}

	host_save = strdup(hp->h_name);
	if (host_save == NULL)
		return -1;
	*ahost = host_save;
	h_addr_list = hp->h_addr_list;

	/* If realm is null, look up from table */
	if (realm == NULL || realm[0] == '\0')
		realm = krb_realmofhost(host_save);

	for (;;) {
		s = getport(&lport);
		if (s < 0) {
			if (errno == EAGAIN)
				warnx("kcmd(socket): All ports in use\n");
			else
				warn("kcmd: socket");
			return (-1);
		}
		sin.sin_family = hp->h_addrtype;
		memcpy (&sin.sin_addr, h_addr_list[0], sizeof(sin.sin_addr));
		sin.sin_port = rport;
		if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) >= 0)
			break;
		close(s);
		if (errno == EADDRINUSE) {
			lport--;
			continue;
		}
		/*
		 * don't wait very long for Kerberos rcmd.
		 */
		if (errno == ECONNREFUSED && timo <= 4) {
			/* sleep(timo); don't wait at all here */
			timo *= 2;
			continue;
		}
		if (h_addr_list[1] != NULL) {
			warn ("kcmd: connect (%s)",
			      inet_ntoa(sin.sin_addr));
			h_addr_list++;
			memcpy(&sin.sin_addr,
			       *h_addr_list, 
			       sizeof(sin.sin_addr));
			fprintf(stderr, "Trying %s...\n",
				inet_ntoa(sin.sin_addr));
			continue;
		}
		if (errno != ECONNREFUSED)
			warn ("connect(%s)", hp->h_name);
		return (-1);
	}
	lport--;
	if (fd2p == 0) {
		write(s, "", 1);
		lport = 0;
	} else {
		char num[8];
		int s2 = getport(&lport), s3;
		int len = sizeof(from);

		if (s2 < 0) {
			status = -1;
			goto bad;
		}
		listen(s2, 1);
		snprintf(num, sizeof(num), "%d", lport);
		if (write(s, num, strlen(num) + 1) != strlen(num) + 1) {
			warn("kcmd(write): setting up stderr");
			close(s2);
			status = -1;
			goto bad;
		}
		{
		    fd_set fds;
		    FD_ZERO(&fds);
		    if (s >= FD_SETSIZE || s2 >= FD_SETSIZE) {
			warnx("file descriptor too large");
			close(s);
			close(s2);
			status = -1;
			goto bad;
		    }

		    FD_SET(s, &fds);
		    FD_SET(s2, &fds);
		    status = select(FD_SETSIZE, &fds, NULL, NULL, NULL);
		    if(FD_ISSET(s, &fds)){
			warnx("kcmd: connection unexpectedly closed.");
			close(s2);
			status = -1;
			goto bad;
		    }
		}
		s3 = accept(s2, (struct sockaddr *)&from, &len);
		close(s2);
		if (s3 < 0) {
			warn ("kcmd: accept");
			lport = 0;
			status = -1;
			goto bad;
		}
		
		*fd2p = s3;
		from.sin_port = ntohs((u_short)from.sin_port);
		if (from.sin_family != AF_INET ||
		    from.sin_port >= IPPORT_RESERVED) {
			warnx("kcmd(socket): "
			      "protocol failure in circuit setup.");
			status = -1;
			goto bad2;
		}
	}
	/*
	 * Kerberos-authenticated service.  Don't have to send locuser,
	 * since its already in the ticket, and we'll extract it on
	 * the other side.
	 */
	/* write(s, locuser, strlen(locuser)+1); */

	/* set up the needed stuff for mutual auth, but only if necessary */
	if (authopts & KOPT_DO_MUTUAL) {
		int sin_len;
		*faddr = sin;

		sin_len = sizeof(struct sockaddr_in);
		if (getsockname(s, (struct sockaddr *)laddr, &sin_len) < 0) {
			warn("kcmd(getsockname)");
			status = -1;
			goto bad2;
		}
	}
	if ((status = krb_sendauth(authopts, s, ticket, service, *ahost,
			       realm, (unsigned long) getpid(), msg_data,
			       cred, schedule,
			       laddr,
			       faddr,
			       "KCMDV0.1")) != KSUCCESS)
		goto bad2;

	write(s, remuser, strlen(remuser)+1);
	write(s, cmd, strlen(cmd)+1);

	if ((rc = read(s, &c, 1)) != 1) {
		if (rc == -1)
			warn("read(%s)", *ahost);
		else
			warnx("kcmd: bad connection with remote host");
		status = -1;
		goto bad2;
	}
	if (c != '\0') {
		while (read(s, &c, 1) == 1) {
			write(2, &c, 1);
			if (c == '\n')
				break;
		}
		status = -1;
		goto bad2;
	}
	*sock = s;
	return (KSUCCESS);
bad2:
	if (lport)
		close(*fd2p);
bad:
	close(s);
	return (status);
}
