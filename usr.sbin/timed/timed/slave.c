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
static char sccsid[] = "@(#)slave.c	2.22 (Berkeley) 3/2/91";
#endif /* not lint */

#include "globals.h"
#include <protocols/timed.h>
#include <setjmp.h>
#include "pathnames.h"

extern jmp_buf jmpenv;

extern u_short sequence;

slave()
{
	int length;
	int senddateack;
	long electiontime, refusetime, looktime;
	u_short seq;
	char candidate[MAXHOSTNAMELEN];
	struct tsp *msg, to, *readmsg();
	struct sockaddr_in saveaddr, msaveaddr;
	struct timeval time, wait;
	struct tsp *answer, *acksend();
	int timeout();
	char *date();
	long casual();
	int bytenetorder();
	char olddate[32];
	struct sockaddr_in server;
	register struct netinfo *ntp;
	int ind;
	struct tsp resp;
	extern int Mflag;
	extern int justquit;
#ifdef MEASURE
	extern FILE *fp;
#endif
	if (slavenet) {
		resp.tsp_type = TSP_SLAVEUP;
		resp.tsp_vers = TSPVERSION;
		(void)strcpy(resp.tsp_name, hostname);
		bytenetorder(&resp);
		if (sendto(sock, (char *)&resp, sizeof(struct tsp), 0,
		    (struct sockaddr *)&slavenet->dest_addr,
		    sizeof(struct sockaddr_in)) < 0) {
			syslog(LOG_ERR, "sendto: %m");
			exit(1);
		}
	}

	if (status & MASTER) {
#ifdef MEASURE
		if (fp == NULL) {
			fp = fopen(_PATH_MASTERLOG, "w");
			setlinebuf(fp);
		}
#endif
		syslog(LOG_INFO, "THIS MACHINE IS A SUBMASTER");
		if (trace) {
			fprintf(fd, "THIS MACHINE IS A SUBMASTER\n");
		}
		for (ntp = nettab; ntp != NULL; ntp = ntp->next)
			if (ntp->status == MASTER)
				masterup(ntp);

	} else {
		syslog(LOG_INFO, "THIS MACHINE IS A SLAVE");
		if (trace) {
			fprintf(fd, "THIS MACHINE IS A SLAVE\n");
		}
	}

	seq = 0;
	senddateack = OFF;
	refusetime = 0;

	(void)gettimeofday(&time, (struct timezone *)0);
	electiontime = time.tv_sec + delay2;
	if (Mflag)
		if (justquit)
			looktime = time.tv_sec + delay2;
		else 
			looktime = 1;
	else
		looktime = 0;

loop:
	length = sizeof(struct sockaddr_in);
	(void)gettimeofday(&time, (struct timezone *)0);
	if (time.tv_sec > electiontime) {
		if (trace) 
			fprintf(fd, "election timer expired\n");
		longjmp(jmpenv, 1);
	}
	if (looktime && time.tv_sec > looktime) {
		if (trace) 
			fprintf(fd, "Looking for nets to master and loops\n");
		
		if (nignorednets > 0) {
			for (ntp = nettab; ntp != NULL; ntp = ntp->next) {
				if (ntp->status == IGNORE) {
					lookformaster(ntp);
					if (ntp->status == MASTER)
						masterup(ntp);
					else
						ntp->status = IGNORE;
				}
			}
			setstatus();
#ifdef MEASURE
			/*
			 * Check to see if we just became master
			 * (file not open)
			 */
			if (fp == NULL) {
				fp = fopen(_PATH_MASTERLOG, "w");
				setlinebuf(fp);
			}
#endif
		}

		for (ntp = nettab; ntp != NULL; ntp = ntp->next) {
		    if (ntp->status == MASTER) {
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
		(void)gettimeofday(&time, (struct timezone *)0);
		looktime = time.tv_sec + delay2;
	}
	wait.tv_sec = electiontime - time.tv_sec + 10;
	wait.tv_usec = 0;
	msg = readmsg(TSP_ANY, (char *)ANYADDR, &wait, (struct netinfo *)NULL);
	if (msg != NULL) {
		switch (msg->tsp_type) {
		case TSP_SETDATE:
#ifdef TESTING
		case TSP_TEST:
#endif
		case TSP_MSITE:
		case TSP_TRACEOFF:
		case TSP_TRACEON:
			break;
		case TSP_MASTERUP:
			if (fromnet == NULL) {
				if (trace) {
					fprintf(fd, "slave ignored: ");
					print(msg, &from);
				}
				goto loop;
			}
			break;
		default:
			if (fromnet == NULL || fromnet->status == IGNORE) {
				if (trace) {
					fprintf(fd, "slave ignored: ");
					print(msg, &from);
				}
				goto loop;
			}
			break;
		}

		switch (msg->tsp_type) {

		case TSP_ADJTIME:
			if (fromnet->status != SLAVE)
				break;
			(void)gettimeofday(&time, (struct timezone *)0);
			electiontime = time.tv_sec + delay2;
			if (seq != msg->tsp_seq) {
				seq = msg->tsp_seq;
				if ((status & SUBMASTER) == SUBMASTER) {
					synch((msg->tsp_time.tv_sec * 1000) + 
					    (msg->tsp_time.tv_usec / 1000));
				} else {
					adjclock(&(msg->tsp_time));
				}
			}
			break;
		case TSP_SETTIME:
			if (fromnet->status != SLAVE)
				break;
			if (seq == msg->tsp_seq)
				break;

			seq = msg->tsp_seq;

			(void)strcpy(olddate, date());
			logwtmp("|", "date", "");
			(void)settimeofday(&msg->tsp_time,
				(struct timezone *)0);
			logwtmp("{", "date", "");
			syslog(LOG_NOTICE, "date changed by %s from: %s",
				msg->tsp_name, olddate);
			if ((status & SUBMASTER) == SUBMASTER)
				spreadtime();
			(void)gettimeofday(&time, (struct timezone *)0);
			electiontime = time.tv_sec + delay2;

			if (senddateack == ON) {
				senddateack = OFF;
				msg->tsp_type = TSP_DATEACK;
				(void)strcpy(msg->tsp_name, hostname);
				bytenetorder(msg);
				length = sizeof(struct sockaddr_in);
				if (sendto(sock, (char *)msg, 
				    sizeof(struct tsp), 0,
				    (struct sockaddr *)&saveaddr,
				    length) < 0) {
					syslog(LOG_ERR, "sendto: %m");
					exit(1);
				}
			}
			break;
		case TSP_MASTERUP:
			if (slavenet && fromnet != slavenet)
				break;
			makeslave(fromnet);
			setstatus();
			msg->tsp_type = TSP_SLAVEUP;
			msg->tsp_vers = TSPVERSION;
			(void)strcpy(msg->tsp_name, hostname);
			bytenetorder(msg);
			answerdelay();
			length = sizeof(struct sockaddr_in);
			if (sendto(sock, (char *)msg, sizeof(struct tsp), 0, 
			    (struct sockaddr *)&from, length) < 0) {
				syslog(LOG_ERR, "sendto: %m");
				exit(1);
			}
			backoff = 1;
			delay2 = casual((long)MINTOUT, (long)MAXTOUT);
			(void)gettimeofday(&time, (struct timezone *)0);
			electiontime = time.tv_sec + delay2;
			refusetime = 0;
			break;
		case TSP_MASTERREQ:
			if (fromnet->status != SLAVE)
				break;
			(void)gettimeofday(&time, (struct timezone *)0);
			electiontime = time.tv_sec + delay2;
			break;
		case TSP_SETDATE:
			saveaddr = from;
			msg->tsp_type = TSP_SETDATEREQ;
			msg->tsp_vers = TSPVERSION;
			(void)strcpy(msg->tsp_name, hostname);
			for (ntp = nettab; ntp != NULL; ntp = ntp->next) {
				if (ntp->status == SLAVE)
					break;
			}
			if (ntp == NULL)
				break;
			answer = acksend(msg, &ntp->dest_addr, (char *)ANYADDR,
			    TSP_DATEACK, ntp);
			if (answer != NULL) {
				msg->tsp_type = TSP_ACK;
				bytenetorder(msg);
				length = sizeof(struct sockaddr_in);
				if (sendto(sock, (char *)msg,
				    sizeof(struct tsp), 0,
				    (struct sockaddr *)&saveaddr,
				    length) < 0) {
					syslog(LOG_ERR, "sendto: %m");
					exit(1);
				}
				senddateack = ON;
			}
			break;
		case TSP_SETDATEREQ:
			saveaddr = from;
			if (status != SUBMASTER || fromnet->status != MASTER)
				break;
			for (ntp = nettab; ntp != NULL; ntp = ntp->next) {
				if (ntp->status == SLAVE)
					break;
			}
			ind = findhost(msg->tsp_name);
			if (ind < 0) {
			    syslog(LOG_WARNING,
				"DATEREQ from uncontrolled machine");
			    break;
			}
			syslog(LOG_DEBUG,
			    "forwarding date change request for %s",
			    msg->tsp_name);
			(void)strcpy(msg->tsp_name, hostname);
			answer = acksend(msg, &ntp->dest_addr, (char *)ANYADDR,
			    TSP_DATEACK, ntp);
			if (answer != NULL) {
				msg->tsp_type = TSP_DATEACK;
				bytenetorder(msg);
				length = sizeof(struct sockaddr_in);
				if (sendto(sock, (char *)msg,
				    sizeof(struct tsp), 0,
				    (struct sockaddr *)&saveaddr,
				    length) < 0) {
					syslog(LOG_ERR, "sendto: %m");
					exit(1);
				}
			}
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
		case TSP_SLAVEUP:
			if ((status & MASTER) && fromnet->status == MASTER) {
				ind = addmach(msg->tsp_name, &from);
				newslave(ind, msg->tsp_seq);
			}
			break;
		case TSP_ELECTION:
			if (fromnet->status == SLAVE) {
				(void)gettimeofday(&time, (struct timezone *)0);
				electiontime = time.tv_sec + delay2;
				seq = 0;            /* reset sequence number */
				if (time.tv_sec < refusetime)
					msg->tsp_type = TSP_REFUSE;
				else {
					msg->tsp_type = TSP_ACCEPT;
					refusetime = time.tv_sec + 30;
				}
				(void)strcpy(candidate, msg->tsp_name);
				(void)strcpy(msg->tsp_name, hostname);
				answerdelay();
				server = from;
				answer = acksend(msg, &server, candidate, TSP_ACK,
				    (struct netinfo *)NULL);
				if (answer == NULL)
					syslog(LOG_WARNING,
					   "no answer from master candidate\n");
			} else {	/* fromnet->status == MASTER */
				to.tsp_type = TSP_QUIT;
				(void)strcpy(to.tsp_name, hostname);
				server = from;
				answer = acksend(&to, &server, msg->tsp_name,
				    TSP_ACK, (struct netinfo *)NULL);
				if (answer == NULL) {
					syslog(LOG_WARNING,
					    "election error: no reply to QUIT");
				} else {
					(void) addmach(msg->tsp_name, &from);
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
                                        syslog(LOG_WARNING,
					    "conflict error: no reply to QUIT");
				} else {
                                        (void) addmach(answer->tsp_name, &from);
				}
                        }
                        masterup(fromnet);
                        break;
		case TSP_MSITE:
			if (!slavenet)
				break;
			msaveaddr = from;
			msg->tsp_type = TSP_MSITEREQ;
			msg->tsp_vers = TSPVERSION;
			(void)strcpy(msg->tsp_name, hostname);
			answer = acksend(msg, &slavenet->dest_addr,
					 (char *)ANYADDR, TSP_ACK, slavenet);
			if (answer != NULL) {
				msg->tsp_type = TSP_ACK;
				length = sizeof(struct sockaddr_in);
				bytenetorder(msg);
				if (sendto(sock, (char *)msg,
				    sizeof(struct tsp), 0,
				    (struct sockaddr *)&msaveaddr,
				    length) < 0) {
					syslog(LOG_ERR, "sendto: %m");
					exit(1);
				}
			}
			break;
		case TSP_ACCEPT:
		case TSP_REFUSE:
			break;
		case TSP_RESOLVE:
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
#ifdef TESTING
		case TSP_TEST:
			electiontime = 0;
			break;
#endif
		case TSP_MSITEREQ:
			if (status & MASTER)
				break;
			if (trace) {
				fprintf(fd, "garbage: ");
				print(msg, &from);
			}
			break;

		case TSP_LOOP:
			/* looking for loops of masters */
			if ( !(status & MASTER))
				break;
			if (fromnet->status == SLAVE) {
			    if ( !strcmp(msg->tsp_name, hostname)) {
				  for(;;) {
				    to.tsp_type = TSP_RESOLVE;
				    answer = acksend(&to, &fromnet->dest_addr,
					(char *)ANYADDR, TSP_MASTERACK,
					fromnet);
				    if (answer == NULL)
					    break;
				    to.tsp_type = TSP_QUIT;
				    (void)strcpy(to.tsp_name, hostname);
				    server = from;
				    answer = acksend(&to, &server,
					answer->tsp_name, TSP_ACK,
					(struct netinfo *)NULL);
				    if (answer == NULL) {
					syslog(LOG_ERR, "loop kill error");
				    } else {
					electiontime = 0;
				    }
				  }
			    } else {
				if (msg->tsp_hopcnt-- <= 0)
				    break;
				bytenetorder(msg);
				ntp = nettab;
				for (; ntp != NULL; ntp = ntp->next)
				    if (ntp->status == MASTER)
					if (sendto(sock, (char *)msg, 
					    sizeof(struct tsp), 0,
					    (struct sockaddr *)&ntp->dest_addr,
					    length) < 0) {
						syslog(LOG_ERR, "sendto: %m");
						exit(1);
					}
			    }
			} else {
			    /*
			     * We should not have received this from a net
			     * we are master on.  There must be two masters
			     * in this case.
			     */
			    if (fromnet->my_addr.s_addr == from.sin_addr.s_addr)
				break;
			    for (;;) {
				to.tsp_type = TSP_RESOLVE;
				answer = acksend(&to, &fromnet->dest_addr,
				    (char *)ANYADDR, TSP_MASTERACK,
				    fromnet);
				if (answer == NULL)
					break;
				to.tsp_type = TSP_QUIT;
				(void)strcpy(to.tsp_name, hostname);
				server = from;
				answer = acksend(&to, &server, answer->tsp_name,
				    TSP_ACK, (struct netinfo *)NULL);
				if (answer == NULL) {
					syslog(LOG_ERR, "loop kill error2");
				} else {
					(void)addmach(msg->tsp_name, &from);
				}
			    }
			}
			break;
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
 * Used before answering a broadcast message to avoid network
 * contention and likely collisions.
 */
answerdelay()
{
	struct timeval timeout;

	timeout.tv_sec = 0;
	timeout.tv_usec = delay1;

	(void)select(0, (fd_set *)NULL, (fd_set *)NULL, (fd_set *)NULL,
	    &timeout);
	return;
}
