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
#ifdef sgi
#include <sys/syssgi.h>
#endif /* sgi */

static void adjclock __P((struct timeval *));

/*
 * sends to the slaves the corrections for their clocks after fixing our
 * own
 */
void
correct(avdelta)
	long avdelta;
{
	struct hosttbl *htp;
	int corr;
	struct timeval adjlocal;
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
				(void)gettimeofday(&to.tsp_time,0);
				timevaladd(&to.tsp_time, &adjlocal);
				to.tsp_type = TSP_SETTIME;
			} else {
				mstotvround(&to.tsp_time, corr);
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
adjclock(corr)
	struct timeval *corr;
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
		       "clock correction %d sec too large to adjust",
		       adj.tv_sec);
		(void) gettimeofday(&now, 0);
		timevaladd(&now, corr);
		if (settimeofday(&now, 0) < 0)
			syslog(LOG_ERR, "settimeofday: %m");
	}

#ifdef sgi
	/* Accumulate the total change, and use it to adjust the basic
	 * clock rate.
	 */
	if (++passes > 2) {
#define F_USEC_PER_SEC	(1000000*1.0)	/* reduce typos */
#define F_NSEC_PER_SEC	(F_USEC_PER_SEC*1000.0)

		extern char *timetrim_fn;
		extern char *timetrim_wpat;
		extern long timetrim;
		extern double tot_adj, hr_adj;	/* totals in nsec */
		extern double tot_ticks, hr_ticks;

		static double nag_tick;
		double cur_ticks, hr_delta_ticks, tot_delta_ticks;
		double tru_tot_adj, tru_hr_adj; /* nsecs of adjustment */
		double tot_trim, hr_trim;   /* nsec/sec */
		struct tms tm;
		FILE *timetrim_st;

		cur_ticks = times(&tm);
		tot_adj += delta*1000.0;
		hr_adj += delta*1000.0;

		tot_delta_ticks = cur_ticks-tot_ticks;
		if (tot_delta_ticks >= 16*SECDAY*CLK_TCK) {
			tot_adj -= rint(tot_adj/16);
			tot_ticks += rint(tot_delta_ticks/16);
			tot_delta_ticks = cur_ticks-tot_ticks;
		}
		hr_delta_ticks = cur_ticks-hr_ticks;

		tru_hr_adj = hr_adj + timetrim*rint(hr_delta_ticks/CLK_TCK);
		tru_tot_adj = (tot_adj
			       + timetrim*rint(tot_delta_ticks/CLK_TCK));

		if (hr_delta_ticks >= SECDAY*CLK_TCK
		    || (tot_delta_ticks < 4*SECDAY*CLK_TCK
			&& hr_delta_ticks >= SECHR*CLK_TCK)
		    || (trace && hr_delta_ticks >= (SECHR/10)*CLK_TCK)) {

			tot_trim = rint(tru_tot_adj*CLK_TCK/tot_delta_ticks);
			hr_trim = rint(tru_hr_adj*CLK_TCK/hr_delta_ticks);

			if (trace
			    || (abs(timetrim - hr_trim) > 100000.0
				&& 0 == timetrim_fn
				&& ((cur_ticks - nag_tick)
				    >= 24*SECDAY*CLK_TCK))) {
				nag_tick = cur_ticks;
				syslog(LOG_NOTICE,
		   "%+.3f/%.2f or %+.3f/%.2f sec/hr; timetrim=%+.0f or %+.0f",
				       tru_tot_adj/F_NSEC_PER_SEC,
				       tot_delta_ticks/(SECHR*CLK_TCK*1.0),
				       tru_hr_adj/F_NSEC_PER_SEC,
				       hr_delta_ticks/(SECHR*CLK_TCK*1.0),
				       tot_trim,
				       hr_trim);
			}

			if (tot_trim < -MAX_TRIM || tot_trim > MAX_TRIM) {
				tot_ticks = hr_ticks;
				tot_adj = hr_adj;
			} else if (0 > syssgi(SGI_SETTIMETRIM,
					      (long)tot_trim)) {
				syslog(LOG_ERR, "SETTIMETRIM(%d): %m",
				       (long)tot_trim);
			} else {
				if (0 != timetrim_fn) {
				    timetrim_st = fopen(timetrim_fn, "w");
				    if (0 == timetrim_st) {
					syslog(LOG_ERR, "fopen(%s): %m",
					       timetrim_fn);
				    } else {
					if (0 > fprintf(timetrim_st,
							timetrim_wpat,
							(long)tot_trim,
							tru_tot_adj,
							tot_delta_ticks)) {
						syslog(LOG_ERR,
						       "fprintf(%s): %m",
						       timetrim_fn);
					}
					(void)fclose(timetrim_st);
				    }
				}

				tot_adj -= ((tot_trim - timetrim)
					    * rint(tot_delta_ticks/CLK_TCK));
				timetrim = tot_trim;
			}

			hr_ticks = cur_ticks;
			hr_adj = 0;
		}
	}
#endif /* sgi */
}


/* adjust the time in a message by the time it
 *	spent in the queue
 */
void
adj_msg_time(msg, now)
	struct tsp *msg;
	struct timeval *now;
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
