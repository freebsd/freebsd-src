/*
 * Copyright (c) 1985, 1989 Regents of the University of California.
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
static char sccsid[] = "@(#)send.c	5.18 (Berkeley) 3/2/91";
#endif /* not lint */

/*
 *******************************************************************************
 *
 *  send.c --
 *
 *	Routine to send request packets to a name server.
 *
 *	Based on "@(#)res_send.c  6.25 (Berkeley) 6/1/90".
 *
 *******************************************************************************
 */


/*
 * Send query to name server and wait for reply.
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <stdio.h>
#include <errno.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <resolv.h>
#include "res.h"

extern int errno;

static int s = -1;	/* socket used for communications */


#ifndef FD_SET
#define	NFDBITS		32
#define	FD_SETSIZE	32
#define	FD_SET(n, p)	((p)->fds_bits[(n)/NFDBITS] |= (1 << ((n) % NFDBITS)))
#define	FD_CLR(n, p)	((p)->fds_bits[(n)/NFDBITS] &= ~(1 << ((n) % NFDBITS)))
#define	FD_ISSET(n, p)	((p)->fds_bits[(n)/NFDBITS] & (1 << ((n) % NFDBITS)))
#define FD_ZERO(p)	bzero((char *)(p), sizeof(*(p)))
#endif

#define SR 1		/* SendRequest style */

#ifndef DEBUG
#define DEBUG
#endif

unsigned short nsport = NAMESERVER_PORT;



/*
 *******************************************************************************
 *
 *   SendRequest --
 *
 *	Sends a request packet to a name server whose address
 *	is specified by the first argument and returns with
 *	the answer packet.
 *
 *  Results:
 *	SUCCESS		- the request was sent and an answer
 *			  was received.
 *	TIME_OUT	- the virtual circuit connection timed-out
 *			  or a reply to a datagram wasn't received.
 *
 *
 *******************************************************************************
 */

