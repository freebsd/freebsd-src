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
static char sccsid[] = "@(#)correct.c	8.1 (Berkeley) 6/6/93";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include "globals.h"
#include <math.h>
#include <sys/types.h>
#include <sys/times.h>

static void adjclock(struct timeval *);

/*
 * sends to the slaves the corrections for their clocks after fixing our
 * own
 */
void
correct(long avdelta)
{
	struct hosttbl *htp;
	int corr;
	struct timeval adjlocal, tmptv;
	struct tsp to;
	struct tsp *answer;

	mstotvround(&adjlocal, avdelta);

	for (htp = self.l_fwd; htp != &self; htp = htp->l_fwd) {
		if (htp->delta != HOSTDOWN)  {
			corr = avdelta - htp->delta;
/* If the other machine is off in the weeds, set its time directly.
 *	If a slave gets the wrong day, the original code would simply
 *	fix the minutes.  If you fix a network partition, you can get
 *	into such situations.
 */
			if (htp->need_set
			    || corr >= MAXADJ*1000
			    || corr <= -MAXADJ*1000) {
				htp->need_set = 0;
				(void)gettimeofday(&tmptv,0);
				timevaladd(&tmptv, &adjlocal);
				to.tsp_time.tv_sec = tmptv.tv_sec;
				to.tsp_time.tv_usec = tmptv.tv_usec;
				to.tsp_type = TSP_SETTIME;
			} else {
				tmptv.tv_sec = to.tsp_time.tv_sec;
				tmptv.tv_usec = to.tsp_time.tv_usec;
				mstotvround(&tmptv, corr);
				to.tsp_time.tv_sec = tmptv.tv_sec;
				to.tsp_time.tv_usec = tmptv.tv_usec;
				to.tsp_type = TSP_ADJTIME;
			}
			(void)strcpy(to.tsp_name, hostname);
			answer = acksend(&to, &htp->addr, htp->name,
					 TSP_ACK, 0, 0);
			if (!answer) {
				htp->delta = HOSTDOWN;
				syslog(LOG_WARNING,
				       "no reply to time correction from %s",
				       htp->name);
				if (++htp->noanswer >= LOSTHOST) {
					if (trace) {
						fprintf(fd,
					     "purging %s for not answering\n",
							htp->name);
						(void)fflush(fd);
					}
					htp = remmach(htp);
				}
			}
		}
	}

	/*
	 * adjust our own clock now that we are not sending it out
	 */
	adjclock(&adjlocal);
}


static void
adjclock(struct timeval *corr)
{
	static int passes = 0;
	static int smoother = 0;
	long delta;			/* adjustment in usec */
	long ndelta;
	struct timeval now;
	struct timeval adj;

	if (!timerisset(corr))
		return;

	adj = *corr;
	if (adj.tv_sec < MAXADJ && adj.tv_sec > - MAXADJ) {
		delta = adj.tv_sec*1000000 + adj.tv_usec;
		/* If the correction is less than the minimum round
		 *	trip time for an ICMP packet, and thus
		 *	less than the likely error in the measurement,
		 *	do not do the entire correction.  Do half
		 *	or a quarter of it.
		 */

		if (delta > -MIN_ROUND*1000
		    && delta < MIN_ROUND*1000) {
			if (smoother <= 4)
				smoother++;
			ndelta = delta >> smoother;
			if (trace)
				fprintf(fd,
					"trimming delta %ld usec to %ld\n",
					delta, ndelta);
			adj.tv_usec = ndelta;
			adj.tv_sec = 0;
		} else if (smoother > 0) {
			smoother--;
		}
		if (0 > adjtime(corr, 0)) {
			syslog(LOG_ERR, "adjtime: %m");
		}
		if (passes > 1
		    && (delta < -BIG_ADJ || delta > BIG_ADJ)) {
			smoother = 0;
			passes = 0;
			syslog(LOG_WARNING,
			       "large time adjustment of %+.3f sec",
			       delta/1000000.0);
		}
	} else {
		syslog(LOG_WARNING,
		       "clock correction %jd sec too large to adjust",
		       (intmax_t)adj.tv_sec);
		(void) gettimeofday(&now, 0);
		timevaladd(&now, corr);
		if (settimeofday(&now, 0) < 0)
			syslog(LOG_ERR, "settimeofday: %m");
	}
}


/* adjust the time in a message by the time it
 *	spent in the queue
 */
void
adj_msg_time(struct tsp *msg, struct timeval *now)
{
	msg->tsp_time.tv_sec += (now->tv_sec - from_when.tv_sec);
	msg->tsp_time.tv_usec += (now->tv_usec - from_when.tv_usec);

	while (msg->tsp_time.tv_usec < 0) {
		msg->tsp_time.tv_sec--;
		msg->tsp_time.tv_usec += 1000000;
	}
	while (msg->tsp_time.tv_usec >= 1000000) {
		msg->tsp_time.tv_sec++;
		msg->tsp_time.tv_usec -= 1000000;
	}
}
