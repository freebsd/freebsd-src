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
static char sccsid[] = "@(#)netdate.c	5.2 (Berkeley) 2/25/91";
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <netdb.h>
#define TSPTYPES
#include <protocols/timed.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#define	WAITACK		2	/* seconds */
#define	WAITDATEACK	5	/* seconds */

extern int retval;

/*
 * Set the date in the machines controlled by timedaemons by communicating the
 * new date to the local timedaemon.  If the timedaemon is in the master state,
 * it performs the correction on all slaves.  If it is in the slave state, it
 * notifies the master that a correction is needed.
 * Returns 0 on success.  Returns > 0 on failure, setting retval to 2;
 */
netsettime(tval)
	time_t tval;
{
	struct timeval tout;
	struct servent *sp;
	struct tsp msg;
	struct sockaddr_in sin, dest, from;
	fd_set ready;
	long waittime;
	int s, length, port, timed_ack, found, err;
	char hostname[MAXHOSTNAMELEN];

	if ((sp = getservbyname("timed", "udp")) == NULL) {
		(void)fprintf(stderr, "date: udp/timed: unknown service.n");
		return (retval = 2);
	}

	dest.sin_port = sp->s_port;
	dest.sin_family = AF_INET;
	dest.sin_addr.s_addr = htonl((u_long)INADDR_ANY);
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0) {
		if (errno != EPROTONOSUPPORT)
			perror("date: timed");
		return(retval = 2);
	}

	bzero((char *)&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	for (port = IPPORT_RESERVED - 1; port > IPPORT_RESERVED / 2; port--) {
		sin.sin_port = htons((u_short)port);
		if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) >= 0)
			break;
		if (errno == EADDRINUSE)
			continue;
		if (errno != EADDRNOTAVAIL)
			perror("date: bind");
		goto bad;
	}
	if (port == IPPORT_RESERVED / 2) {
		(void)fprintf(stderr, "date: all ports in use.\n");
		goto bad;
	}
	msg.tsp_type = TSP_SETDATE;
	msg.tsp_vers = TSPVERSION;
	if (gethostname(hostname, sizeof(hostname))) {
		perror("date: gethostname");
		goto bad;
	}
	(void)strncpy(msg.tsp_name, hostname, sizeof(hostname));
	msg.tsp_seq = htons((u_short)0);
	msg.tsp_time.tv_sec = htonl((u_long)tval);
	msg.tsp_time.tv_usec = htonl((u_long)0);
	length = sizeof(struct sockaddr_in);
	if (connect(s, (struct sockaddr *)&dest, length) < 0) {
		perror("date: connect");
		goto bad;
	}
	if (send(s, (char *)&msg, sizeof(struct tsp), 0) < 0) {
		if (errno != ECONNREFUSED)
			perror("date: send");
		goto bad;
	}

	timed_ack = -1;
	waittime = WAITACK;
loop:
	tout.tv_sec = waittime;
	tout.tv_usec = 0;

	FD_ZERO(&ready);
	FD_SET(s, &ready);
	found = select(FD_SETSIZE, &ready, (fd_set *)0, (fd_set *)0, &tout);

	length = sizeof(err);
	if (!getsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&err, &length)
	    && err) {
		if (err != ECONNREFUSED)
			perror("date: send (delayed error)");
		goto bad;
	}

	if (found > 0 && FD_ISSET(s, &ready)) {
		length = sizeof(struct sockaddr_in);
		if (recvfrom(s, &msg, sizeof(struct tsp), 0,
		    (struct sockaddr *)&from, &length) < 0) {
			if (errno != ECONNREFUSED)
				perror("date: recvfrom");
			goto bad;
		}
		msg.tsp_seq = ntohs(msg.tsp_seq);
		msg.tsp_time.tv_sec = ntohl(msg.tsp_time.tv_sec);
		msg.tsp_time.tv_usec = ntohl(msg.tsp_time.tv_usec);
		switch (msg.tsp_type) {
		case TSP_ACK:
			timed_ack = TSP_ACK;
			waittime = WAITDATEACK;
			goto loop;
		case TSP_DATEACK:
			(void)close(s);
			return (0);
		default:
			(void)fprintf(stderr,
			    "date: wrong ack received from timed: %s.\n", 
			    tsptype[msg.tsp_type]);
			timed_ack = -1;
			break;
		}
	}
	if (timed_ack == -1)
		(void)fprintf(stderr,
		    "date: can't reach time daemon, time set locally.\n");

bad:
	(void)close(s);
	return(retval = 2);
}
