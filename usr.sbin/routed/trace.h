/*
 * Copyright (c) 1983, 1988, 1993
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
 *
 *	@(#)trace.h	8.1 (Berkeley) 6/5/93
 */

/*
 * Routing table management daemon.
 */

/*
 * Trace record format.
 */
struct	iftrace {
	struct	timeval ift_stamp;	/* time stamp */
	struct	sockaddr ift_who;	/* from/to */
	char	*ift_packet;		/* pointer to packet */
	short	ift_size;		/* size of packet */
	short	ift_metric;		/* metric on associated metric */
};

/*
 * Per interface packet tracing buffers.  An incoming and
 * outgoing circular buffer of packets is maintained, per
 * interface, for debugging.  Buffers are dumped whenever
 * an interface is marked down.
 */
struct	ifdebug {
	struct	iftrace *ifd_records;	/* array of trace records */
	struct	iftrace *ifd_front;	/* next empty trace record */
	int	ifd_count;		/* number of unprinted records */
	struct	interface *ifd_if;	/* for locating stuff */
};

/*
 * Packet tracing stuff.
 */
int	tracepackets;		/* watch packets as they go by */
int	tracecontents;		/* watch packet contents as they go by */
int	traceactions;		/* on/off */
int	tracehistory;		/* on/off */
FILE	*ftrace;		/* output trace file */

#define	TRACE_ACTION(action, route) { \
	  if (traceactions) \
		traceaction(ftrace, action, route); \
	}
#define	TRACE_NEWMETRIC(route, newmetric) { \
	  if (traceactions) \
		tracenewmetric(ftrace, route, newmetric); \
	}
#define	TRACE_INPUT(ifp, src, pack, size) { \
	  if (tracehistory) { \
		ifp = if_iflookup(src); \
		if (ifp) \
			trace(&ifp->int_input, src, pack, size, \
				ntohl(ifp->int_metric)); \
	  } \
	  if (tracepackets) \
		dumppacket(ftrace, "from", src, pack, size, &now); \
	}
#define	TRACE_OUTPUT(ifp, dst, size) { \
	  if (tracehistory && ifp) \
		trace(&ifp->int_output, dst, packet, size, ifp->int_metric); \
	  if (tracepackets) \
		dumppacket(ftrace, "to", dst, packet, size, &now); \
	}
