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
static char sccsid[] = "@(#)master.c	8.1 (Berkeley) 6/6/93";
#endif
static const char rcsid[] =
	"$Id$";
#endif /* not lint */

#include "globals.h"
#include <sys/file.h>
#include <sys/types.h>
#include <sys/times.h>
#include <setjmp.h>
#ifdef sgi
#include <sys/schedctl.h>
#endif /* sgi */
#include <utmp.h>
#include "pathnames.h"

extern int measure_delta;
extern jmp_buf jmpenv;
extern int Mflag;
extern int justquit;

static int dictate;
static int slvcount;			/* slaves listening to our clock */

static void mchgdate __P((struct tsp *));

#ifdef sgi
extern void logwtmp __P((struct timeval *, struct timeval *));
#else
extern void logwtmp __P((char *, char *, char *));
#endif /* sgi */

/*
 * The main function of `master' is to periodically compute the differences
 * (deltas) between its clock and the clocks of the slaves, to compute the
 * network average delta, and to send to the slaves the differences between
 * their individual deltas and the network delta.
 * While waiting, it receives messages from the slaves (i.e. requests for
 * master's name, remote requests to set the network time, ...), and
 * takes the appropriate action.
 */
int
master()
{
	struct hosttbl *htp;
	long pollingtime;
#define POLLRATE 4
	int polls;
	struct timeval wait, ntime;
	struct tsp *msg, *answer, to;
	char newdate[32];
	struct sockaddr_in taddr;
	char tname[MAXHOSTNAMELEN];
	struct netinfo *ntp;
	int i;

	syslog(LOG_NOTICE, "This machine is master");
	if (trace)
		fprintf(fd, "This machine is master\n");
	for (ntp = nettab; ntp != NULL; ntp = ntp->next) {
		if (ntp->status == MASTER)
			masterup(ntp);
	}
	(void)gettimeofday(&ntime, 0);
	pollingtime = ntime.tv_sec+3;
	if (justquit)
		polls = 0;
	else
		polls = POLLRATE-1;

/* Process all outstanding messages before spending the long time necessary
 *	to update all timers.
 */
loop:
	(void)gettimeofday(&ntime, 0);
	wait.tv_sec = pollingtime - ntime.tv_sec;
	if (wait.tv_sec < 0)
		wait.tv_sec = 0;
	wait.tv_usec = 0;
	msg = readmsg(TSP_ANY, ANYADDR, &wait, 0);
	if (!msg) {
		(void)gettimeofday(&ntime, 0);
		if (ntime.tv_sec >= pollingtime) {
			pollingtime = ntime.tv_sec + SAMPLEINTVL;
			get_goodgroup(0);

/* If a bogus master told us to quit, we can have decided to ignore a
 * network.  Therefore, periodically try to take over everything.
 */
			polls = (polls + 1) % POLLRATE;
			if (0 == polls && nignorednets > 0) {
				trace_msg("Looking for nets to re-master\n");
				for (ntp = nettab; ntp; ntp = ntp->next) {
					if (ntp->status == IGNORE
					    || ntp->status == NOMASTER) {
						lookformaster(ntp);
						if (ntp->status == MASTER) {
							masterup(ntp);
							polls = POLLRATE-1;
						}
					}
					if (ntp->status == MASTER
					    && --ntp->quit_count < 0)
						ntp->quit_count = 0;
				}
				if (polls != 0)
					setstatus();
			}

			synch(0L);

			for (ntp = nettab; ntp != NULL; ntp = ntp->next) {
				to.tsp_type = TSP_LOOP;
				to.tsp_vers = TSPVERSION;
				to.tsp_seq = sequence++;
				to.tsp_hopcnt = MAX_HOPCNT;
				(void)strncpy(to.tsp_name, hostname,
						sizeof to.tsp_name-1);
				to.tsp_name[sizeof to.tsp_name-1] = '\0';
				bytenetorder(&to);
				if (sendto(sock, (char *)&to,
					   sizeof(struct tsp), 0,
					   (struct sockaddr*)&ntp->dest_addr,
					   sizeof(ntp->dest_addr)) < 0) {
				   trace_sendto_err(ntp->dest_addr.sin_addr);
				}
			}
		}


	} else {
		switch (msg->tsp_type) {

		case TSP_MASTERREQ:
			break;

		case TSP_SLAVEUP:
			newslave(msg);
			break;

		case TSP_SETDATE:
			/*
			 * XXX check to see it is from ourself
			 */
#ifdef sgi
			(void)cftime(newdate, "%D %T", &msg->tsp_time.tv_sec);
#else
			(void)strncpy(newdate, ctime(&msg->tsp_time.tv_sec),
					sizeof newdate-1);
			newdate[sizeof newdate-1] = '\0';
#endif /* sgi */
			if (!good_host_name(msg->tsp_name)) {
				syslog(LOG_NOTICE,
				       "attempted date change by %s to %s",
				       msg->tsp_name, newdate);
				spreadtime();
				break;
			}

			mchgdate(msg);
			(void)gettimeofday(&ntime, 0);
			pollingtime = ntime.tv_sec + SAMPLEINTVL;
			break;

		case TSP_SETDATEREQ:
			if (!fromnet || fromnet->status != MASTER)
				break;
#ifdef sgi
			(void)cftime(newdate, "%D %T", &msg->tsp_time.tv_sec);
#else
			(void)strncpy(newdate, ctime(&msg->tsp_time.tv_sec),
					sizeof newdate-1);
			newdate[sizeof newdate-1] = '\0';
#endif /* sgi */
			htp = findhost(msg->tsp_name);
			if (htp == 0) {
				syslog(LOG_ERR,
				       "attempted SET DATEREQ by uncontrolled %s to %s",
				       msg->tsp_name, newdate);
				break;
			}
			if (htp->seq == msg->tsp_seq)
				break;
			htp->seq = msg->tsp_seq;
			if (!htp->good) {
				syslog(LOG_NOTICE,
				"attempted SET DATEREQ by untrusted %s to %s",
				       msg->tsp_name, newdate);
				spreadtime();
				break;
			}

			mchgdate(msg);
			(void)gettimeofday(&ntime, 0);
			pollingtime = ntime.tv_sec + SAMPLEINTVL;
			break;

		case TSP_MSITE:
			xmit(TSP_ACK, msg->tsp_seq, &from);
			break;

		case TSP_MSITEREQ:
			break;

		case TSP_TRACEON:
			traceon();
			break;

		case TSP_TRACEOFF:
			traceoff("Tracing ended at %s\n");
			break;

		case TSP_ELECTION:
			if (!fromnet)
				break;
			if (fromnet->status == MASTER) {
				pollingtime = 0;
				(void)addmach(msg->tsp_name, &from,fromnet);
			}
			taddr = from;
			(void)strncpy(tname, msg->tsp_name, sizeof tname-1);
			tname[sizeof tname-1] = '\0';
			to.tsp_type = TSP_QUIT;
			(void)strncpy(to.tsp_name, hostname,
					sizeof to.tsp_name-1);
			to.tsp_name[sizeof to.tsp_name-1] = '\0';
			answer = acksend(&to, &taddr, tname,
					 TSP_ACK, 0, 1);
			if (answer == NULL) {
				syslog(LOG_ERR, "election error by %s",
				       tname);
			}
			break;

		case TSP_CONFLICT:
			/*
			 * After a network partition, there can be
			 * more than one master: the first slave to
			 * come up will notify here the situation.
			 */
			if (!fromnet || fromnet->status != MASTER)
				break;
			(void)strncpy(to.tsp_name, hostname,
					sizeof to.tsp_name-1);
			to.tsp_name[sizeof to.tsp_name-1] = '\0';

			/* The other master often gets into the same state,
			 * with boring results if we stay at it forever.
			 */
			ntp = fromnet;	/* (acksend() can leave fromnet=0 */
			for (i = 0; i < 3; i++) {
				to.tsp_type = TSP_RESOLVE;
				(void)strncpy(to.tsp_name, hostname,
						sizeof to.tsp_name-1);
				to.tsp_name[sizeof to.tsp_name-1] = '\0';
				answer = acksend(&to, &ntp->dest_addr,
						 ANYADDR, TSP_MASTERACK,
						 ntp, 0);
				if (!answer)
					break;
				htp = addmach(answer->tsp_name,&from,ntp);
				to.tsp_type = TSP_QUIT;
				msg = acksend(&to, &htp->addr, htp->name,
					      TSP_ACK, 0, htp->noanswer);
				if (msg == NULL) {
					syslog(LOG_ERR,
				    "no response from %s to CONFLICT-QUIT",
					       htp->name);
				}
			}
			masterup(ntp);
			pollingtime = 0;
			break;

		case TSP_RESOLVE:
			if (!fromnet || fromnet->status != MASTER)
				break;
			/*
			 * do not want to call synch() while waiting
			 * to be killed!
			 */
			(void)gettimeofday(&ntime, (struct timezone *)0);
			pollingtime = ntime.tv_sec + SAMPLEINTVL;
			break;

		case TSP_QUIT:
			doquit(msg);		/* become a slave */
			break;

		case TSP_LOOP:
			if (!fromnet || fromnet->status != MASTER
			    || !strcmp(msg->tsp_name, hostname))
				break;
			/*
			 * We should not have received this from a net
			 * we are master on.  There must be two masters.
			 */
			htp = addmach(msg->tsp_name, &from,fromnet);
			to.tsp_type = TSP_QUIT;
			(void)strncpy(to.tsp_name, hostname,
					sizeof to.tsp_name-1);
			to.tsp_name[sizeof to.tsp_name-1] = '\0';
			answer = acksend(&to, &htp->addr, htp->name,
					 TSP_ACK, 0, 1);
			if (!answer) {
				syslog(LOG_WARNING,
				"loop breakage: no reply from %s=%s to QUIT",
				    htp->name, inet_ntoa(htp->addr.sin_addr));
				(void)remmach(htp);
			}

		case TSP_TEST:
			if (trace) {
				fprintf(fd,
		"\tnets = %d, masters = %d, slaves = %d, ignored = %d\n",
		nnets, nmasternets, nslavenets, nignorednets);
				setstatus();
			}
			pollingtime = 0;
			polls = POLLRATE-1;
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
 * change the system date on the master
 */
static void
mchgdate(msg)
	struct tsp *msg;
{
	char tname[MAXHOSTNAMELEN];
	char olddate[32];
	struct timeval otime, ntime;

	(void)strncpy(tname, msg->tsp_name, sizeof tname-1);
	tname[sizeof tname-1] = '\0';

	xmit(TSP_DATEACK, msg->tsp_seq, &from);

	(void)strncpy(olddate, date(), sizeof olddate-1);
	olddate[sizeof olddate-1] = '\0';

	/* adjust time for residence on the queue */
	(void)gettimeofday(&otime, 0);
	adj_msg_time(msg,&otime);

	timevalsub(&ntime, &msg->tsp_time, &otime);
	if (ntime.tv_sec < MAXADJ && ntime.tv_sec > -MAXADJ) {
		/*
		 * do not change the clock if we can adjust it
		 */
		dictate = 3;
		synch(tvtomsround(ntime));
	} else {
#ifdef sgi
		if (0 > settimeofday(&msg->tsp_time, 0)) {
			syslog(LOG_ERR, "settimeofday(): %m");
		}
		logwtmp(&otime, &msg->tsp_time);
#else
		logwtmp("|", "date", "");
		(void)settimeofday(&msg->tsp_time, 0);
		logwtmp("{", "date", "");
#endif /* sgi */
		spreadtime();
	}

	syslog(LOG_NOTICE, "date changed by %s from %s",
	       tname, olddate);
}


/*
 * synchronize all of the slaves
 */
void
synch(mydelta)
	long mydelta;
{
	struct hosttbl *htp;
	int measure_status;
	struct timeval check, stop, wait;
#ifdef sgi
	int pri;
#endif /* sgi */

	if (slvcount > 0) {
		if (trace)
			fprintf(fd, "measurements starting at %s\n", date());
		(void)gettimeofday(&check, 0);
#ifdef sgi
		/* run fast to get good time */
		pri = schedctl(NDPRI,0,NDPHIMIN);
		if (pri < 0)
			syslog(LOG_ERR, "schedctl(): %m");
#endif /* sgi */
		for (htp = self.l_fwd; htp != &self; htp = htp->l_fwd) {
			if (htp->noanswer != 0) {
				measure_status = measure(500, 100,
							 htp->name,
							 &htp->addr,0);
			} else {
				measure_status = measure(3000, 100,
							 htp->name,
							 &htp->addr,0);
			}
			if (measure_status != GOOD) {
				/* The slave did not respond.  We have
				 * just wasted lots of time on it.
				 */
				htp->delta = HOSTDOWN;
				if (++htp->noanswer >= LOSTHOST) {
					if (trace) {
						fprintf(fd,
					"purging %s for not answering ICMP\n",
							htp->name);
						(void)fflush(fd);
					}
					htp = remmach(htp);
				}
			} else {
				htp->delta = measure_delta;
			}
			(void)gettimeofday(&stop, 0);
			timevalsub(&stop, &stop, &check);
			if (stop.tv_sec >= 1) {
				if (trace)
					(void)fflush(fd);
				/*
				 * ack messages periodically
				 */
				wait.tv_sec = 0;
				wait.tv_usec = 0;
				if (0 != readmsg(TSP_TRACEON,ANYADDR,
						 &wait,0))
					traceon();
				(void)gettimeofday(&check, 0);
			}
		}
#ifdef sgi
		if (pri >= 0)
			(void)schedctl(NDPRI,0,pri);
#endif /* sgi */
		if (trace)
			fprintf(fd, "measurements finished at %s\n", date());
	}
	if (!(status & SLAVE)) {
		if (!dictate) {
			mydelta = networkdelta();
		} else {
			dictate--;
		}
	}
	if (trace && (mydelta != 0 || (status & SLAVE)))
		fprintf(fd,"local correction of %ld ms.\n", mydelta);
	correct(mydelta);
}

/*
 * sends the time to each slave after the master
 * has received the command to set the network time
 */
void
spreadtime()
{
	struct hosttbl *htp;
	struct tsp to;
	struct tsp *answer;

/* Do not listen to the consensus after forcing the time.  This is because
 *	the consensus takes a while to reach the time we are dictating.
 */
	dictate = 2;
	for (htp = self.l_fwd; htp != &self; htp = htp->l_fwd) {
		to.tsp_type = TSP_SETTIME;
		(void)strncpy(to.tsp_name, hostname, sizeof to.tsp_name-1);
		to.tsp_name[sizeof to.tsp_name-1] = '\0';
		(void)gettimeofday(&to.tsp_time, 0);
		answer = acksend(&to, &htp->addr, htp->name,
				 TSP_ACK, 0, htp->noanswer);
		if (answer == 0) {
			/* We client does not respond, then we have
			 * just wasted lots of time on it.
			 */
			syslog(LOG_WARNING,
			       "no reply to SETTIME from %s", htp->name);
			if (++htp->noanswer >= LOSTHOST) {
				if (trace) {
					fprintf(fd,
					     "purging %s for not answering",
						htp->name);
					(void)fflush(fd);
				}
				htp = remmach(htp);
			}
		}
	}
}

void
prthp(delta)
	clock_t delta;
{
	static time_t next_time;
	time_t this_time;
	struct tms tm;
	struct hosttbl *htp;
	int length, l;
	int i;

	if (!fd)			/* quit if tracing already off */
		return;

	this_time = times(&tm);
	if (this_time + delta < next_time)
		return;
	next_time = this_time + CLK_TCK;

	fprintf(fd, "host table: %d entries at %s\n", slvcount, date());
	htp = self.l_fwd;
	length = 1;
	for (i = 1; i <= slvcount; i++, htp = htp->l_fwd) {
		l = strlen(htp->name) + 1;
		if (length+l >= 80) {
			fprintf(fd, "\n");
			length = 0;
		}
		length += l;
		fprintf(fd, " %s", htp->name);
	}
	fprintf(fd, "\n");
}


static struct hosttbl *newhost_hash;
static struct hosttbl *lasthfree = &hosttbl[0];


struct hosttbl *			/* answer or 0 */
findhost(name)
	char *name;
{
	int i, j;
	struct hosttbl *htp;
	char *p;

	j= 0;
	for (p = name, i = 0; i < 8 && *p != '\0'; i++, p++)
		j = (j << 2) ^ *p;
	newhost_hash = &hosttbl[j % NHOSTS];

	htp = newhost_hash;
	if (htp->name[0] == '\0')
		return(0);
	do {
		if (!strcmp(name, htp->name))
			return(htp);
		htp = htp->h_fwd;
	} while (htp != newhost_hash);
	return(0);
}

/*
 * add a host to the list of controlled machines if not already there
 */
struct hosttbl *
addmach(name, addr, ntp)
	char *name;
	struct sockaddr_in *addr;
	struct netinfo *ntp;
{
	struct hosttbl *ret, *p, *b, *f;

	ret = findhost(name);
	if (ret == 0) {
		if (slvcount >= NHOSTS) {
			if (trace) {
				fprintf(fd, "no more slots in host table\n");
				prthp(CLK_TCK);
			}
			syslog(LOG_ERR, "no more slots in host table");
			Mflag = 0;
			longjmp(jmpenv, 2); /* give up and be a slave */
		}

		/* if our home hash slot is occupied, find a free entry
		 * in the hash table
		 */
		if (newhost_hash->name[0] != '\0') {
			do {
				ret = lasthfree;
				if (++lasthfree > &hosttbl[NHOSTS])
					lasthfree = &hosttbl[1];
			} while (ret->name[0] != '\0');

			if (!newhost_hash->head) {
				/* Move an interloper using our home.  Use
				 * scratch pointers in case the new head is
				 * pointing to itself.
				 */
				f = newhost_hash->h_fwd;
				b = newhost_hash->h_bak;
				f->h_bak = ret;
				b->h_fwd = ret;
				f = newhost_hash->l_fwd;
				b = newhost_hash->l_bak;
				f->l_bak = ret;
				b->l_fwd = ret;
				bcopy(newhost_hash,ret,sizeof(*ret));
				ret = newhost_hash;
				ret->head = 1;
				ret->h_fwd = ret;
				ret->h_bak = ret;
			} else {
				/* link to an existing chain in our home
				 */
				ret->head = 0;
				p = newhost_hash->h_bak;
				ret->h_fwd = newhost_hash;
				ret->h_bak = p;
				p->h_fwd = ret;
				newhost_hash->h_bak = ret;
			}
		} else {
			ret = newhost_hash;
			ret->head = 1;
			ret->h_fwd = ret;
			ret->h_bak = ret;
		}
		ret->addr = *addr;
		ret->ntp = ntp;
		(void)strncpy(ret->name, name, sizeof(ret->name));
		ret->good = good_host_name(name);
		ret->l_fwd = &self;
		ret->l_bak = self.l_bak;
		self.l_bak->l_fwd = ret;
		self.l_bak = ret;
		slvcount++;

		ret->noanswer = 0;
		ret->need_set = 1;

	} else {
		ret->noanswer = (ret->noanswer != 0);
	}

	/* need to clear sequence number anyhow */
	ret->seq = 0;
	return(ret);
}

/*
 * remove the machine with the given index in the host table.
 */
struct hosttbl *
remmach(htp)
	struct hosttbl *htp;
{
	struct hosttbl *lprv, *hnxt, *f, *b;

	if (trace)
		fprintf(fd, "remove %s\n", htp->name);

	/* get out of the lists */
	htp->l_fwd->l_bak = lprv = htp->l_bak;
	htp->l_bak->l_fwd = htp->l_fwd;
	htp->h_fwd->h_bak = htp->h_bak;
	htp->h_bak->h_fwd = hnxt = htp->h_fwd;

	/* If we are in the home slot, pull up the chain */
	if (htp->head && hnxt != htp) {
		if (lprv == hnxt)
			lprv = htp;

		/* Use scratch pointers in case the new head is pointing to
		 * itself.
		 */
		f = hnxt->h_fwd;
		b = hnxt->h_bak;
		f->h_bak = htp;
		b->h_fwd = htp;
		f = hnxt->l_fwd;
		b = hnxt->l_bak;
		f->l_bak = htp;
		b->l_fwd = htp;
		hnxt->head = 1;
		bcopy(hnxt, htp, sizeof(*htp));
		lasthfree = hnxt;
	} else {
		lasthfree = htp;
	}

	lasthfree->name[0] = '\0';
	lasthfree->h_fwd = 0;
	lasthfree->l_fwd = 0;
	slvcount--;

	return lprv;
}


/*
 * Remove all the machines from the host table that exist on the given
 * network.  This is called when a master transitions to a slave on a
 * given network.
 */
void
rmnetmachs(ntp)
	struct netinfo *ntp;
{
	struct hosttbl *htp;

	if (trace)
		prthp(CLK_TCK);
	for (htp = self.l_fwd; htp != &self; htp = htp->l_fwd) {
		if (ntp == htp->ntp)
			htp = remmach(htp);
	}
	if (trace)
		prthp(CLK_TCK);
}

void
masterup(net)
	struct netinfo *net;
{
	xmit(TSP_MASTERUP, 0, &net->dest_addr);

	/*
	 * Do not tell new slaves our time for a while.  This ensures
	 * we do not tell them to start using our time, before we have
	 * found a good master.
	 */
	(void)gettimeofday(&net->slvwait, 0);
}

void
newslave(msg)
	struct tsp *msg;
{
	struct hosttbl *htp;
	struct tsp *answer, to;
	struct timeval now;

	if (!fromnet || fromnet->status != MASTER)
		return;

	htp = addmach(msg->tsp_name, &from,fromnet);
	htp->seq = msg->tsp_seq;
	if (trace)
		prthp(0);

	/*
	 * If we are stable, send our time to the slave.
	 * Do not go crazy if the date has been changed.
	 */
	(void)gettimeofday(&now, 0);
	if (now.tv_sec >= fromnet->slvwait.tv_sec+3
	    || now.tv_sec < fromnet->slvwait.tv_sec) {
		to.tsp_type = TSP_SETTIME;
		(void)strncpy(to.tsp_name, hostname, sizeof to.tsp_name-1);
		to.tsp_name[sizeof to.tsp_name-1] = '\0';
		(void)gettimeofday(&to.tsp_time, 0);
		answer = acksend(&to, &htp->addr,
				 htp->name, TSP_ACK,
				 0, htp->noanswer);
		if (answer) {
			htp->need_set = 0;
		} else {
			syslog(LOG_WARNING,
			       "no reply to initial SETTIME from %s",
			       htp->name);
			htp->noanswer = LOSTHOST;
		}
	}
}


/*
 * react to a TSP_QUIT:
 */
void
doquit(msg)
	struct tsp *msg;
{
	if (fromnet->status == MASTER) {
		if (!good_host_name(msg->tsp_name)) {
			if (fromnet->quit_count <= 0) {
				syslog(LOG_NOTICE,"untrusted %s told us QUIT",
				       msg->tsp_name);
				suppress(&from, msg->tsp_name, fromnet);
				fromnet->quit_count = 1;
				return;
			}
			syslog(LOG_NOTICE, "untrusted %s told us QUIT twice",
			       msg->tsp_name);
			fromnet->quit_count = 2;
			fromnet->status = NOMASTER;
		} else {
			fromnet->status = SLAVE;
		}
		rmnetmachs(fromnet);
		longjmp(jmpenv, 2);		/* give up and be a slave */

	} else {
		if (!good_host_name(msg->tsp_name)) {
			syslog(LOG_NOTICE, "untrusted %s told us QUIT",
			       msg->tsp_name);
			fromnet->quit_count = 2;
		}
	}
}

void
traceon()
{
	if (!fd) {
		fd = fopen(_PATH_TIMEDLOG, "w");
		if (!fd) {
			trace = 0;
			return;
		}
		fprintf(fd,"Tracing started at %s\n", date());
	}
	trace = 1;
	get_goodgroup(1);
	setstatus();
	prthp(CLK_TCK);
}


void
traceoff(msg)
	char *msg;
{
	get_goodgroup(1);
	setstatus();
	prthp(CLK_TCK);
	if (trace) {
		fprintf(fd, msg, date());
		(void)fclose(fd);
		fd = 0;
	}
#ifdef GPROF
	moncontrol(0);
	_mcleanup();
	moncontrol(1);
#endif
	trace = OFF;
}


#ifdef sgi
void
logwtmp(otime, ntime)
	struct timeval *otime, *ntime;
{
	static struct utmp wtmp[2] = {
		{"","",OTIME_MSG,0,OLD_TIME,0,0,0},
		{"","",NTIME_MSG,0,NEW_TIME,0,0,0}
	};
	static char *wtmpfile = WTMP_FILE;
	int f;

	wtmp[0].ut_time = otime->tv_sec + (otime->tv_usec + 500000) / 1000000;
	wtmp[1].ut_time = ntime->tv_sec + (ntime->tv_usec + 500000) / 1000000;
	if (wtmp[0].ut_time == wtmp[1].ut_time)
		return;

	setutent();
	(void)pututline(&wtmp[0]);
	(void)pututline(&wtmp[1]);
	endutent();
	if ((f = open(wtmpfile, O_WRONLY|O_APPEND)) >= 0) {
		(void) write(f, (char *)wtmp, sizeof(wtmp));
		(void) close(f);
	}
}
#endif /* sgi */
