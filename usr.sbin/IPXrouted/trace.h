/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Copyright (c) 1995 John Hay.  All rights reserved.
 *
 * This file includes significant work done at Cornell University by
 * Bill Nesheim.  That work included by permission.
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
 *
 *	$Id: trace.h,v 1.5 1997/02/22 16:01:04 peter Exp $
 */

/*
 * IPX Routing Information Protocol.
 */

/*
 * Trace record format.
 */
struct	iftrace {
	time_t	ift_stamp;		/* time stamp */
	struct	sockaddr ift_who;	/* from/to */
	char	*ift_packet;		/* pointer to packet */
	short	ift_size;		/* size of packet */
	short	ift_metric;		/* metric  */
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
int	tracing;		/* on/off */
FILE	*ftrace;		/* output trace file */

#define	TRACE_ACTION(action, route) { \
	  if (tracing) \
		traceaction(ftrace, "action", route); \
	  traceactionlog(action, route); \
	}
#define TRACE_SAP_ACTION(action, service) { \
	  tracesapactionlog(action, service); \
	}
#define	TRACE_INPUT(ifp, src, size) { \
	  if (tracing) { \
		ifp = if_iflookup(src); \
		if (ifp) \
			trace(&ifp->int_input, src, \
				&packet[sizeof(struct ipx)], size, \
				ntohl(ifp->int_metric)); \
	  } \
	  if (tracepackets && ftrace) \
		dumppacket(ftrace, "from", src, \
				&packet[sizeof(struct ipx)], size); \
	}
#define	TRACE_OUTPUT(ifp, dst, size) { \
	  if (tracing) { \
		ifp = if_iflookup(dst); \
		if (ifp) \
		    trace(&ifp->int_output, dst, \
				&packet[sizeof(struct ipx)], \
				size, ifp->int_metric); \
	  } \
	  if (tracepackets && ftrace) \
		dumppacket(ftrace, "to", dst, \
				&packet[sizeof(struct ipx)], size); \
	}

#define	TRACE_SAP_OUTPUT(ifp, dst, size) { \
	  if (tracing) { \
		ifp = if_iflookup(dst); \
		if (ifp) \
		    trace(&ifp->int_output, dst, \
				&packet[sizeof(struct ipx)], \
				size, ifp->int_metric); \
	  } \
	  if (tracepackets && ftrace) \
		dumpsappacket(ftrace, "to", dst, \
				&packet[sizeof(struct ipx)], size); \
	}

void traceinit(struct interface *);
void traceon(char *file);
void traceoff(void);
void traceaction(FILE *, char *, struct rt_entry *);
void traceactionlog(char *, struct rt_entry *);
void tracesapactionlog(char *action, struct sap_entry *sap);
void trace(struct ifdebug *, struct sockaddr *, char *, int, int);
void dumppacket(FILE *, char *, struct sockaddr *, char *, int);
void dumpsappacket(FILE *, char *, struct sockaddr *, char *, int);
void dumpsaptable(FILE *fd, struct sap_hash *sh);
void dumpriptable(FILE *fd);

char *ipxdp_nettoa(union ipx_net);
char *ipxdp_ntoa(struct ipx_addr *);

