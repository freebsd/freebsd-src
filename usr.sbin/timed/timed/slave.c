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
static char sccsid[] = "@(#)slave.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

#ifdef sgi
#ident "$Revision: 1.1.1.1 $"
#endif

#include "globals.h"
#include <setjmp.h>
#include "pathnames.h"

extern jmp_buf jmpenv;
extern int Mflag;
extern int justquit;

extern u_short sequence;

static char master_name[MAXHOSTNAMELEN+1];
static struct netinfo *old_slavenet;
static int old_status;

static void schgdate __P((struct tsp *, char *));
static void setmaster __P((struct tsp *));
static void answerdelay __P((void));

#ifdef sgi
extern void logwtmp __P((struct timeval *, struct timeval *));
#else
extern void logwtmp __P((char *, char *, char *));
#endif /* sgi */

int
slave()
{
	int tries;
	long electiontime, refusetime, looktime, looptime, adjtime;
	u_short seq;
	long fastelection;
#define FASTTOUT 3
	struct in_addr cadr;
	struct timeval otime;
	struct sockaddr_in taddr;
	char tname[MAXHOSTNAMELEN];
	struct tsp *msg, to;
	struct timeval ntime, wait;
	struct tsp *answer;
	int timeout();
	char olddate[32];
	char newdate[32];
	struct netinfo *ntp;
	struct hosttbl *htp;


	old_slavenet = 0;
	seq = 0;
	refusetime = 0;
	adjtime = 0;

	(void)gettimeofday(&ntime, 0);
	electiontime = ntime.tv_sec + delay2;
	fastelection = ntime.tv_sec + FASTTOUT;
	if (justquit)
		looktime = electiontime;
	else
		looktime = fastelection;
	looptime = fastelection;

	if (slavenet)
		xmit(TSP_SLAVEUP, 0, &slavenet->dest_addr);
	if (status & MASTER) {
		for (ntp = nettab; ntp != NULL; ntp = ntp->next) {
			if (ntp->status == MASTER)
				masterup(ntp);
		}
	}

loop:
	get_goodgroup(0);
	(void)gettimeofday(&ntime, (struct timezone *)0);
	if (ntime.tv_sec > electiontime) {
		if (trace)
			fprintf(fd, "election timer expired\n");
		longjmp(jmpenv, 1);
	}

	if (ntime.tv_sec >= looktime) {
		if (trace)
			fprintf(fd, "Looking for nets to master\n");

		if (Mflag && nignorednets > 0) {
			for (ntp = nettab; ntp != NULL; ntp = ntp->next) {
				if (ntp->status == IGNORE
				    || ntp->status == NOMASTER) {
					lookformaster(ntp);
					if (ntp->status == MASTER) {
						masterup(ntp);
					} else if (ntp->status == MASTER) {
						ntp->status = NOMASTER;
					}
				}
				if (ntp->status == MASTER
				    && --ntp->quit_count < 0)
					ntp->quit_count = 0;
			}
			makeslave(slavenet);	/* prune extras */
			setstatus();
		}
		(void)gettimeofday(&ntime, 0);
		looktime = ntime.tv_sec + delay2;
	}
	if (ntime.tv_sec >= looptime) {
		if (trace)
			fprintf(fd, "Looking for loops\n");
		for (ntp = nettab; ntp != NULL; ntp = ntp->next) {
		    if (ntp->status == MASTER) {
			to.tsp_type = TSP_LOOP;
			to.tsp_vers = TSPVERSION;
			to.tsp_seq = sequence++;
			to.tsp_hopcnt = MAX_HOPCNT;
			(void)strcpy(to.tsp_name, hostname);
			bytenetorder(&to);
			if (sendto(sock, (char *)&to, sizeof(struct tsp), 0,
				   (struct sockaddr*)&ntp->dest_addr,
				   sizeof(ntp->dest_addr)) < 0) {
				trace_sendto_err(ntp->dest_addr.sin_addr);
			}
		    }
		}
		(void)gettimeofday(&ntime, 0);
		looptime = ntime.tv_sec + delay2;
	}

	wait.tv_sec = min(electiontime,min(looktime,looptime)) - ntime.tv_sec;
	if (wait.tv_sec < 0)
		wait.tv_sec = 0;
	wait.tv_sec += FASTTOUT;
	wait.tv_usec = 0;
	msg = readmsg(TSP_ANY, ANYADDR, &wait, 0);

	if (msg != NULL) {
		/*
		 * filter stuff not for us
		 */
		switch (msg->tsp_type) {
		case TSP_SETDATE:
		case TSP_TRACEOFF:
		case TSP_TRACEON:
			/*
			 * XXX check to see they are from ourself
			 */
			break;

		case TSP_TEST:
		case TSP_MSITE:
			break;

		case TSP_MASTERUP:
			if (!fromnet) {
				if (trace) {
					fprintf(fd, "slave ignored: ");
					print(msg, &from);
				}
				goto loop;
			}
			break;

		default:
			if (!fromnet
			    || fromnet->status == IGNORE
			    || fromnet->status == NOMASTER) {
				if (trace) {
					fprintf(fd, "slave ignored: ");
					print(msg, &from);
				}
				goto loop;
			}
			break;
		}


		/*
		 * now process the message
		 */
		switch (msg->tsp_type) {

		case TSP_ADJTIME:
			if (fromnet != slavenet)
				break;
			if (!good_host_name(msg->tsp_name)) {
				syslog(LOG_NOTICE,
				   "attempted time adjustment by %s",
				       msg->tsp_name);
				suppress(&from, msg->tsp_name, fromnet);
				break;
			}
			/*
			 * Speed up loop detection in case we have a loop.
			 * Otherwise the clocks can race until the loop
			 * is found.
			 */
			(void)gettimeofday(&otime, 0);
			if (adjtime < otime.tv_sec)
				looptime -= (looptime-otime.tv_sec)/2 + 1;

			setmaster(msg);
			if (seq != msg->tsp_seq) {
				seq = msg->tsp_seq;
				synch(tvtomsround(msg->tsp_time));
			}
			(void)gettimeofday(&ntime, 0);
			electiontime = ntime.tv_sec + delay2;
			fastelection = ntime.tv_sec + FASTTOUT;
			adjtime = ntime.tv_sec + SAMPLEINTVL*2;
			break;

		case TSP_SETTIME:
			if (fromnet != slavenet)
				break;
			if (seq == msg->tsp_seq)
				break;
			seq = msg->tsp_seq;

			/* adjust time for residence on the queue */
			(void)gettimeofday(&otime, 0);
			adj_msg_time(msg,&otime);
#ifdef sgi
			(void)cftime(newdate, "%D %T", &msg->tsp_time.tv_sec);
			(void)cftime(olddate, "%D %T", &otime.tv_sec);
#else
			/*
			 * the following line is necessary due to syslog
			 * calling ctime() which clobbers the static buffer
			 */
			(void)strcpy(olddate, date());
			(void)strcpy(newdate, ctime(&msg->tsp_time.tv_sec));
#endif /* sgi */

			if (!good_host_name(msg->tsp_name)) {
				syslog(LOG_NOTICE,
			    "attempted time setting by untrusted %s to %s",
				       msg->tsp_name, newdate);
				suppress(&from, msg->tsp_name, fromnet);
				break;
			}

			setmaster(msg);
			timevalsub(&ntime, &msg->tsp_time, &otime);
			if (ntime.tv_sec < MAXADJ && ntime.tv_sec > -MAXADJ) {
				/*
				 * do not change the clock if we can adjust it
				 */
				synch(tvtomsround(ntime));
			} else {
#ifdef sgi
				if (0 > settimeofday(&msg->tsp_time, 0)) {
					syslog(LOG_ERR,"settimeofdate(): %m");
					break;
				}
				logwtmp(&otime, &msg->tsp_time);
#else
				logwtmp("|", "date", "");
				(void)settimeofday(&msg->tsp_time, 0);
				logwtmp("{", "date", "");
#endif /* sgi */
				syslog(LOG_NOTICE,
				       "date changed by %s from %s",
					msg->tsp_name, olddate);
				if (status & MASTER)
					spreadtime();
			}
			(void)gettimeofday(&ntime, 0);
			electiontime = ntime.tv_sec + delay2;
			fastelection = ntime.tv_sec + FASTTOUT;

/* This patches a bad protocol bug.  Imagine a system with several networks,
 * where there are a pair of redundant gateways between a pair of networks,
 * each running timed.  Assume that we start with a third machine mastering
 * one of the networks, and one of the gateways mastering the other.
 * Imagine that the third machine goes away and the non-master gateway
 * decides to replace it.  If things are timed just 'right,' we will have
 * each gateway mastering one network for a little while.  If a SETTIME
 * message gets into the network at that time, perhaps from the newly
 * masterful gateway as it was taking control, the SETTIME will loop
 * forever.  Each time a gateway receives it on its slave side, it will
 * call spreadtime to forward it on its mastered network.  We are now in
 * a permanent loop, since the SETTIME msgs will keep any clock
 * in the network from advancing.  Normally, the 'LOOP' stuff will detect
 * and correct the situation.  However, with the clocks stopped, the
 * 'looptime' timer cannot expire.  While they are in this state, the
 * masters will try to saturate the network with SETTIME packets.
 */
			looptime = ntime.tv_sec + (looptime-otime.tv_sec)/2-1;
			break;

		case TSP_MASTERUP:
			if (slavenet && fromnet != slavenet)
				break;
			if (!good_host_name(msg->tsp_name)) {
				suppress(&from, msg->tsp_name, fromnet);
				if (electiontime > fastelection)
					electiontime = fastelection;
				break;
			}
			makeslave(fromnet);
			setmaster(msg);
			setstatus();
			answerdelay();
			xmit(TSP_SLAVEUP, 0, &from);
			(void)gettimeofday(&ntime, 0);
			electiontime = ntime.tv_sec + delay2;
			fastelection = ntime.tv_sec + FASTTOUT;
			refusetime = 0;
			break;

		case TSP_MASTERREQ:
			if (fromnet->status != SLAVE)
				break;
			(void)gettimeofday(&ntime, 0);
			electiontime = ntime.tv_sec + delay2;
			break;

		case TSP_SETDATE:
#ifdef sgi
			(void)cftime(newdate, "%D %T", &msg->tsp_time.tv_sec);
#else
			(void)strcpy(newdate, ctime(&msg->tsp_time.tv_sec));
#endif /* sgi */
			schgdate(msg, newdate);
			break;

		case TSP_SETDATEREQ:
			if (fromnet->status != MASTER)
				break;
#ifdef sgi
			(void)cftime(newdate, "%D %T", &msg->tsp_time.tv_sec);
#else
			(void)strcpy(newdate, ctime(&msg->tsp_time.tv_sec));
#endif /* sgi */
			htp = findhost(msg->tsp_name);
			if (0 == htp) {
				syslog(LOG_WARNING,
				       "DATEREQ from uncontrolled machine");
				break;
			}
			if (!htp->good) {
				syslog(LOG_WARNING,
				"attempted date change by untrusted %s to %s",
				       htp->name, newdate);
				spreadtime();
				break;
			}
			schgdate(msg, newdate);
			break;

		case TSP_TRACEON:
			traceon();
			break;

		case TSP_TRACEOFF:
			traceoff("Tracing ended at %s\n");
			break;

		case TSP_SLAVEUP:
			newslave(msg);
			break;

		case TSP_ELECTION:
			if (fromnet->status == SLAVE) {
				(void)gettimeofday(&ntime, 0);
				electiontime = ntime.tv_sec + delay2;
				fastelection = ntime.tv_sec + FASTTOUT;
				seq = 0;
				if (!good_host_name(msg->tsp_name)) {
					syslog(LOG_NOTICE,
					       "suppress election of %s",
					       msg->tsp_name);
					to.tsp_type = TSP_QUIT;
					electiontime = fastelection;
				} else if (cadr.s_addr != from.sin_addr.s_addr
					   && ntime.tv_sec < refusetime) {
/* if the candidate has to repeat itself, the old code would refuse it
 * the second time.  That would prevent elections.
 */
					to.tsp_type = TSP_REFUSE;
				} else {
					cadr.s_addr = from.sin_addr.s_addr;
					to.tsp_type = TSP_ACCEPT;
					refusetime = ntime.tv_sec + 30;
				}
				taddr = from;
				(void)strcpy(tname, msg->tsp_name);
				(void)strcpy(to.tsp_name, hostname);
				answerdelay();
				if (!acksend(&to, &taddr, tname,
					     TSP_ACK, 0, 0))
					syslog(LOG_WARNING,
					     "no answer from candidate %s\n",
					       tname);

			} else {	/* fromnet->status == MASTER */
				htp = addmach(msg->tsp_name, &from,fromnet);
				to.tsp_type = TSP_QUIT;
				(void)strcpy(to.tsp_name, hostname);
				if (!acksend(&to, &htp->addr, htp->name,
					     TSP_ACK, 0, htp->noanswer)) {
					syslog(LOG_ERR,
					  "no reply from %s to ELECTION-QUIT",
					       htp->name);
					(void)remmach(htp);
				}
			}
			break;

		case TSP_CONFLICT:
			if (fromnet->status != MASTER)
				break;
			/*
			 * After a network partition, there can be
			 * more than one master: the first slave to
			 * come up will notify here the situation.
			 */
			(void)strcpy(to.tsp_name, hostname);

			/* The other master often gets into the same state,
			 * with boring results.
			 */
			ntp = fromnet;	/* (acksend() can leave fromnet=0 */
			for (tries = 0; tries < 3; tries++) {
				to.tsp_type = TSP_RESOLVE;
				answer = acksend(&to, &ntp->dest_addr,
						 ANYADDR, TSP_MASTERACK,
						 ntp, 0);
				if (answer == NULL)
					break;
				htp = addmach(answer->tsp_name,&from,ntp);
				to.tsp_type = TSP_QUIT;
				answer = acksend(&to, &htp->addr, htp->name,
						 TSP_ACK, 0, htp->noanswer);
				if (!answer) {
					syslog(LOG_WARNING,
				  "conflict error: no reply from %s to QUIT",
						htp->name);
					(void)remmach(htp);
				}
			}
			masterup(ntp);
			break;

		case TSP_MSITE:
			if (!slavenet)
				break;
			taddr = from;
			to.tsp_type = TSP_MSITEREQ;
			to.tsp_vers = TSPVERSION;
			to.tsp_seq = 0;
			(void)strcpy(to.tsp_name, hostname);
			answer = acksend(&to, &slavenet->dest_addr,
					 ANYADDR, TSP_ACK,
					 slavenet, 0);
			if (answer != NULL
			    && good_host_name(answer->tsp_name)) {
				setmaster(answer);
				to.tsp_type = TSP_ACK;
				(void)strcpy(to.tsp_name, answer->tsp_name);
				bytenetorder(&to);
				if (sendto(sock, (char *)&to,
					   sizeof(struct tsp), 0,
					   (struct sockaddr*)&taddr, sizeof(taddr)) < 0) {
					trace_sendto_err(taddr.sin_addr);
				}
			}
			break;

		case TSP_MSITEREQ:
			break;

		case TSP_ACCEPT:
		case TSP_REFUSE:
		case TSP_RESOLVE:
			break;

		case TSP_QUIT:
			doquit(msg);		/* become a slave */
			break;

		case TSP_TEST:
			electiontime = 0;
			break;

		case TSP_LOOP:
			/* looking for loops of masters */
			if (!(status & MASTER))
				break;
			if (fromnet->status == SLAVE) {
			    if (!strcmp(msg->tsp_name, hostname)) {
				/*
				 * Someone forwarded our message back to
				 * us.  There must be a loop.  Tell the
				 * master of this network to quit.
				 *
				 * The other master often gets into
				 * the same state, with boring results.
				 */
				ntp = fromnet;
				for (tries = 0; tries < 3; tries++) {
				    to.tsp_type = TSP_RESOLVE;
				    answer = acksend(&to, &ntp->dest_addr,
						     ANYADDR, TSP_MASTERACK,
						     ntp,0);
				    if (answer == NULL)
					break;
				    taddr = from;
				    (void)strcpy(tname, answer->tsp_name);
				    to.tsp_type = TSP_QUIT;
				    (void)strcpy(to.tsp_name, hostname);
				    if (!acksend(&to, &taddr, tname,
						 TSP_ACK, 0, 1)) {
					syslog(LOG_ERR,
					"no reply from %s to slave LOOP-QUIT",
						 tname);
				    } else {
					electiontime = 0;
				    }
				}
				(void)gettimeofday(&ntime, 0);
				looptime = ntime.tv_sec + FASTTOUT;
			    } else {
				if (msg->tsp_hopcnt-- < 1)
				    break;
				bytenetorder(msg);
				for (ntp = nettab; ntp != 0; ntp = ntp->next) {
				    if (ntp->status == MASTER
					&& 0 > sendto(sock, (char *)msg,
						      sizeof(struct tsp), 0,
					      (struct sockaddr*)&ntp->dest_addr,
						      sizeof(ntp->dest_addr)))
				    trace_sendto_err(ntp->dest_addr.sin_addr);
				}
			    }
			} else {	/* fromnet->status == MASTER */
			    /*
			     * We should not have received this from a net
			     * we are master on.  There must be two masters,
			     * unless the packet was really from us.
			     */
			    if (from.sin_addr.s_addr
				== fromnet->my_addr.s_addr) {
				if (trace)
				    fprintf(fd,"discarding forwarded LOOP\n");
				break;
			    }

			    /*
			     * The other master often gets into the same
			     * state, with boring results.
			     */
			    ntp = fromnet;
			    for (tries = 0; tries < 3; tries++) {
				to.tsp_type = TSP_RESOLVE;
				answer = acksend(&to, &ntp->dest_addr,
						 ANYADDR, TSP_MASTERACK,
						ntp,0);
				if (!answer)
					break;
				htp = addmach(answer->tsp_name,
					      &from,ntp);
				to.tsp_type = TSP_QUIT;
				(void)strcpy(to.tsp_name, hostname);
				if (!acksend(&to,&htp->addr,htp->name,
					     TSP_ACK, 0, htp->noanswer)) {
					syslog(LOG_ERR,
				    "no reply from %s to master LOOP-QUIT",
					       htp->name);
					(void)remmach(htp);
				}
			    }
			    (void)gettimeofday(&ntime, 0);
			    looptime = ntime.tv_sec + FASTTOUT;
			}
			break;
		default:
			if (trace) {
				fprintf(fd, "garbage message: ");
				print(msg, &from);
			}
			break;
		}
	}
	goto loop;
}


