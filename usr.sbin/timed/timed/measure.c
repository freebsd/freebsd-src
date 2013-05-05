/*-
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
#if 0
static char sccsid[] = "@(#)measure.c	8.1 (Berkeley) 6/6/93";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include "globals.h"
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>

#define MSEC_DAY	(SECDAY*1000)

#define PACKET_IN	1024

#define MSGS		5		/* timestamps to average */
#define TRIALS		10		/* max # of timestamps sent */

extern int sock_raw;

int measure_delta;

static n_short seqno = 0;

/*
 * Measures the differences between machines' clocks using
 * ICMP timestamp messages.
 * maxmsec	wait this many msec at most
 * wmsec	msec to wait for an answer
 * print	print complaints on stderr
 */
int					/* status val defined in globals.h */
measure(u_long maxmsec, u_long wmsec, char *hname, struct sockaddr_in *addr, int print)
{
	int length;
	int measure_status;
	int rcvcount, trials;
	int cc, count;
	fd_set ready;
	long sendtime, recvtime, histime1, histime2;
	long idelta, odelta, total;
	long min_idelta, min_odelta;
	struct timeval tdone, tcur, ttrans, twait, tout;
	u_char packet[PACKET_IN], opacket[64];
	register struct icmp *icp = (struct icmp *) packet;
	register struct icmp *oicp = (struct icmp *) opacket;
	struct ip *ip = (struct ip *) packet;

	min_idelta = min_odelta = 0x7fffffff;
	measure_status = HOSTDOWN;
	measure_delta = HOSTDOWN;
	trials = 0;
	errno = 0;

	/* open raw socket used to measure time differences */
	if (sock_raw < 0) {
		sock_raw = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
		if (sock_raw < 0)  {
			syslog(LOG_ERR, "opening raw socket: %m");
			goto quit;
		}
	}


	/*
	 * empty the icmp input queue
	 */
	FD_ZERO(&ready);
	for (;;) {
		tout.tv_sec = tout.tv_usec = 0;
		FD_SET(sock_raw, &ready);
		if (select(sock_raw+1, &ready, 0,0, &tout)) {
			length = sizeof(struct sockaddr_in);
			cc = recvfrom(sock_raw, (char *)packet, PACKET_IN, 0,
				      0,&length);
			if (cc < 0)
				goto quit;
			continue;
		}
		break;
	}

	/*
	 * Choose the smallest transmission time in each of the two
	 * directions. Use these two latter quantities to compute the delta
	 * between the two clocks.
	 */

	oicp->icmp_type = ICMP_TSTAMP;
	oicp->icmp_code = 0;
	oicp->icmp_id = getpid();
	oicp->icmp_rtime = 0;
	oicp->icmp_ttime = 0;
	oicp->icmp_seq = seqno;

	FD_ZERO(&ready);

	(void)gettimeofday(&tdone, NULL);
	mstotvround(&tout, maxmsec);
	timevaladd(&tdone, &tout);		/* when we give up */

	mstotvround(&twait, wmsec);

	rcvcount = 0;
	while (rcvcount < MSGS) {
		(void)gettimeofday(&tcur, NULL);

		/*
		 * keep sending until we have sent the max
		 */
		if (trials < TRIALS) {
			trials++;
			oicp->icmp_otime = htonl((tcur.tv_sec % SECDAY) * 1000
						 + tcur.tv_usec / 1000);
			oicp->icmp_cksum = 0;
			oicp->icmp_cksum = in_cksum((u_short*)oicp,
						    sizeof(*oicp));

			count = sendto(sock_raw, opacket, sizeof(*oicp), 0,
				       (struct sockaddr*)addr,
				       sizeof(struct sockaddr));
			if (count < 0) {
				if (measure_status == HOSTDOWN)
					measure_status = UNREACHABLE;
				goto quit;
			}
			++oicp->icmp_seq;

			ttrans = tcur;
			timevaladd(&ttrans, &twait);
		} else {
			ttrans = tdone;
		}

		while (rcvcount < trials) {
			timevalsub(&tout, &ttrans, &tcur);
			if (tout.tv_sec < 0)
				tout.tv_sec = 0;

			FD_SET(sock_raw, &ready);
			count = select(sock_raw+1, &ready, (fd_set *)0,
				       (fd_set *)0, &tout);
			(void)gettimeofday(&tcur, NULL);
			if (count <= 0)
				break;

			length = sizeof(struct sockaddr_in);
			cc = recvfrom(sock_raw, (char *)packet, PACKET_IN, 0,
				      0,&length);
			if (cc < 0)
				goto quit;

			/*
			 * got something.  See if it is ours
			 */
			icp = (struct icmp *)(packet + (ip->ip_hl << 2));
			if (cc < sizeof(*ip)
			    || icp->icmp_type != ICMP_TSTAMPREPLY
			    || icp->icmp_id != oicp->icmp_id
			    || icp->icmp_seq < seqno
			    || icp->icmp_seq >= oicp->icmp_seq)
				continue;


			sendtime = ntohl(icp->icmp_otime);
			recvtime = ((tcur.tv_sec % SECDAY) * 1000 +
				    tcur.tv_usec / 1000);

			total = recvtime-sendtime;
			if (total < 0)	/* do not hassle midnight */
				continue;

			rcvcount++;
			histime1 = ntohl(icp->icmp_rtime);
			histime2 = ntohl(icp->icmp_ttime);
			/*
			 * a host using a time format different from
			 * msec. since midnight UT (as per RFC792) should
			 * set the high order bit of the 32-bit time
			 * value it transmits.
			 */
			if ((histime1 & 0x80000000) != 0) {
				measure_status = NONSTDTIME;
				goto quit;
			}
			measure_status = GOOD;

			idelta = recvtime-histime2;
			odelta = histime1-sendtime;

			/* do not be confused by midnight */
			if (idelta < -MSEC_DAY/2) idelta += MSEC_DAY;
			else if (idelta > MSEC_DAY/2) idelta -= MSEC_DAY;

			if (odelta < -MSEC_DAY/2) odelta += MSEC_DAY;
			else if (odelta > MSEC_DAY/2) odelta -= MSEC_DAY;

			/* save the quantization error so that we can get a
			 * measurement finer than our system clock.
			 */
			if (total < MIN_ROUND) {
				measure_delta = (odelta - idelta)/2;
				goto quit;
			}

			if (idelta < min_idelta)
				min_idelta = idelta;
			if (odelta < min_odelta)
				min_odelta = odelta;

			measure_delta = (min_odelta - min_idelta)/2;
		}

		if (tcur.tv_sec > tdone.tv_sec
		    || (tcur.tv_sec == tdone.tv_sec
			&& tcur.tv_usec >= tdone.tv_usec))
			break;
	}

quit:
	seqno += TRIALS;		/* allocate our sequence numbers */

	/*
	 * If no answer is received for TRIALS consecutive times,
	 * the machine is assumed to be down
	 */
	if (measure_status == GOOD) {
		if (trace) {
			fprintf(fd,
				"measured delta %4d, %d trials to %-15s %s\n",
			   	measure_delta, trials,
				inet_ntoa(addr->sin_addr), hname);
		}
	} else if (print) {
		if (errno != 0)
			warn("measure %s", hname);
	} else {
		if (errno != 0) {
			syslog(LOG_ERR, "measure %s: %m", hname);
		} else {
			syslog(LOG_ERR, "measure: %s did not respond", hname);
		}
		if (trace) {
			fprintf(fd,
				"measure: %s failed after %d trials\n",
				hname, trials);
			(void)fflush(fd);
		}
	}

	return(measure_status);
}





