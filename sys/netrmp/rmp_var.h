/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah $Hdr: rmp_var.h 1.3 89/06/07$
 *
 *	@(#)rmp_var.h	7.1 (Berkeley) 5/8/90
 */

#ifndef _NETRMP_RMP_VAR_H_
#define _NETRMP_RMP_VAR_H_ 1

/*
 *  WARNING: rmp_packet is defined assuming alignment on 16-bit boundaries.
 *  Data will be contiguous on HP's (MC68000), but there may be holes if
 *  this is used elsewhere (e.g. VAXen).  Or, in other words:
 *
 *  if (sizeof(struct rmp_packet) != 1504) error("AlignmentProblem");
 */

/*
 *  Possible values for "rmp_type" fields.
 */

#define	RMP_BOOT_REQ	1	/* boot request packet */
#define	RMP_BOOT_REPL	129	/* boot reply packet */
#define	RMP_READ_REQ	2	/* read request packet */
#define	RMP_READ_REPL	130	/* read reply packet */
#define	RMP_BOOT_DONE	3	/* boot complete packet */

/*
 *  Useful constants.
 */

#define RMP_VERSION	2	/* protocol version */
#define RMP_TIMEOUT	600	/* timeout connection after ten minutes */
#define	RMP_PROBESID	0xffff	/* session ID for probes */
#define	RMP_HOSTLEN	13	/* max length of server's name */

/*
 *  RMP error codes
 */

#define	RMP_E_OKAY	0
#define	RMP_E_EOF	2	/* read reply: returned end of file */
#define	RMP_E_ABORT	3	/* abort operation */
#define	RMP_E_BUSY	4	/* boot reply: server busy */
#define	RMP_E_TIMEOUT	5	/* lengthen time out (not implemented) */
#define	RMP_E_NOFILE	16	/* boot reply: file does not exist */
#define RMP_E_OPENFILE	17	/* boot reply: file open failed */
#define	RMP_E_NODFLT	18	/* boot reply: default file does not exist */
#define RMP_E_OPENDFLT	19	/* boot reply: default file open failed */
#define	RMP_E_BADSID	25	/* read reply: bad session ID */
#define RMP_E_BADPACKET	27 	/* Bad packet detected */

/*
 *  Assorted field sizes.
 */

#define	RMPADDRLEN	6	/* size of ethernet address */
#define RMPLENGTHLEN	2	/* size of ethernet length field (802.3) */
#define	RMPMACHLEN	20	/* length of machine type field */

/*
 *  RMPDATALEN is the maximum number of data octets that can be stuffed
 *  into an RMP packet.  This excludes the 802.2 LLC w/HP extensions.
 */

#define RMPDATALEN	(RMP_MAX_PACKET - (2*RMPADDRLEN + RMPLENGTHLEN + \
			                   sizeof(struct hp_llc)) )

/*
 *  Define sizes of packets we send.  Boot and Read replies are variable
 *  in length depending on the length of `s'.
 *
 *  Also, define how much space `restofpkt' can take up for outgoing
 *  Boot and Read replies.  Boot Request packets are effectively
 *  limited to 255 bytes due to the preceding 1-byte length field.
 */

#define	RMPBOOTSIZE(s)	(sizeof(struct hp_llc) + sizeof(struct ifnet *) + \
			 sizeof(struct rmp_boot_repl) + s - \
			 sizeof(restofpkt))
#define	RMPREADSIZE(s)	(sizeof(struct hp_llc) + sizeof(struct ifnet *) + \
			 sizeof(struct rmp_read_repl) + s - \
			 sizeof(restofpkt) - sizeof(u_char))
#define	RMPDONESIZE	(sizeof(struct hp_llc) + sizeof(struct ifnet *) + \
			 sizeof(struct rmp_boot_done))
#define	RMPBOOTDATA	255
#define	RMPREADDATA	(RMPDATALEN - \
			 (2*sizeof(u_char)+sizeof(u_short)+sizeof(u_long)))


