/*
 * Copyright (c) 1985, 1989, 1993
 *    The Regents of the University of California.  All rights reserved.
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
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
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

/*
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/*
 * Portions Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Portions Copyright (c) 1996-2003 by Internet Software Consortium
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *   Internet Systems Consortium, Inc.
 *   950 Charter Street
 *   Redwood City, CA 94063
 *   <info@isc.org>
 *   http://www.isc.org/
 */

#if defined(LIBC_SCCS) && !defined(lint)
static const char sccsid[] = "@(#)res_send.c	8.1 (Berkeley) 6/4/93";
static const char rcsid[] = "$Id: res_send.c,v 1.7.2.2 2004/06/10 17:59:44 dhankins Exp $";
#endif /* LIBC_SCCS and not lint */

/* Rename the I/O functions in case we're tracing. */
#define send		trace_mr_send
#define recvfrom	trace_mr_recvfrom
#define	read		trace_mr_read
#define connect		trace_mr_connect
#define socket		trace_mr_socket
#define bind		trace_mr_bind
#define close		trace_mr_close
#define select		trace_mr_select
#define time		trace_mr_time

/*
 * Send query to name server and wait for reply.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "minires/minires.h"
#include "arpa/nameser.h"

#define	CHECK_SRVR_ADDR
		
static int cmpsock(struct sockaddr_in *a1, struct sockaddr_in *a2);
void res_pquery(const res_state, const u_char *, int, FILE *);

/* int
 * res_isourserver(ina)
 *	looks up "ina" in _res.ns_addr_list[]
 * returns:
 *	0  : not found
 *	>0 : found
 * author:
 *	paul vixie, 29may94
 */
int
res_ourserver_p(const res_state statp, const struct sockaddr_in *inp) {
	struct sockaddr_in ina;
	int ns;

	ina = *inp;
	for (ns = 0;  ns < statp->nscount;  ns++) {
		const struct sockaddr_in *srv = &statp->nsaddr_list[ns];

		if (srv->sin_family == ina.sin_family &&
		    srv->sin_port == ina.sin_port &&
		    (srv->sin_addr.s_addr == INADDR_ANY ||
		     srv->sin_addr.s_addr == ina.sin_addr.s_addr))
			return (1);
	}
	return (0);
}

/* int
 * res_nameinquery(name, type, class, buf, eom)
 *	look for (name,type,class) in the query section of packet (buf,eom)
 * requires:
 *	buf + HFIXEDSZ <= eom
 * returns:
 *	-1 : format error
 *	0  : not found
 *	>0 : found
 * author:
 *	paul vixie, 29may94
 */
int
res_nameinquery(const char *name, int type, int class,
		const u_char *buf, const u_char *eom)
{
	const u_char *cp = buf + HFIXEDSZ;
	int qdcount = ntohs(((const HEADER *)buf)->qdcount);

	while (qdcount-- > 0) {
		char tname[MAXDNAME+1];
		int n, ttype, tclass;

		n = dn_expand(buf, eom, cp, tname, sizeof tname);
		if (n < 0)
			return (-1);
		cp += n;
		if (cp + 2 * INT16SZ > eom)
			return (-1);
		ttype = getUShort(cp); cp += INT16SZ;
		tclass = getUShort(cp); cp += INT16SZ;
		if (ttype == type && tclass == class &&
		    ns_samename(tname, name) == 1)
			return (1);
	}
	return (0);
}

/* int
 * res_queriesmatch(buf1, eom1, buf2, eom2)
 *	is there a 1:1 mapping of (name,type,class)
 *	in (buf1,eom1) and (buf2,eom2)?
 * returns:
 *	-1 : format error
 *	0  : not a 1:1 mapping
 *	>0 : is a 1:1 mapping
 * author:
 *	paul vixie, 29may94
 */