/*
 * round a number of milliseconds into a struct timeval
 */
void
mstotvround(struct timeval *res, long x)
{
	if (x < 0)
		x = -((-x + 3)/5);
	else
		x = (x+3)/5;
	x *= 5;
	res->tv_sec = x/1000;
	res->tv_usec = (x-res->tv_sec*1000)*1000;
	if (res->tv_usec < 0) {
		res->tv_usec += 1000000;
		res->tv_sec--;
	}
}

void
timevaladd(struct timeval *tv1, struct timeval *tv2)
{
	tv1->tv_sec += tv2->tv_sec;
	tv1->tv_usec += tv2->tv_usec;
	if (tv1->tv_usec >= 1000000) {
		tv1->tv_sec++;
		tv1->tv_usec -= 1000000;
	}
	if (tv1->tv_usec < 0) {
		tv1->tv_sec--;
		tv1->tv_usec += 1000000;
	}
}

void
timevalsub(struct timeval *res, struct timeval *tv1, struct timeval *tv2)
{
	res->tv_sec = tv1->tv_sec - tv2->tv_sec;
	res->tv_usec = tv1->tv_usec - tv2->tv_usec;
	if (res->tv_usec >= 1000000) {
		res->tv_sec++;
		res->tv_usec -= 1000000;
	}
	if (res->tv_usec < 0) {
		res->tv_sec--;
		res->tv_usec += 1000000;
	}
}
