/*
 * Copyright (c) 1985 Regents of the University of California.
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
static char sccsid[] = "@(#)master.c	2.20 (Berkeley) 3/2/91";
#endif /* not lint */

#include "globals.h"
#include <protocols/timed.h>
#include <sys/file.h>
#include <setjmp.h>
#include <utmp.h>
#include "pathnames.h"

extern int machup;
extern int measure_delta;
extern jmp_buf jmpenv;

extern u_short sequence;

#ifdef MEASURE
int header;
FILE *fp = NULL;
#endif

/*
 * The main function of `master' is to periodically compute the differences 
 * (deltas) between its clock and the clocks of the slaves, to compute the 
 * network average delta, and to send to the slaves the differences between 
 * their individual deltas and the network delta.
 * While waiting, it receives messages from the slaves (i.e. requests for
 * master's name, remote requests to set the network time, ...), and
 * takes the appropriate action.
 */

master()
{
	int ind;
	long pollingtime;
	struct timeval wait;
	struct timeval time;
	struct timezone tzone;
	struct tsp *msg, to;
	struct sockaddr_in saveaddr;
	int findhost();
	char *date();
	struct tsp *readmsg();
	struct tsp *answer, *acksend();
	char olddate[32];
	struct sockaddr_in server;
	register struct netinfo *ntp;

#ifdef MEASURE
	if (fp == NULL) {
		fp = fopen(_PATH_MASTERLOG, "w");
		setlinebuf(fp);
	}
#endif

	syslog(LOG_INFO, "This machine is master");
	if (trace)
		fprintf(fd, "THIS MACHINE IS MASTER\n");

	for (ntp = nettab; ntp != NULL; ntp = ntp->next)
		if (ntp->status == MASTER)
			masterup(ntp);
	pollingtime = 0;

loop:
	(void)gettimeofday(&time, (struct timezone *)0);
	if (time.tv_sec >= pollingtime) {
		pollingtime = time.tv_sec + SAMPLEINTVL;
		synch(0L);

		for (ntp = nettab; ntp != NULL; ntp = ntp->next) {
			to.tsp_type = TSP_LOOP;
			to.tsp_vers = TSPVERSION;
			to.tsp_seq = sequence++;
			to.tsp_hopcnt = 10;
			(void)strcpy(to.tsp_name, hostname);
			bytenetorder(&to);
			if (sendto(sock, (char *)&to, sizeof(struct tsp), 0,
			    (struct sockaddr *)&ntp->dest_addr,
			    sizeof(struct sockaddr_in)) < 0) {
				syslog(LOG_ERR, "sendto: %m");
				exit(1);
			}
		}
	}

	wait.tv_sec = pollingtime - time.tv_sec;
	wait.tv_usec = 0;
	msg = readmsg(TSP_ANY, (char *)ANYADDR, &wait, (struct netinfo *)NULL);
	if (msg != NULL) {
		switch (msg->tsp_type) {

		case TSP_MASTERREQ:
			break;
		case TSP_SLAVEUP:
			ind = addmach(msg->tsp_name, &from);
			newslave(ind, msg->tsp_seq);
			break;
		case TSP_SETDATE:
			saveaddr = from;
			/*
			 * the following line is necessary due to syslog
			 * calling ctime() which clobbers the static buffer
			 */
			(void)strcpy(olddate, date());
			(void)gettimeofday(&time, &tzone);
			time.tv_sec = msg->tsp_time.tv_sec;
			time.tv_usec = msg->tsp_time.tv_usec;
			logwtmp("|", "date", "");
			(void)settimeofday(&time, &tzone);
			logwtmp("}", "date", "");
			syslog(LOG_NOTICE, "date changed from: %s", olddate);
			msg->tsp_type = TSP_DATEACK;
			msg->tsp_vers = TSPVERSION;
			(void)strcpy(msg->tsp_name, hostname);
			bytenetorder(msg);
			if (sendto(sock, (char *)msg, sizeof(struct tsp), 0,
			    (struct sockaddr *)&saveaddr,
			    sizeof(struct sockaddr_in)) < 0) {
				syslog(LOG_ERR, "sendto: %m");
				exit(1);
			}
			spreadtime();
			pollingtime = 0;
			break;
		case TSP_SETDATEREQ:
			ind = findhost(msg->tsp_name);
			if (ind < 0) { 
			    syslog(LOG_WARNING,
				"DATEREQ from uncontrolled machine");
			    break;
			}
			if (hp[ind].seq !=  msg->tsp_seq) {
				hp[ind].seq = msg->tsp_seq;
				/*
				 * the following line is necessary due to syslog
				 * calling ctime() which clobbers the static buffer
				 */
				(void)strcpy(olddate, date());
				(void)gettimeofday(&time, &tzone);
				time.tv_sec = msg->tsp_time.tv_sec;
				time.tv_usec = msg->tsp_time.tv_usec;
				logwtmp("|", "date", "");
				(void)settimeofday(&time, &tzone);
				logwtmp("{", "date", "");
				syslog(LOG_NOTICE,
				    "date changed by %s from: %s",
				    msg->tsp_name, olddate);
				spreadtime();
				pollingtime = 0;
			}
			break;
		case TSP_MSITE:
		case TSP_MSITEREQ:
			break;
		case TSP_TRACEON:
			if (!(trace)) {
				fd = fopen(tracefile, "w");
				setlinebuf(fd);
				fprintf(fd, "Tracing started on: %s\n\n", 
							date());
			}
			trace = ON;
			break;
		case TSP_TRACEOFF:
			if (trace) {
				fprintf(fd, "Tracing ended on: %s\n", date());
				(void)fclose(fd);
			}
#ifdef GPROF
			moncontrol(0);
			_mcleanup();
			moncontrol(1);
#endif
			trace = OFF;
			break;
		case TSP_ELECTION:
			to.tsp_type = TSP_QUIT;
			(void)strcpy(to.tsp_name, hostname);
			server = from;
			answer = acksend(&to, &server, msg->tsp_name, TSP_ACK,
			    (struct netinfo *)NULL);
			if (answer == NULL) {
				syslog(LOG_ERR, "election error");
			} else {
				(void) addmach(msg->tsp_name, &from);
			}
			pollingtime = 0;
			break;
		case TSP_CONFLICT:
			/*
			 * After a network partition, there can be 
			 * more than one master: the first slave to 
			 * come up will notify here the situation.
			 */

			(void)strcpy(to.tsp_name, hostname);

			if (fromnet == NULL)
				break;
			for(;;) {
				to.tsp_type = TSP_RESOLVE;
				answer = acksend(&to, &fromnet->dest_addr,
				    (char *)ANYADDR, TSP_MASTERACK, fromnet);
				if (answer == NULL)
					break;
				to.tsp_type = TSP_QUIT;
				server = from;
				msg = acksend(&to, &server, answer->tsp_name,
				    TSP_ACK, (struct netinfo *)NULL);
				if (msg == NULL) {
					syslog(LOG_ERR, "error on sending QUIT");
				} else {
					(void) addmach(answer->tsp_name, &from);
				}
			}
			masterup(fromnet);
			pollingtime = 0;
			break;
		case TSP_RESOLVE:
			/*
			 * do not want to call synch() while waiting
			 * to be killed!
			 */
			(void)gettimeofday(&time, (struct timezone *)0);
			pollingtime = time.tv_sec + SAMPLEINTVL;
			break;
		case TSP_QUIT:
			/* become slave */
#ifdef MEASURE
			if (fp != NULL) {
				(void)fclose(fp);
				fp = NULL;
			}
#endif
			longjmp(jmpenv, 2);
			break;
		case TSP_LOOP:
			/*
			 * We should not have received this from a net
			 * we are master on.  There must be two masters
			 * in this case.
			 */
			to.tsp_type = TSP_QUIT;
			(void)strcpy(to.tsp_name, hostname);
			server = from;
			answer = acksend(&to, &server, msg->tsp_name, TSP_ACK,
				(struct netinfo *)NULL);
			if (answer == NULL) {
				syslog(LOG_WARNING,
				    "loop breakage: no reply to QUIT");
			} else {
				(void)addmach(msg->tsp_name, &from);
			}
		default:
			if (trace) {
				fprintf(fd, "garbage: ");
				print(msg, &from);
			}
			break;
		}
	}
	goto loop;
}

