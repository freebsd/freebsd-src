/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1985, 1993
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
 * 3. Neither the name of the University nor the names of its contributors
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
#if 0
static char sccsid[] = "@(#)readmsg.c	8.1 (Berkeley) 6/6/93";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#define	TSPTYPES
#include "globals.h"

/*
 * LOOKAT checks if the message is of the requested type and comes from
 * the right machine, returning 1 in case of affirmative answer
 */
#define LOOKAT(msg, mtype, mfrom, netp, froms) \
	(((mtype) == TSP_ANY || (mtype) == (msg).tsp_type) &&		\
	 ((mfrom) == 0 || !strcmp((mfrom), (msg).tsp_name)) &&		\
	 ((netp) == 0 || 						\
	  ((netp)->mask & (froms).sin_addr.s_addr) == (netp)->net.s_addr))

struct timeval rtime, rwait, rtout;
struct tsp msgin;
static struct tsplist {
	struct tsp info;
	struct timeval when;
	struct sockaddr_in addr;
	struct tsplist *p;
} msgslist;
struct sockaddr_in from;
struct netinfo *fromnet;
struct timeval from_when;

/*
 * `readmsg' returns message `type' sent by `machfrom' if it finds it
 * either in the receive queue, or in a linked list of previously received
 * messages that it maintains.
 * Otherwise it waits to see if the appropriate message arrives within
 * `intvl' seconds. If not, it returns NULL.
 */