int
SendRequest(nsAddrPtr, buf, buflen, answer, anslen, trueLenPtr)
	struct in_addr	*nsAddrPtr;
	char		*buf;
	int		buflen;
	char		*answer;
	u_int		anslen;
	int		*trueLenPtr;
{
	register int n;
	int try, v_circuit, resplen, ns;
	int gotsomewhere = 0, connected = 0;
	int connreset = 0;
	u_short id, len;
	char *cp;
	fd_set dsmask;
	struct timeval timeout;
	HEADER *hp = (HEADER *) buf;
	HEADER *anhp = (HEADER *) answer;
	struct iovec iov[2];
	int terrno = ETIMEDOUT;
	char junk[512];

#if SR
	struct sockaddr_in sin;

	if (_res.options & RES_DEBUG2) {
	    printf("------------\nSendRequest(), len %d\n", buflen);
	    Print_query(buf, buf+buflen, 1);
	}
	sin.sin_family	= AF_INET;
	sin.sin_port	= htons(nsport);
	sin.sin_addr	= *nsAddrPtr;
#else
#ifdef DEBUG
	if (_res.options & RES_DEBUG) {
		printf("res_send()\n");
		p_query(buf);
	}
#endif DEBUG
	if (!(_res.options & RES_INIT))
		if (res_init() == -1) {
			return(-1);
		}
#endif /* SR */
	v_circuit = (_res.options & RES_USEVC) || buflen > PACKETSZ;
	id = hp->id;
	/*
	 * Send request, RETRY times, or until successful
	 */
	for (try = 0; try < _res.retry; try++) {
#if !SR
	   for (ns = 0; ns < _res.nscount; ns++) {
#ifdef DEBUG
		if (_res.options & RES_DEBUG)
			printf("Querying server (# %d) address = %s\n", ns+1,
			      inet_ntoa(_res.nsaddr_list[ns].sin_addr));
#endif DEBUG
#endif /* !SR */
	usevc:
		if (v_circuit) {
			int truncated = 0;

			/*
			 * Use virtual circuit;
			 * at most one attempt per server.
			 */
			try = _res.retry;
			if (s < 0) {
				s = socket(AF_INET, SOCK_STREAM, 0);
				if (s < 0) {
					terrno = errno;
#ifdef DEBUG
					if (_res.options & RES_DEBUG)
					    perror("socket (vc) failed");
#endif DEBUG
					continue;
				}
#if SR
				if (connect(s, (struct sockaddr *)&sin,
#else
				if (connect(s,
				    (struct sockaddr *)&(_res.nsaddr_list[ns]),
#endif
				   sizeof(struct sockaddr)) < 0) {
					terrno = errno;
#ifdef DEBUG
					if (_res.options & RES_DEBUG)
					    perror("connect failed");
#endif DEBUG
					(void) close(s);
					s = -1;
					continue;
				}
			}
			/*
			 * Send length & message
			 */
			len = htons((u_short)buflen);
			iov[0].iov_base = (caddr_t)&len;
			iov[0].iov_len = sizeof(len);
			iov[1].iov_base = buf;
			iov[1].iov_len = buflen;
			if (writev(s, iov, 2) != sizeof(len) + buflen) {
				terrno = errno;
#ifdef DEBUG
				if (_res.options & RES_DEBUG)
					perror("write failed");
#endif DEBUG
				(void) close(s);
				s = -1;
				continue;
			}
			/*
			 * Receive length & response
			 */
			cp = answer;
			len = sizeof(short);
			while (len != 0 &&
			    (n = read(s, (char *)cp, (int)len)) > 0) {
				cp += n;
				len -= n;
			}
			if (n <= 0) {
				terrno = errno;
#ifdef DEBUG
				if (_res.options & RES_DEBUG)
					perror("read failed");
#endif DEBUG
				(void) close(s);
				s = -1;
				/*
				 * A long running process might get its TCP
				 * connection reset if the remote server was
				 * restarted.  Requery the server instead of
				 * trying a new one.  When there is only one
				 * server, this means that a query might work
				 * instead of failing.  We only allow one reset
				 * per query to prevent looping.
				 */
				if (terrno == ECONNRESET && !connreset) {
					connreset = 1;
					ns--;
				}
				continue;
			}
			cp = answer;
			if ((resplen = ntohs(*(u_short *)cp)) > anslen) {
#ifdef DEBUG
				if (_res.options & RES_DEBUG)
					fprintf(stderr, "response truncated\n");
#endif DEBUG
				len = anslen;
				truncated = 1;
			} else
				len = resplen;
			while (len != 0 &&
			   (n = read(s, (char *)cp, (int)len)) > 0) {
				cp += n;
				len -= n;
			}
			if (n <= 0) {
				terrno = errno;
#ifdef DEBUG
				if (_res.options & RES_DEBUG)
					perror("read failed");
#endif DEBUG
				(void) close(s);
				s = -1;
				continue;
			}
			if (truncated) {
				/*
				 * Flush rest of answer
				 * so connection stays in synch.
				 */
				anhp->tc = 1;
				len = resplen - anslen;
				while (len != 0) {
					n = (len > sizeof(junk) ?
					    sizeof(junk) : len);
					if ((n = read(s, junk, n)) > 0)
						len -= n;
					else
						break;
				}
			}
		} else {
			/*
			 * Use datagrams.
			 */
			if (s < 0) {
				s = socket(AF_INET, SOCK_DGRAM, 0);
				if (s < 0) {
					terrno = errno;
#ifdef DEBUG
					if (_res.options & RES_DEBUG)
					    perror("socket (dg) failed");
#endif DEBUG
					continue;
				}
			}
#if SR
			/*
			 * Special case the send code below
			 * since we have just 1 server.
			 */
#if BSD >= 43
				if (connected == 0) {
					if (connect(s, (struct sockaddr *)&sin,
					    sizeof(struct sockaddr)) < 0) {
						if (_res.options & RES_DEBUG)
							perror("connect");
						continue;
					}
					connected = 1;
				}
				if (send(s, buf, buflen, 0) != buflen) {
					if (_res.options & RES_DEBUG)
						perror("send");
					continue;
				}
#else /* BSD */
				if (sendto(s, buf, buflen, 0, &sin,
				    sizeof(struct sockaddr)) != buflen) {
					if (_res.options & RES_DEBUG)
						perror("sendto");
					continue;
				}
#endif
#else /* SR */
#if	BSD >= 43
			/*
			 * I'm tired of answering this question, so:
			 * On a 4.3BSD+ machine (client and server,
			 * actually), sending to a nameserver datagram
			 * port with no nameserver will cause an
			 * ICMP port unreachable message to be returned.
			 * If our datagram socket is "connected" to the
			 * server, we get an ECONNREFUSED error on the next
			 * socket operation, and select returns if the
			 * error message is received.  We can thus detect
			 * the absence of a nameserver without timing out.
			 * If we have sent queries to at least two servers,
			 * however, we don't want to remain connected,
			 * as we wish to receive answers from the first
			 * server to respond.
			 */
			if (_res.nscount == 1 || (try == 0 && ns == 0)) {
				/*
				 * Don't use connect if we might
				 * still receive a response
				 * from another server.
				 */
				if (connected == 0) {
					if (connect(s, &_res.nsaddr_list[ns],
					    sizeof(struct sockaddr)) < 0) {
#ifdef DEBUG
						if (_res.options & RES_DEBUG)
							perror("connect");
#endif DEBUG
						continue;
					}
					connected = 1;
				}
				if (send(s, buf, buflen, 0) != buflen) {
#ifdef DEBUG
					if (_res.options & RES_DEBUG)
						perror("send");
#endif DEBUG
					continue;
				}
			} else {
				/*
				 * Disconnect if we want to listen
				 * for responses from more than one server.
				 */
				if (connected) {
					(void) connect(s, &no_addr,
					    sizeof(no_addr));
					connected = 0;
				}
#endif BSD
				if (sendto(s, buf, buflen, 0,
				    &_res.nsaddr_list[ns],
				    sizeof(struct sockaddr)) != buflen) {
#ifdef DEBUG
					if (_res.options & RES_DEBUG)
						perror("sendto");
#endif DEBUG
					continue;
				}
#if	BSD >= 43
			}
#endif
#endif /* SR */

			/*
			 * Wait for reply
			 */
			timeout.tv_sec = (_res.retrans << try);
#if !SR
			if (try > 0)
				timeout.tv_sec /= _res.nscount;
#endif /* SR */
			if (timeout.tv_sec <= 0)
				timeout.tv_sec = 1;
			timeout.tv_usec = 0;
wait:
			FD_ZERO(&dsmask);
			FD_SET(s, &dsmask);
			n = select(s+1, &dsmask, (fd_set *)NULL,
				(fd_set *)NULL, &timeout);
			if (n < 0) {
#ifdef DEBUG
				if (_res.options & RES_DEBUG)
					perror("select");
#endif DEBUG
				continue;
			}
			if (n == 0) {
				/*
				 * timeout
				 */
#ifdef DEBUG
				if (_res.options & RES_DEBUG)
					printf("timeout (%d secs)\n", 
						timeout.tv_sec);
#endif DEBUG
#if BSD >= 43
				gotsomewhere = 1;
#endif
				continue;
			}
			if ((resplen = recv(s, answer, anslen, 0)) <= 0) {
#ifdef DEBUG
				if (_res.options & RES_DEBUG)
					perror("recvfrom");
#endif DEBUG
				continue;
			}
			gotsomewhere = 1;
			if (id != anhp->id) {
				/*
				 * response from old query, ignore it
				 */
#if SR
				if (_res.options & RES_DEBUG2) {
					printf("------------\nOld answer:\n");
					Print_query(answer, answer+resplen, 1);
				}
#else
#ifdef DEBUG
				if (_res.options & RES_DEBUG) {
					printf("old answer:\n");
					p_query(answer);
				}
#endif DEBUG
#endif
				goto wait;
			}
			if (!(_res.options & RES_IGNTC) && anhp->tc) {
				/*
				 * get rest of answer;
				 * use TCP with same server.
				 */
#ifdef DEBUG
				if (_res.options & RES_DEBUG)
					printf("truncated answer\n");
#endif DEBUG
				(void) close(s);
				s = -1;
				v_circuit = 1;
				goto usevc;
			}
		}
#if SR
		if (_res.options & RES_DEBUG) {
		    if (_res.options & RES_DEBUG2)
			printf("------------\nGot answer (%d bytes):\n",
			    resplen);
		    else
			printf("------------\nGot answer:\n");
		    Print_query(answer, answer+resplen, 1);
		}
		(void) close(s);
		s = -1;
		*trueLenPtr = resplen;
		return (SUCCESS);
#else
#ifdef DEBUG
		if (_res.options & RES_DEBUG) {
			printf("got answer:\n");
			p_query(answer);
		}
#endif DEBUG
		/*
		 * If using virtual circuits, we assume that the first server
		 * is preferred * over the rest (i.e. it is on the local
		 * machine) and only keep that one open.
		 * If we have temporarily opened a virtual circuit,
		 * or if we haven't been asked to keep a socket open,
		 * close the socket.
		 */
		if ((v_circuit &&
		    ((_res.options & RES_USEVC) == 0 || ns != 0)) ||
		    (_res.options & RES_STAYOPEN) == 0) {
			(void) close(s);
			s = -1;
		}
		return (resplen);
	   }
#endif /* SR */
	}
	if (s >= 0) {
		(void) close(s);
		s = -1;
	}
#if SR
	if (v_circuit == 0)
		if (gotsomewhere == 0)
			return NO_RESPONSE;	/* no nameservers found */
		else
			return TIME_OUT;	/* no answer obtained */
	else
		if (errno == ECONNREFUSED)
			return NO_RESPONSE;
		else
			return ERROR;
#else
	if (v_circuit == 0)
		if (gotsomewhere == 0)
			errno = ECONNREFUSED;	/* no nameservers found */
		else
			errno = ETIMEDOUT;	/* no answer obtained */
	else
		errno = terrno;
	return (-1);
#endif
}

/*
 * This routine is for closing the socket if a virtual circuit is used and
 * the program wants to close it.
 *
 * Called from the interrupt handler.
 */
SendRequest_close()
{
	if (s != -1) {
		(void) close(s);
		s = -1;
	}
}