/*
 * tell the world who our master is
 */
static void
setmaster(msg)
	struct tsp *msg;
{
	if (slavenet
	    && (slavenet != old_slavenet
		|| strcmp(msg->tsp_name, master_name)
		|| old_status != status)) {
		(void)strcpy(master_name, msg->tsp_name);
		old_slavenet = slavenet;
		old_status = status;

		if (status & MASTER) {
			syslog(LOG_NOTICE, "submaster to %s", master_name);
			if (trace)
				fprintf(fd, "submaster to %s\n", master_name);

		} else {
			syslog(LOG_NOTICE, "slave to %s", master_name);
			if (trace)
				fprintf(fd, "slave to %s\n", master_name);
		}
	}
}



/*
 * handle date change request on a slave
 */
static void
schgdate(msg, newdate)
	struct tsp *msg;
	char *newdate;
{
	struct tsp to;
	u_short seq;
	struct sockaddr_in taddr;
	struct timeval otime;

	if (!slavenet)
		return;			/* no where to forward */

	taddr = from;
	seq = msg->tsp_seq;

	syslog(LOG_INFO,
	       "forwarding date change by %s to %s",
	       msg->tsp_name, newdate);

	/* adjust time for residence on the queue */
	(void)gettimeofday(&otime, 0);
	adj_msg_time(msg, &otime);

	to.tsp_type = TSP_SETDATEREQ;
	to.tsp_time = msg->tsp_time;
	(void)strcpy(to.tsp_name, hostname);
	if (!acksend(&to, &slavenet->dest_addr,
		     ANYADDR, TSP_DATEACK,
		     slavenet, 0))
		return;			/* no answer */

	xmit(TSP_DATEACK, seq, &taddr);
}


/*
 * Used before answering a broadcast message to avoid network
 * contention and likely collisions.
 */
static void
answerdelay()
{
#ifdef sgi
	sginap(delay1);
#else
	struct timeval timeout;

	timeout.tv_sec = 0;
	timeout.tv_usec = delay1;

	(void)select(0, (fd_set *)NULL, (fd_set *)NULL, (fd_set *)NULL,
	    &timeout);
	return;
#endif /* sgi */
}