/*
 * `synch' synchronizes all the slaves by calling measure, 
 * networkdelta and correct 
 */

synch(mydelta)
long mydelta;
{
	int i;
	int measure_status;
	long netdelta;
	struct timeval tack;
#ifdef MEASURE
#define MAXLINES	8
	static int lines = 1;
	struct timeval start, end;
#endif
	int measure();
	int correct();
	long networkdelta();
	char *date();

	if (slvcount > 1) {
#ifdef MEASURE
		(void)gettimeofday(&start, (struct timezone *)0);
		if (header == ON || --lines == 0) {
			fprintf(fp, "%s\n", date());
			for (i=0; i<slvcount; i++)
				fprintf(fp, "%.7s\t", hp[i].name);
			fprintf(fp, "\n");
			lines = MAXLINES;
			header = OFF;
		}
#endif
		machup = 1;
		hp[0].delta = 0;
		for(i=1; i<slvcount; i++) {
			tack.tv_sec = 0;
			tack.tv_usec = 500000;
			if ((measure_status = measure(&tack, &hp[i].addr)) <0) {
				syslog(LOG_ERR, "measure: %m");
				exit(1);
			}
			hp[i].delta = measure_delta;
			if (measure_status == GOOD)
				machup++;
		}
		if (status & SLAVE) {
			/* called by a submaster */
			if (trace)
				fprintf(fd, "submaster correct: %d ms.\n",
				    mydelta);
			correct(mydelta);	
		} else {
			if (machup > 1) {
				netdelta = networkdelta();
				if (trace)
					fprintf(fd,
					    "master correct: %d ms.\n",
					    mydelta);
				correct(netdelta);
			}
		}
#ifdef MEASURE
		gettimeofday(&end, 0);
		end.tv_sec -= start.tv_sec;
		end.tv_usec -= start.tv_usec;
		if (end.tv_usec < 0) {
			end.tv_sec -= 1;
			end.tv_usec += 1000000;
		}
		fprintf(fp, "%d ms.\n", (end.tv_sec*1000+end.tv_usec/1000));
#endif
		for(i=1; i<slvcount; i++) {
			if (hp[i].delta == HOSTDOWN) {
				rmmach(i);
#ifdef MEASURE
				header = ON;
#endif
			}
		}
	} else {
		if (status & SLAVE) {
			correct(mydelta);
		}
	}
}