int
res_queriesmatch(const u_char *buf1, const u_char *eom1,
		 const u_char *buf2, const u_char *eom2)
{
	const u_char *cp = buf1 + HFIXEDSZ;
	int qdcount = ntohs(((const HEADER *)buf1)->qdcount);

	if (buf1 + HFIXEDSZ > eom1 || buf2 + HFIXEDSZ > eom2)
		return (-1);

	/*
	 * Only header section present in replies to
	 * dynamic update packets.
	 */
	if ( (((const HEADER *)buf1)->opcode == ns_o_update) &&
	     (((const HEADER *)buf2)->opcode == ns_o_update) )
		return (1);

	if (qdcount != ntohs(((const HEADER*)buf2)->qdcount))
		return (0);
	while (qdcount-- > 0) {
		char tname[MAXDNAME+1];
		int n, ttype, tclass;

		n = dn_expand(buf1, eom1, cp, tname, sizeof tname);
		if (n < 0)
			return (-1);
		cp += n;
		if (cp + 2 * INT16SZ > eom1)
			return (-1);
		ttype = getUShort(cp);	cp += INT16SZ;
		tclass = getUShort(cp); cp += INT16SZ;
		if (!res_nameinquery(tname, ttype, tclass, buf2, eom2))
			return (0);
	}
	return (1);
}

