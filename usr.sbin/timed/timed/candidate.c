/*
 * Copyright (c) 1983 Regents of the University of California.
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
static char sccsid[] = "@(#)candidate.c	2.7 (Berkeley) 3/2/91";
#endif /* not lint */

#include "globals.h"
#include <protocols/timed.h>

#define ELECTIONWAIT	3	/* seconds */

/*
 * `election' candidates a host as master: it is called by a slave 
 * which runs with the -M option set when its election timeout expires. 
 * Note the conservative approach: if a new timed comes up, or another
 * candidate sends an election request, the candidature is withdrawn.
 */

election(net)
struct netinfo *net;
{
	int ret;
	struct tsp *resp, msg, *readmsg();
	struct timeval wait;
	struct tsp *answer, *acksend();
	long casual();
	struct sockaddr_in server;

	syslog(LOG_INFO, "THIS MACHINE IS A CANDIDATE");
	if (trace) {
		fprintf(fd, "THIS MACHINE IS A CANDIDATE\n");
	}

	ret = MASTER;
	slvcount = 1;

	msg.tsp_type = TSP_ELECTION;
	msg.tsp_vers = TSPVERSION;
	(void)strcpy(msg.tsp_name, hostname);
	bytenetorder(&msg);
	if (sendto(sock, (char *)&msg, sizeof(struct tsp), 0,
		(struct sockaddr *)&net->dest_addr,
		sizeof(struct sockaddr_in)) < 0) {
		syslog(LOG_ERR, "sendto: %m");
		exit(1);
	}

	do {
		wait.tv_sec = ELECTIONWAIT;
		wait.tv_usec = 0;
		resp = readmsg(TSP_ANY, (char *)ANYADDR, &wait, net);
		if (resp != NULL) {
			switch (resp->tsp_type) {

			case TSP_ACCEPT:
				(void) addmach(resp->tsp_name, &from);
				break;

			case TSP_MASTERUP:
			case TSP_MASTERREQ:
				/*
				 * If a timedaemon is coming up at the same time,
				 * give up the candidature: it will be the master.
				 */
				ret = SLAVE;
				break;

			case TSP_QUIT:
			case TSP_REFUSE:
				/*
				 * Collision: change value of election timer 
				 * using exponential backoff.
				 * The value of timer will be recomputed (in slave.c)
				 * using the original interval when election will 
				 * be successfully completed.
				 */
				backoff *= 2;
				delay2 = casual((long)MINTOUT, 
							(long)(MAXTOUT * backoff));
				ret = SLAVE;
				break;

			case TSP_ELECTION:
				/* no master for another round */
				msg.tsp_type = TSP_REFUSE;
				(void)strcpy(msg.tsp_name, hostname);
				server = from;
				answer = acksend(&msg, &server, resp->tsp_name,
				    TSP_ACK, (struct netinfo *)NULL);
				if (answer == NULL) {
					syslog(LOG_ERR, "error in election");
				} else {
					(void) addmach(resp->tsp_name, &from);
				}
				break;

			case TSP_SLAVEUP:
				(void) addmach(resp->tsp_name, &from);
				break;

			case TSP_SETDATE:
			case TSP_SETDATEREQ:
				break;

			default:
				if (trace) {
					fprintf(fd, "candidate: ");
					print(resp, &from);
				}
				break;
			}
		} else {
			break;
		}
	} while (ret == MASTER);
	return(ret);
}