struct tsp *
readmsg(int type, char *machfrom, struct timeval *intvl, struct netinfo *netfrom)
{
	int length;
	fd_set ready;
	static struct tsplist *head = &msgslist;
	static struct tsplist *tail = &msgslist;
	static int msgcnt = 0;
	struct tsplist *prev;
	register struct netinfo *ntp;
	register struct tsplist *ptr;
	ssize_t n;

	if (trace) {
		fprintf(fd, "readmsg: looking for %s from %s, %s\n",
			tsptype[type], machfrom == NULL ? "ANY" : machfrom,
			netfrom == NULL ? "ANYNET" : inet_ntoa(netfrom->net));
		if (head->p != NULL) {
			length = 1;
			for (ptr = head->p; ptr != NULL; ptr = ptr->p) {
				/* do not repeat the hundreds of messages */
				if (++length > 3) {
					if (ptr == tail) {
						fprintf(fd,"\t ...%d skipped\n",
							length);
					} else {
						continue;
					}
				}
				fprintf(fd, length > 1 ? "\t" : "queue:\t");
				print(&ptr->info, &ptr->addr);
			}
		}
	}

	ptr = head->p;
	prev = head;

	/*
	 * Look for the requested message scanning through the
	 * linked list. If found, return it and free the space
	 */

	while (ptr != NULL) {
		if (LOOKAT(ptr->info, type, machfrom, netfrom, ptr->addr)) {
again:
			msgin = ptr->info;
			from = ptr->addr;
			from_when = ptr->when;
			prev->p = ptr->p;
			if (ptr == tail)
				tail = prev;
			free((char *)ptr);
			fromnet = NULL;
			if (netfrom == NULL)
			    for (ntp = nettab; ntp != NULL; ntp = ntp->next) {
				    if ((ntp->mask & from.sin_addr.s_addr) ==
					ntp->net.s_addr) {
					    fromnet = ntp;
					    break;
				    }
			    }
			else
			    fromnet = netfrom;
			if (trace) {
				fprintf(fd, "readmsg: found ");
				print(&msgin, &from);
			}

/* The protocol can get far behind.  When it does, it gets
 *	hopelessly confused.  So delete duplicate messages.
 */
			for (ptr = prev; (ptr = ptr->p) != NULL; prev = ptr) {
				if (ptr->addr.sin_addr.s_addr
					== from.sin_addr.s_addr
				    && ptr->info.tsp_type == msgin.tsp_type) {
					if (trace)
						fprintf(fd, "\tdup ");
					goto again;
				}
			}
			msgcnt--;
			return(&msgin);
		} else {
			prev = ptr;
			ptr = ptr->p;
		}
	}

	/*
	 * If the message was not in the linked list, it may still be
	 * coming from the network. Set the timer and wait
	 * on a select to read the next incoming message: if it is the
	 * right one, return it, otherwise insert it in the linked list.
	 */

	(void)gettimeofday(&rtout, NULL);
	timevaladd(&rtout, intvl);
	FD_ZERO(&ready);
	for (;;) {
		(void)gettimeofday(&rtime, NULL);
		timevalsub(&rwait, &rtout, &rtime);
		if (rwait.tv_sec < 0)
			rwait.tv_sec = rwait.tv_usec = 0;
		else if (rwait.tv_sec == 0
			 && rwait.tv_usec < 1000000/CLK_TCK)
			rwait.tv_usec = 1000000/CLK_TCK;

		if (trace) {
			fprintf(fd, "readmsg: wait %jd.%6ld at %s\n",
				(intmax_t)rwait.tv_sec, rwait.tv_usec, date());
			/* Notice a full disk, as we flush trace info.
			 * It is better to flush periodically than at
			 * every line because the tracing consists of bursts
			 * of many lines.  Without care, tracing slows
			 * down the code enough to break the protocol.
			 */
			if (rwait.tv_sec != 0
			    && EOF == fflush(fd))
				traceoff("Tracing ended for cause at %s\n");
		}

		FD_SET(sock, &ready);
		if (!select(sock+1, &ready, (fd_set *)0, (fd_set *)0,
			   &rwait)) {
			if (rwait.tv_sec == 0 && rwait.tv_usec == 0)
				return(0);
			continue;
		}
		length = sizeof(from);
		if ((n = recvfrom(sock, (char *)&msgin, sizeof(struct tsp), 0,
			     (struct sockaddr*)&from, &length)) < 0) {
			syslog(LOG_ERR, "recvfrom: %m");
			exit(1);
		}
		/*
		 * The 4.3BSD protocol spec had a 32-byte tsp_name field, and
		 * this is still OS-dependent.  Demand that the packet is at
		 * least long enough to hold a 4.3BSD packet.
		 */
		if (n < (ssize_t)(sizeof(struct tsp) - MAXHOSTNAMELEN + 32)) {
			syslog(LOG_NOTICE,
			    "short packet (%zd/%zu bytes) from %s",
			      n, sizeof(struct tsp) - MAXHOSTNAMELEN + 32,
			      inet_ntoa(from.sin_addr));
			continue;
		}
		(void)gettimeofday(&from_when, NULL);
		bytehostorder(&msgin);

		if (msgin.tsp_vers > TSPVERSION) {
			if (trace) {
			    fprintf(fd,"readmsg: version mismatch\n");
			    /* should do a dump of the packet */
			}
			continue;
		}

		if (memchr(msgin.tsp_name,
		    '\0', sizeof msgin.tsp_name) == NULL) {
			syslog(LOG_NOTICE, "hostname field not NUL terminated "
			    "in packet from %s", inet_ntoa(from.sin_addr));
			continue;
		}

		fromnet = NULL;
		for (ntp = nettab; ntp != NULL; ntp = ntp->next)
			if ((ntp->mask & from.sin_addr.s_addr) ==
			    ntp->net.s_addr) {
				fromnet = ntp;
				break;
			}

		/*
		 * drop packets from nets we are ignoring permanently
		 */
		if (fromnet == NULL) {
			/*
			 * The following messages may originate on
			 * this host with an ignored network address
			 */
			if (msgin.tsp_type != TSP_TRACEON &&
			    msgin.tsp_type != TSP_SETDATE &&
			    msgin.tsp_type != TSP_MSITE &&
			    msgin.tsp_type != TSP_TEST &&
			    msgin.tsp_type != TSP_TRACEOFF) {
				if (trace) {
				    fprintf(fd,"readmsg: discard null net ");
				    print(&msgin, &from);
				}
				continue;
			}
		}

		/*
		 * Throw away messages coming from this machine,
		 * unless they are of some particular type.
		 * This gets rid of broadcast messages and reduces
		 * master processing time.
		 */
		if (!strcmp(msgin.tsp_name, hostname)
		    && msgin.tsp_type != TSP_SETDATE
		    && msgin.tsp_type != TSP_TEST
		    && msgin.tsp_type != TSP_MSITE
		    && msgin.tsp_type != TSP_TRACEON
		    && msgin.tsp_type != TSP_TRACEOFF
		    && msgin.tsp_type != TSP_LOOP) {
			if (trace) {
				fprintf(fd, "readmsg: discard own ");
				print(&msgin, &from);
			}
			continue;
		}

		/*
		 * Send acknowledgements here; this is faster and
		 * avoids deadlocks that would occur if acks were
		 * sent from a higher level routine.  Different
		 * acknowledgements are necessary, depending on
		 * status.
		 */
		if (fromnet == NULL)	/* do not de-reference 0 */
			ignoreack();
		else if (fromnet->status == MASTER)
			masterack();
		else if (fromnet->status == SLAVE)
			slaveack();
		else
			ignoreack();

		if (LOOKAT(msgin, type, machfrom, netfrom, from)) {
			if (trace) {
				fprintf(fd, "readmsg: ");
				print(&msgin, &from);
			}
			return(&msgin);
		} else if (++msgcnt > NHOSTS*3) {

/* The protocol gets hopelessly confused if it gets too far
*	behind.  However, it seems able to recover from all cases of lost
*	packets.  Therefore, if we are swamped, throw everything away.
*/
			if (trace)
				fprintf(fd,
					"readmsg: discarding %d msgs\n",
					msgcnt);
			msgcnt = 0;
			while ((ptr=head->p) != NULL) {
				head->p = ptr->p;
				free((char *)ptr);
			}
			tail = head;
		} else {
			tail->p = (struct tsplist *)
				    malloc(sizeof(struct tsplist));
			tail = tail->p;
			tail->p = NULL;
			tail->info = msgin;
			tail->addr = from;
			/* timestamp msgs so SETTIMEs are correct */
			tail->when = from_when;
		}
	}
}