isc_result_t
res_nsend(res_state statp,
	  double *buf, unsigned buflen,
	  double *ans, unsigned anssiz, unsigned *ansret)
{
	HEADER *hp = (HEADER *) buf;
	HEADER *anhp = (HEADER *) ans;
	int gotsomewhere, connreset, terrno, try, v_circuit, resplen, ns, n;
	u_int badns;	/* XXX NSMAX can't exceed #/bits in this variable */
	static int highestFD = FD_SETSIZE - 1;

	if (anssiz < HFIXEDSZ) {
		return ISC_R_INVALIDARG;
	}
	DprintQ((statp->options & RES_DEBUG) ||
		(statp->pfcode & RES_PRF_QUERY),
		(stdout, ";; res_send()\n"), buf, buflen);
	v_circuit = (statp->options & RES_USEVC) || buflen > PACKETSZ;
	gotsomewhere = 0;
	connreset = 0;
	terrno = ISC_R_TIMEDOUT;
	badns = 0;

	/*
	 * Some callers want to even out the load on their resolver list.
	 */
	if (statp->nscount > 0 && (statp->options & RES_ROTATE) != 0) {
		struct sockaddr_in ina;
		int lastns = statp->nscount - 1;

		ina = statp->nsaddr_list[0];
		for (ns = 0; ns < lastns; ns++)
			statp->nsaddr_list[ns] = statp->nsaddr_list[ns + 1];
		statp->nsaddr_list[lastns] = ina;
	}

#if defined (TRACING)
	trace_mr_statp_setup (statp);
#endif

	/*
	 * Send request, RETRY times, or until successful
	 */
	for (try = 0; try < statp->retry; try++) {
	    for (ns = 0; ns < statp->nscount; ns++) {
		struct sockaddr_in *nsap = &statp->nsaddr_list[ns];
 same_ns:
		if (badns & (1 << ns)) {
			res_nclose(statp);
			goto next_ns;
		}

		if (statp->qhook) {
			int done = 0, loops = 0;

			do {
				res_sendhookact act;

				act = (*statp->qhook)(&nsap, &buf, &buflen,
						      ans, anssiz, &resplen);
				switch (act) {
				case res_goahead:
					done = 1;
					break;
				case res_nextns:
					res_nclose(statp);
					goto next_ns;
				case res_done:
					return (resplen);
				case res_modified:
					/* give the hook another try */
					if (++loops < 42) /*doug adams*/
						break;
					/*FALLTHROUGH*/
				case res_error:
					/*FALLTHROUGH*/
				default:
					return ISC_R_UNEXPECTED;
				}
			} while (!done);
		}

		Dprint(statp->options & RES_DEBUG,
		       (stdout, ";; Querying server (# %d) address = %s\n",
			ns + 1, inet_ntoa(nsap->sin_addr)));

		if (v_circuit) {
			int truncated;
			struct iovec iov[2];
			u_short len;
			u_char *cp;

			/* Use VC; at most one attempt per server. */
			try = statp->retry;
			truncated = 0;

			/* Are we still talking to whom we want to talk to? */
			if (statp->_sock >= 0 &&
			    (statp->_flags & RES_F_VC) != 0) {
				struct sockaddr_in peer;
				SOCKLEN_T size = sizeof(peer);

				if (getpeername(statp->_sock,
						(struct sockaddr *)&peer,
						&size) < 0) {
					res_nclose(statp);
					statp->_flags &= ~RES_F_VC;
				} else if (!cmpsock(&peer, nsap)) {
					res_nclose(statp);
					statp->_flags &= ~RES_F_VC;
				}
			}

			if (statp->_sock < 0 ||
			    (statp->_flags & RES_F_VC) == 0) {
				if (statp->_sock >= 0)
					res_nclose(statp);

				statp->_sock = socket(PF_INET,
						       SOCK_STREAM, 0);
				if (statp->_sock < 0 ||
				    statp->_sock > highestFD) {
					terrno = uerr2isc (errno);
					Perror(statp, stderr,
					       "socket(vc)", errno);
					return (-1);
				}
				errno = 0;
				if (connect(statp->_sock,
					    (struct sockaddr *)nsap,
					    sizeof *nsap) < 0) {
					terrno = uerr2isc (errno);
					Aerror(statp, stderr, "connect/vc",
					       errno, *nsap);
					badns |= (1 << ns);
					res_nclose(statp);
					goto next_ns;
				}
				statp->_flags |= RES_F_VC;
			}
			/*
			 * Send length & message
			 */
			putUShort((u_char*)&len, buflen);
			iov[0].iov_base = (caddr_t)&len;
			iov[0].iov_len = INT16SZ;
			iov[1].iov_base = (const caddr_t)buf;
			iov[1].iov_len = buflen;
			if (writev(statp->_sock, iov, 2) !=
			    (INT16SZ + buflen)) {
				terrno = uerr2isc (errno);
				Perror(statp, stderr, "write failed", errno);
				badns |= (1 << ns);
				res_nclose(statp);
				goto next_ns;
			}
			/*
			 * Receive length & response
			 */
 read_len:
			cp = (u_char *)ans;
			len = INT16SZ;
			while ((n = read(statp->_sock,
					 (char *)cp, (unsigned)len)) > 0) {
				cp += n;
				if ((len -= n) <= 0)
					break;
			}
			if (n <= 0) {
				terrno = uerr2isc (errno);
				Perror(statp, stderr, "read failed", errno);
				res_nclose(statp);
				/*
				 * A long running process might get its TCP
				 * connection reset if the remote server was
				 * restarted.  Requery the server instead of
				 * trying a new one.  When there is only one
				 * server, this means that a query might work
				 * instead of failing.  We only allow one reset
				 * per query to prevent looping.
				 */
				if (terrno == ISC_R_CONNREFUSED &&
				    !connreset) {
					connreset = 1;
					res_nclose(statp);
					goto same_ns;
				}
				res_nclose(statp);
				goto next_ns;
			}
			resplen = getUShort ((unsigned char *)ans);
			if (resplen > anssiz) {
				Dprint(statp->options & RES_DEBUG,
				       (stdout, ";; response truncated\n")
				       );
				truncated = 1;
				len = anssiz;
			} else
				len = resplen;
			if (len < HFIXEDSZ) {
				/*
				 * Undersized message.
				 */
				Dprint(statp->options & RES_DEBUG,
				       (stdout, ";; undersized: %d\n", len));
				terrno = ISC_R_NOSPACE;
				badns |= (1 << ns);
				res_nclose(statp);
				goto next_ns;
			}
			cp = (u_char *)ans;
			while (len != 0 &&
			       (n = read(statp->_sock,
					 (char *)cp, (unsigned)len))
			       > 0) {
				cp += n;
				len -= n;
			}
			if (n <= 0) {
				terrno = uerr2isc (errno);
				Perror(statp, stderr, "read(vc)", errno);
				res_nclose(statp);
				goto next_ns;
			}
			if (truncated) {
				/*
				 * Flush rest of answer
				 * so connection stays in synch.
				 */
				anhp->tc = 1;
				len = resplen - anssiz;
				while (len != 0) {
					char junk[PACKETSZ];

					n = (len > sizeof(junk)
					     ? sizeof(junk)
					     : len);
					n = read(statp->_sock,
						 junk, (unsigned)n);
					if (n > 0)
						len -= n;
					else
						break;
				}
			}
			/*
			 * The calling applicating has bailed out of
			 * a previous call and failed to arrange to have
			 * the circuit closed or the server has got
			 * itself confused. Anyway drop the packet and
			 * wait for the correct one.
			 */
			if (hp->id != anhp->id) {
				DprintQ((statp->options & RES_DEBUG) ||
					(statp->pfcode & RES_PRF_REPLY),
					(stdout,
					 ";; old answer (unexpected):\n"),
					ans, (resplen>anssiz)?anssiz:resplen);
				goto read_len;
			}
		} else {
			/*
			 * Use datagrams.
			 */
			int start, timeout, finish;
			fd_set dsmask;
			struct sockaddr_in from;
			SOCKLEN_T fromlen;
			int seconds;

			if (statp->_sock < 0 ||
			    (statp->_flags & RES_F_VC) != 0) {
				if ((statp->_flags & RES_F_VC) != 0)
					res_nclose(statp);
				statp->_sock = socket(PF_INET, SOCK_DGRAM, 0);
				if (statp->_sock < 0 ||
				    statp->_sock > highestFD) {
#ifndef CAN_RECONNECT
 bad_dg_sock:
#endif
					terrno = uerr2isc (errno);
					Perror(statp, stderr,
					       "socket(dg)", errno);
					return terrno;
				}
				statp->_flags &= ~RES_F_CONN;
			}
#ifndef CANNOT_CONNECT_DGRAM
			/*
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
			if (statp->nscount == 1 || (try == 0 && ns == 0)) {
				/*
				 * Connect only if we are sure we won't
				 * receive a response from another server.
				 */
				if ((statp->_flags & RES_F_CONN) == 0) {
					if (connect(statp->_sock,
						    (struct sockaddr *)nsap,
						    sizeof *nsap) < 0) {
						Aerror(statp, stderr,
						       "connect(dg)",
						       errno, *nsap);
						badns |= (1 << ns);
						res_nclose(statp);
						goto next_ns;
					}
					statp->_flags |= RES_F_CONN;
				}
                              if (send(statp->_sock,
				       (const char*)buf, (unsigned)buflen, 0)
				  != buflen) {
					Perror(statp, stderr, "send", errno);
					badns |= (1 << ns);
					res_nclose(statp);
					goto next_ns;
				}
			} else {
				/*
				 * Disconnect if we want to listen
				 * for responses from more than one server.
				 */
				if ((statp->_flags & RES_F_CONN) != 0) {
#ifdef CAN_RECONNECT
					struct sockaddr_in no_addr;

					no_addr.sin_family = AF_INET;
					no_addr.sin_addr.s_addr = INADDR_ANY;
					no_addr.sin_port = 0;
					(void) connect(statp->_sock,
						       (struct sockaddr *)
						        &no_addr,
						       sizeof no_addr);
#else
					struct sockaddr_in local_addr;
					SOCKLEN_T len;
					int result, s1;

					len = sizeof(local_addr);
					s1 = socket(PF_INET, SOCK_DGRAM, 0);
					result = getsockname(statp->_sock,
						(struct sockaddr *)&local_addr,
							     &len);
					if (s1 < 0)
						goto bad_dg_sock;
					(void) dup2(s1, statp->_sock);
					(void) close(s1);
					if (result == 0) {
						/*
						 * Attempt to rebind to old
						 * port.  Note connected socket
						 * has an sin_addr set.
						 */
						local_addr.sin_addr.s_addr =
							htonl(0);
						(void)bind(statp->_sock,
							   (struct sockaddr *)
							   &local_addr,
							   (unsigned)len);
					}
					Dprint(statp->options & RES_DEBUG,
					       (stdout, ";; new DG socket\n"));
#endif /* CAN_RECONNECT */
					statp->_flags &= ~RES_F_CONN;
					errno = 0;
				}
#endif /* !CANNOT_CONNECT_DGRAM */
				if (sendto(statp->_sock,
					   (const char *)buf, buflen, 0,
					   (struct sockaddr *)nsap,
					   sizeof *nsap)
				    != buflen) {
					Aerror(statp, stderr, "sendto", errno, *nsap);
					badns |= (1 << ns);
					res_nclose(statp);
					goto next_ns;
				}
#ifndef CANNOT_CONNECT_DGRAM
			}
#endif /* !CANNOT_CONNECT_DGRAM */

			if (statp->_sock < 0 || statp->_sock > highestFD) {
				Perror(statp, stderr,
				       "fd out-of-bounds", EMFILE);
				res_nclose(statp);
				goto next_ns;
			}

			/*
			 * Wait for reply
			 */
			seconds = (statp->retrans << try);
			if (try > 0)
				seconds /= statp->nscount;
			if (seconds <= 0)
				seconds = 1;
			start = cur_time;
			timeout = seconds;
			finish = start + timeout;
 wait:
			FD_ZERO(&dsmask);
			FD_SET(statp->_sock, &dsmask);
			{
				struct timeval t;
				t.tv_sec = timeout;
				t.tv_usec = 0;
				n = select(statp->_sock + 1,
					   &dsmask, NULL, NULL, &t);
			}
			if (n == 0) {
				Dprint(statp->options & RES_DEBUG,
				       (stdout, ";; timeout\n"));
				gotsomewhere = 1;
				goto next_ns;
			}
			if (n < 0) {
				if (errno == EINTR) {
					if (finish >= cur_time) {
						timeout = finish - cur_time;
						goto wait;
					}
				}
				Perror(statp, stderr, "select", errno);
				res_nclose(statp);
				goto next_ns;
			}
			errno = 0;
			fromlen = sizeof(struct sockaddr_in);
			resplen = recvfrom(statp->_sock,
					   (char *)ans, anssiz, 0,
					   (struct sockaddr *)&from, &fromlen);
			if (resplen <= 0) {
				Perror(statp, stderr, "recvfrom", errno);
				res_nclose(statp);
				goto next_ns;
			}
			gotsomewhere = 1;
			if (resplen < HFIXEDSZ) {
				/*
				 * Undersized message.
				 */
				Dprint(statp->options & RES_DEBUG,
				       (stdout, ";; undersized: %d\n",
					resplen));
				terrno = ISC_R_NOSPACE;
				badns |= (1 << ns);
				res_nclose(statp);
				goto next_ns;
			}
			if (hp->id != anhp->id) {
				/*
				 * response from old query, ignore it.
				 * XXX - potential security hazard could
				 *	 be detected here.
				 */
				DprintQ((statp->options & RES_DEBUG) ||
					(statp->pfcode & RES_PRF_REPLY),
					(stdout, ";; old answer:\n"),
					ans, (resplen>anssiz)?anssiz:resplen);
				goto wait;
			}
#ifdef CHECK_SRVR_ADDR
			if (!(statp->options & RES_INSECURE1) &&
			    !res_ourserver_p(statp, &from)) {
				/*
				 * response from wrong server? ignore it.
				 * XXX - potential security hazard could
				 *	 be detected here.
				 */
				DprintQ((statp->options & RES_DEBUG) ||
					(statp->pfcode & RES_PRF_REPLY),
					(stdout, ";; not our server:\n"),
					ans, (resplen>anssiz)?anssiz:resplen);
				goto wait;
			}
#endif
			if (!(statp->options & RES_INSECURE2) &&
			    !res_queriesmatch((u_char *)buf,
					      ((u_char *)buf) + buflen,
					      (u_char *)ans,
					      ((u_char *)ans) + anssiz)) {
				/*
				 * response contains wrong query? ignore it.
				 * XXX - potential security hazard could
				 *	 be detected here.
				 */
				DprintQ((statp->options & RES_DEBUG) ||
					(statp->pfcode & RES_PRF_REPLY),
					(stdout, ";; wrong query name:\n"),
					ans, (resplen>anssiz)?anssiz:resplen);
				goto wait;
			}
			if (anhp->rcode == SERVFAIL ||
			    anhp->rcode == NOTIMP ||
			    anhp->rcode == REFUSED) {
				DprintQ(statp->options & RES_DEBUG,
					(stdout, "server rejected query:\n"),
					ans, (resplen>anssiz)?anssiz:resplen);
				badns |= (1 << ns);
				res_nclose(statp);
				/* don't retry if called from dig */
				if (!statp->pfcode)
					goto next_ns;
			}
			if (!(statp->options & RES_IGNTC) && anhp->tc) {
				/*
				 * get rest of answer;
				 * use TCP with same server.
				 */
				Dprint(statp->options & RES_DEBUG,
				       (stdout, ";; truncated answer\n"));
				v_circuit = 1;
				res_nclose(statp);
				goto same_ns;
			}
		} /*if vc/dg*/
		Dprint((statp->options & RES_DEBUG) ||
		       ((statp->pfcode & RES_PRF_REPLY) &&
			(statp->pfcode & RES_PRF_HEAD1)),
		       (stdout, ";; got answer:\n"));
		DprintQ((statp->options & RES_DEBUG) ||
			(statp->pfcode & RES_PRF_REPLY),
			(stdout, ""),
			ans, (resplen>anssiz)?anssiz:resplen);
		/*
		 * If using virtual circuits, we assume that the first server
		 * is preferred over the rest (i.e. it is on the local
		 * machine) and only keep that one open.
		 * If we have temporarily opened a virtual circuit,
		 * or if we haven't been asked to keep a socket open,
		 * close the socket.
		 */
		if ((v_circuit && (!(statp->options & RES_USEVC) || ns != 0)) ||
		    !(statp->options & RES_STAYOPEN)) {
			res_nclose(statp);
		}
		if (statp->rhook) {
			int done = 0, loops = 0;

			do {
				res_sendhookact act;

				act = (*statp->rhook)(nsap, buf, buflen,
						      ans, anssiz, &resplen);
				switch (act) {
				case res_goahead:
				case res_done:
					done = 1;
					break;
				case res_nextns:
					res_nclose(statp);
					goto next_ns;
				case res_modified:
					/* give the hook another try */
					if (++loops < 42) /*doug adams*/
						break;
					/*FALLTHROUGH*/
				case res_error:
					/*FALLTHROUGH*/
				default:
					return ISC_R_UNEXPECTED;
				}
			} while (!done);

		}
		*ansret = resplen;
		return ISC_R_SUCCESS;
 next_ns: ;
	   } /*foreach ns*/
	} /*foreach retry*/
	res_nclose(statp);
	if (!v_circuit) {
		if (!gotsomewhere)
			terrno = ISC_R_CONNREFUSED;  /* no nameservers found */
		else
			errno = ISC_R_TIMEDOUT;	/* no answer obtained */
	}
	return terrno;
}

