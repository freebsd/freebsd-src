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
static char sccsid[] = "@(#)acksend.c	2.8 (Berkeley) 3/2/91";
#endif /* not lint */

#include "globals.h"
#include <protocols/timed.h>

#define RECEIVED	0
#define LOST	 	1
#define SECFORACK	1	/* seconds */
#define USECFORACK	0	/* microseconds */
#define MAXCOUNT	5

struct tsp *answer;

/*
 * Acksend implements reliable datagram transmission by using sequence 
 * numbers and retransmission when necessary.
 * `name' is the name of the destination
 * `addr' is the address to send to
 * If `name' is ANYADDR, this routine implements reliable broadcast.
 */

struct tsp *acksend(message, addr, name, ack, net)
struct tsp *message;
struct sockaddr_in *addr;
char *name;
int ack;
struct netinfo *net;
{
	int count;
	int flag;
	extern u_short sequence;
	struct timeval tout;
	struct tsp *readmsg();

	count = 0;

	message->tsp_vers = TSPVERSION;
	message->tsp_seq = sequence;
	if (trace) {
		fprintf(fd, "acksend: ");
		if (name == ANYADDR)
			fprintf(fd, "broadcast: ");
		else
			fprintf(fd, "%s: ", name);
		print(message, addr);
	}
	bytenetorder(message);
	do {
		if (sendto(sock, (char *)message, sizeof(struct tsp), 0,
		    (struct sockaddr *)addr, sizeof(struct sockaddr_in)) < 0) {
			syslog(LOG_ERR, "acksend: sendto: %m");
			exit(1);
		}
		tout.tv_sec = SECFORACK;
		tout.tv_usec = USECFORACK;
		answer  = readmsg(ack, name, &tout, net);
		if (answer != NULL) {
			if (answer->tsp_seq != sequence) {
				if (trace)
					fprintf(fd, "acksend: seq # %d != %d\n",
					    answer->tsp_seq, sequence);
				continue;
			}
			flag = RECEIVED;
		} else {
			flag = LOST;
			if (++count == MAXCOUNT) {
				break;
			}
		}
	} while (flag != RECEIVED);
	sequence++;
	return(answer);
}
