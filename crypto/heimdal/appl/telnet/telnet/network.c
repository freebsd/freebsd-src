/*
 * Copyright (c) 1988, 1993
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

#include "telnet_locl.h"

RCSID("$Id$");

Ring		netoring, netiring;
size_t		netobufsize = 64*1024;
size_t		netibufsize = 64*1024;

/*
 * Initialize internal network data structures.
 */

void
init_network(void)
{
    void *obuf, *ibuf;

    if ((obuf = malloc(netobufsize)) == NULL)
	exit(1);
    if ((ibuf = malloc(netibufsize)) == NULL)
	exit(1);

    if (ring_init(&netoring, obuf, netobufsize) != 1) {
	exit(1);
    }
    if (ring_init(&netiring, ibuf, netibufsize) != 1) {
	exit(1);
    }
    NetTrace = stdout;
}


/*
 * Check to see if any out-of-band data exists on a socket (for
 * Telnet "synch" processing).
 */

int
stilloob(void)
{
    static struct timeval timeout = { 0 };
    fd_set	excepts;
    int value;

    do {
	FD_ZERO(&excepts);
	if (net >= FD_SETSIZE)
	    errx (1, "fd too large");
	FD_SET(net, &excepts);
	value = select(net+1, 0, 0, &excepts, &timeout);
    } while ((value == -1) && (errno == EINTR));

    if (value < 0) {
	perror("select");
	quit();
	/* NOTREACHED */
    }
    if (FD_ISSET(net, &excepts)) {
	return 1;
    } else {
	return 0;
    }
}


/*
 *  setneturg()
 *
 *	Sets "neturg" to the current location.
 */

void
setneturg(void)
{
    ring_mark(&netoring);
}


/*
 *  netflush
 *		Send as much data as possible to the network,
 *	handling requests for urgent data.
 *
 *		The return value indicates whether we did any
 *	useful work.
 */


int
netflush(void)
{
    int n, n1;

#if	defined(ENCRYPTION)
    if (encrypt_output)
	ring_encrypt(&netoring, encrypt_output);
#endif
    if ((n1 = n = ring_full_consecutive(&netoring)) > 0) {
	if (!ring_at_mark(&netoring)) {
	    n = send(net, (char *)netoring.consume, n, 0); /* normal write */
	} else {
	    /*
	     * In 4.2 (and 4.3) systems, there is some question about
	     * what byte in a sendOOB operation is the "OOB" data.
	     * To make ourselves compatible, we only send ONE byte
	     * out of band, the one WE THINK should be OOB (though
	     * we really have more the TCP philosophy of urgent data
	     * rather than the Unix philosophy of OOB data).
	     */
	    n = send(net, (char *)netoring.consume, 1, MSG_OOB);/* URGENT data */
	}
    }
    if (n < 0) {
	if (errno != ENOBUFS && errno != EWOULDBLOCK) {
	    setcommandmode();
	    perror(hostname);
	    NetClose(net);
	    ring_clear_mark(&netoring);
	    longjmp(peerdied, -1);
	    /*NOTREACHED*/
	}
	n = 0;
    }
    if (netdata && n) {
	Dump('>', netoring.consume, n);
    }
    if (n) {
	ring_consumed(&netoring, n);
	/*
	 * If we sent all, and more to send, then recurse to pick
	 * up the other half.
	 */
	if ((n1 == n) && ring_full_consecutive(&netoring)) {
	    netflush();
	}
	return 1;
    } else {
	return 0;
    }
}