/*
 * This routine is for closing the socket if a virtual circuit is used and
 * the program wants to close it.  This provides support for endhostent()
 * which expects to close the socket.
 *
 * This routine is not expected to be user visible.
 */
void
res_nclose(res_state statp) {
	if (statp->_sock >= 0) {
		(void) close(statp->_sock);
		statp->_sock = -1;
		statp->_flags &= ~(RES_F_VC | RES_F_CONN);
	}
}

/* Private */
static int
cmpsock(struct sockaddr_in *a1, struct sockaddr_in *a2) {
	return ((a1->sin_family == a2->sin_family) &&
		(a1->sin_port == a2->sin_port) &&
		(a1->sin_addr.s_addr == a2->sin_addr.s_addr));
}

#ifdef NEED_PSELECT
/* XXX needs to move to the porting library. */
static int
pselect(int nfds, void *rfds, void *wfds, void *efds,
	struct timespec *tsp,
	const sigset_t *sigmask)
{
	struct timeval tv, *tvp;
	sigset_t sigs;
	int n;

	if (tsp) {
		tvp = &tv;
		tv = evTimeVal(*tsp);
	} else
		tvp = NULL;
	if (sigmask)
		sigprocmask(SIG_SETMASK, sigmask, &sigs);
	n = select(nfds, rfds, wfds, efds, tvp);
	if (sigmask)
		sigprocmask(SIG_SETMASK, &sigs, NULL);
	if (tsp)
		*tsp = evTimeSpec(tv);
	return (n);
}
#endif
