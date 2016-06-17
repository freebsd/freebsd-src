/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1997, 1999, 2000, 2001 Ralf Baechle
 * Copyright (C) 2000, 2001 Silicon Graphics, Inc.
 */
#ifndef _ASM_SOCKET_H
#define _ASM_SOCKET_H

#include <asm/sockios.h>

/*
 * For setsockopt(2)
 *
 * This defines are ABI conformant as far as Linux supports these ...
 */
#define SOL_SOCKET	0xffff

#define SO_DEBUG	0x0001	/* Record debugging information.  */
#define SO_REUSEADDR	0x0004	/* Allow reuse of local addresses.  */
#define SO_KEEPALIVE	0x0008	/* Keep connections alive and send
				   SIGPIPE when they die.  */
#define SO_DONTROUTE	0x0010	/* Don't do local routing.  */
#define SO_BROADCAST	0x0020	/* Allow transmission of
				   broadcast messages.  */
#define SO_LINGER	0x0080	/* Block on close of a reliable
				   socket to transmit pending data.  */
#define SO_OOBINLINE 0x0100	/* Receive out-of-band data in-band.  */
#if 0
To add: #define SO_REUSEPORT 0x0200	/* Allow local address and port reuse.  */
#endif

#define SO_TYPE		0x1008	/* Compatible name for SO_STYLE.  */
#define SO_STYLE	SO_TYPE	/* Synonym */
#define SO_ERROR	0x1007	/* get error status and clear */
#define SO_SNDBUF	0x1001	/* Send buffer size. */
#define SO_RCVBUF	0x1002	/* Receive buffer. */
#define SO_SNDLOWAT	0x1003	/* send low-water mark */
#define SO_RCVLOWAT	0x1004	/* receive low-water mark */
#define SO_SNDTIMEO	0x1005	/* send timeout */
#define SO_RCVTIMEO 	0x1006	/* receive timeout */
#define SO_ACCEPTCONN	0x1009

/* linux-specific, might as well be the same as on i386 */
#define SO_NO_CHECK	11
#define SO_PRIORITY	12
#define SO_BSDCOMPAT	14

#define SO_PASSCRED	17
#define SO_PEERCRED	18

/* Security levels - as per NRL IPv6 - don't actually do anything */
#define SO_SECURITY_AUTHENTICATION		22
#define SO_SECURITY_ENCRYPTION_TRANSPORT	23
#define SO_SECURITY_ENCRYPTION_NETWORK		24

#define SO_BINDTODEVICE		25

/* Socket filtering */
#define SO_ATTACH_FILTER        26
#define SO_DETACH_FILTER        27

#define SO_PEERNAME             28
#define SO_TIMESTAMP		29
#define SCM_TIMESTAMP		SO_TIMESTAMP

/* Nast libc5 fixup - bletch */
#if defined(__KERNEL__)
/* Socket types. */
#define SOCK_DGRAM	1		/* datagram (conn.less) socket	*/
#define SOCK_STREAM	2		/* stream (connection) socket	*/
#define SOCK_RAW	3		/* raw socket			*/
#define SOCK_RDM	4		/* reliably-delivered message	*/
#define SOCK_SEQPACKET	5		/* sequential packet socket	*/
#define SOCK_PACKET	10		/* linux specific way of	*/
					/* getting packets at the dev	*/
					/* level.  For writing rarp and	*/
					/* other similar things on the	*/
					/* user level.			*/
#define	SOCK_MAX	(SOCK_PACKET+1)
#endif

#endif /* _ASM_SOCKET_H */