/*
 * Send the necessary acknowledgements:
 * only the type ACK is to be sent by a slave
 */
void
slaveack(void)
{
	switch(msgin.tsp_type) {

	case TSP_ADJTIME:
	case TSP_SETTIME:
	case TSP_ACCEPT:
	case TSP_REFUSE:
	case TSP_TRACEON:
	case TSP_TRACEOFF:
	case TSP_QUIT:
		if (trace) {
			fprintf(fd, "Slaveack: ");
			print(&msgin, &from);
		}
		xmit(TSP_ACK,msgin.tsp_seq, &from);
		break;

	default:
		if (trace) {
			fprintf(fd, "Slaveack: no ack: ");
			print(&msgin, &from);
		}
		break;
	}
}

/*
 * Certain packets may arrive from this machine on ignored networks.
 * These packets should be acknowledged.
 */
void
ignoreack(void)
{
	switch(msgin.tsp_type) {

	case TSP_TRACEON:
	case TSP_TRACEOFF:
	case TSP_QUIT:
		if (trace) {
			fprintf(fd, "Ignoreack: ");
			print(&msgin, &from);
		}
		xmit(TSP_ACK,msgin.tsp_seq, &from);
		break;

	default:
		if (trace) {
			fprintf(fd, "Ignoreack: no ack: ");
			print(&msgin, &from);
		}
		break;
	}
}

/*
 * `masterack' sends the necessary acknowledgments
 * to the messages received by a master
 */
void
masterack(void)
{
	struct tsp resp;

	resp = msgin;
	resp.tsp_vers = TSPVERSION;
	(void)strcpy(resp.tsp_name, hostname);

	switch(msgin.tsp_type) {

	case TSP_QUIT:
	case TSP_TRACEON:
	case TSP_TRACEOFF:
	case TSP_MSITEREQ:
		if (trace) {
			fprintf(fd, "Masterack: ");
			print(&msgin, &from);
		}
		xmit(TSP_ACK,msgin.tsp_seq, &from);
		break;

	case TSP_RESOLVE:
	case TSP_MASTERREQ:
		if (trace) {
			fprintf(fd, "Masterack: ");
			print(&msgin, &from);
		}
		xmit(TSP_MASTERACK,msgin.tsp_seq, &from);
		break;

	default:
		if (trace) {
			fprintf(fd,"Masterack: no ack: ");
			print(&msgin, &from);
		}
		break;
	}
}

/*
 * Print a TSP message
 */
void
print(struct tsp *msg, struct sockaddr_in *addr)
{
	char tm[26];
	time_t tsp_time_sec;

	if (msg->tsp_type >= TSPTYPENUMBER) {
		fprintf(fd, "bad type (%u) on packet from %s\n",
		  msg->tsp_type, inet_ntoa(addr->sin_addr));
		return;
	}

	switch (msg->tsp_type) {

	case TSP_LOOP:
		fprintf(fd, "%s %d %-6u #%d %-15s %s\n",
			tsptype[msg->tsp_type],
			msg->tsp_vers,
			msg->tsp_seq,
			msg->tsp_hopcnt,
			inet_ntoa(addr->sin_addr),
			msg->tsp_name);
		break;

	case TSP_SETTIME:
	case TSP_SETDATE:
	case TSP_SETDATEREQ:
		tsp_time_sec = msg->tsp_time.tv_sec;
		strncpy(tm, ctime(&tsp_time_sec)+3+1, sizeof(tm));
		tm[15] = '\0';		/* ugh */
		fprintf(fd, "%s %d %-6u %s %-15s %s\n",
			tsptype[msg->tsp_type],
			msg->tsp_vers,
			msg->tsp_seq,
			tm,
			inet_ntoa(addr->sin_addr),
			msg->tsp_name);
		break;

	case TSP_ADJTIME:
		fprintf(fd, "%s %d %-6u (%d,%d) %-15s %s\n",
			tsptype[msg->tsp_type],
			msg->tsp_vers,
			msg->tsp_seq,
			msg->tsp_time.tv_sec,
			msg->tsp_time.tv_usec,
			inet_ntoa(addr->sin_addr),
			msg->tsp_name);
		break;

	default:
		fprintf(fd, "%s %d %-6u %-15s %s\n",
			tsptype[msg->tsp_type],
			msg->tsp_vers,
			msg->tsp_seq,
			inet_ntoa(addr->sin_addr),
			msg->tsp_name);
		break;
	}
}