/*
 * 'spreadtime' sends the time to each slave after the master
 * has received the command to set the network time 
 */

spreadtime()
{
	int i;
	struct tsp to;
	struct tsp *answer, *acksend();

	for(i=1; i<slvcount; i++) {
		to.tsp_type = TSP_SETTIME;
		(void)strcpy(to.tsp_name, hostname);
		(void)gettimeofday(&to.tsp_time, (struct timezone *)0);
		answer = acksend(&to, &hp[i].addr, hp[i].name, TSP_ACK,
		    (struct netinfo *)NULL);
		if (answer == NULL) {
			syslog(LOG_WARNING,
			    "no reply to SETTIME from: %s", hp[i].name);
		}
	}
}

findhost(name)
char *name;
{
	int i;
	int ind;

	ind = -1;
	for (i=1; i<slvcount; i++) {
		if (strcmp(name, hp[i].name) == 0) {
			ind = i;
			break;
		}
	}
	return(ind);
}

/*
 * 'addmach' adds a host to the list of controlled machines
 * if not already there 
 */

addmach(name, addr)
char *name;
struct sockaddr_in *addr;
{
	int ret;
	int findhost();

	ret = findhost(name);
	if (ret < 0) {
		hp[slvcount].addr = *addr;
		hp[slvcount].name = (char *)malloc(MAXHOSTNAMELEN);
		(void)strcpy(hp[slvcount].name, name);
		hp[slvcount].seq = 0;
		ret = slvcount;
		if (slvcount < NHOSTS)
			slvcount++;
		else {
			syslog(LOG_ERR, "no more slots in host table");
		}
	} else {
		/* need to clear sequence number anyhow */
		hp[ret].seq = 0;
	}
#ifdef MEASURE
	header = ON;
#endif
	return(ret);
}

/*
 * Remove all the machines from the host table that exist on the given
 * network.  This is called when a master transitions to a slave on a
 * given network.
 */

rmnetmachs(ntp)
	register struct netinfo *ntp;
{
	int i;

	if (trace)
		prthp();
	for (i = 1; i < slvcount; i++)
		if ((hp[i].addr.sin_addr.s_addr & ntp->mask) == ntp->net)
			rmmach(i--);
	if (trace)
		prthp();
}

/*
 * remove the machine with the given index in the host table.
 */
rmmach(ind)
	int ind;
{
	if (trace)
		fprintf(fd, "rmmach: %s\n", hp[ind].name);
	free(hp[ind].name);
	hp[ind] = hp[--slvcount];
}

prthp()
{
	int i;

	fprintf(fd, "host table:");
	for (i=1; i<slvcount; i++)
		fprintf(fd, " %s", hp[i].name);
	fprintf(fd, "\n");
}

masterup(net)
struct netinfo *net;
{
	struct timeval wait;
	struct tsp to, *msg, *readmsg();

	to.tsp_type = TSP_MASTERUP;
	to.tsp_vers = TSPVERSION;
	(void)strcpy(to.tsp_name, hostname);
	bytenetorder(&to);
	if (sendto(sock, (char *)&to, sizeof(struct tsp), 0,
	    (struct sockaddr *)&net->dest_addr,
	    sizeof(struct sockaddr_in)) < 0) {
		syslog(LOG_ERR, "sendto: %m");
		exit(1);
	}

	for (;;) {
		wait.tv_sec = 1;
		wait.tv_usec = 0;
		msg = readmsg(TSP_SLAVEUP, (char *)ANYADDR, &wait, net);
		if (msg != NULL) {
			(void) addmach(msg->tsp_name, &from);
		} else
			break;
	}
}

newslave(ind, seq)
u_short seq;
{
	struct tsp to;
	struct tsp *answer, *acksend();

	if (trace)
		prthp();
	if (seq == 0 || hp[ind].seq !=  seq) {
		hp[ind].seq = seq;
		to.tsp_type = TSP_SETTIME;
		(void)strcpy(to.tsp_name, hostname);
		/*
		 * give the upcoming slave the time
		 * to check its input queue before
		 * setting the time
		 */
		sleep(1);
		(void) gettimeofday(&to.tsp_time,
		    (struct timezone *)0);
		answer = acksend(&to, &hp[ind].addr,
		    hp[ind].name, TSP_ACK,
		    (struct netinfo *)NULL);
		if (answer == NULL) {
			syslog(LOG_WARNING,
			    "no reply to initial SETTIME from: %s",
			    hp[ind].name);
			rmmach(ind);
		}
	}
}