/*
 * This protocol defines some field sizes as "rest of ethernet packet".
 * There is no easy way to specify this in C, so we use a one character
 * field to denote it, and index past it to the end of the packet.
 */

typedef char	restofpkt;

/*
 * Packet structures.
 */

struct rmp_raw {
	u_char	rmp_type;		/* packet type */
	u_char	rmp_rawdata[RMPDATALEN-1];
};

struct rmp_boot_req {		/* boot request */
	u_char	rmp_type;		/* packet type (RMP_BOOT_REQ) */
	u_char	rmp_retcode;		/* return code (0) */
	u_long	rmp_seqno;		/* sequence number (real time clock) */
	u_short	rmp_session;		/* session id (normally 0) */
	u_short	rmp_version;		/* protocol version (RMP_VERSION) */
	char	rmp_machtype[RMPMACHLEN];	/* machine type */
	u_char	rmp_flnmsize;		/* length of rmp_flnm */
	restofpkt rmp_flnm;		/* name of file to be read */
};

struct rmp_boot_repl {		/* boot reply */
	u_char	rmp_type;		/* packet type (RMP_BOOT_REPL) */
	u_char	rmp_retcode;		/* return code (normally 0) */
	u_long	rmp_seqno;		/* sequence number (from boot req) */
	u_short	rmp_session;		/* session id (generated) */
	u_short	rmp_version;		/* protocol version (RMP_VERSION) */
	u_char	rmp_flnmsize;		/* length of rmp_flnm */
	restofpkt rmp_flnm;		/* name of file (from boot req) */
};

struct rmp_read_req {		/* read request */
	u_char	rmp_type;		/* packet type (RMP_READ_REQ) */
	u_char	rmp_retcode;		/* return code (0) */
	u_long	rmp_offset;		/* file relative byte offset */
	u_short	rmp_session;		/* session id (from boot repl) */
	u_short	rmp_size;		/* max no of bytes to send */
};

struct rmp_read_repl {		/* read reply */
	u_char	rmp_type;		/* packet type (RMP_READ_REPL) */
	u_char	rmp_retcode;		/* return code (normally 0) */
	u_long	rmp_offset;		/* byte offset (from read req) */
	u_short	rmp_session;		/* session id (from read req) */
	restofpkt rmp_data;		/* data (max size from read req) */
	u_char	rmp_unused;		/* padding to 16-bit boundary */
};

struct rmp_boot_done {		/* boot complete */
	u_char	rmp_type;		/* packet type (RMP_BOOT_DONE) */
	u_char	rmp_retcode;		/* return code (0) */
	u_long	rmp_unused;		/* not used (0) */
	u_short	rmp_session;		/* session id (from read repl) */
};

struct rmp_packet {
	struct ifnet *ifp;	/* ptr to intf packet arrived on */
	struct hp_llc hp_llc;
	union {
		struct rmp_boot_req	rmp_brq;	/* boot request */
		struct rmp_boot_repl	rmp_brpl;	/* boot reply */
		struct rmp_read_req	rmp_rrq;	/* read request */
		struct rmp_read_repl	rmp_rrpl;	/* read reply */
		struct rmp_boot_done	rmp_done;	/* boot complete */
		struct rmp_raw		rmp_raw;	/* raw data */
	} rmp_proto;
};

/*
 *  Make life easier...
 */

#define	r_type	rmp_proto.rmp_raw.rmp_type
#define	r_data	rmp_proto.rmp_raw.rmp_data
#define	r_brq	rmp_proto.rmp_brq
#define	r_brpl	rmp_proto.rmp_brpl
#define	r_rrq	rmp_proto.rmp_rrq
#define	r_rrpl	rmp_proto.rmp_rrpl
#define	r_done	rmp_proto.rmp_done

/*
 *  RMP socket address: just family & destination addr.
 */

struct sockaddr_rmp {
	short	srmp_family;		/* address family (AF_RMP) */
	u_char	srmp_dhost[RMPADDRLEN];	/* ethernet destination addr */
};

#endif /* _NETRMP_RMP_VAR_H_ */
