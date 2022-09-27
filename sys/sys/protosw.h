/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1982, 1986, 1993
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
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)protosw.h	8.1 (Berkeley) 6/2/93
 * $FreeBSD$
 */

#ifndef _SYS_PROTOSW_H_
#define _SYS_PROTOSW_H_

#include <sys/queue.h>

/* Forward declare these structures referenced from prototypes below. */
struct kaiocb;
struct mbuf;
struct thread;
struct sockaddr;
struct socket;
struct sockopt;

/*#ifdef _KERNEL*/
/*
 * Protocol switch table.
 *
 * Each protocol has a handle initializing one of these structures,
 * which is used for protocol-protocol and system-protocol communication.
 *
 * In retrospect, it would be a lot nicer to use an interface
 * similar to the vnode VOP interface.
 */
struct ifnet;
struct stat;
struct ucred;
struct uio;

/* USE THESE FOR YOUR PROTOTYPES ! */
typedef int	pr_ctloutput_t(struct socket *, struct sockopt *);
typedef int	pr_setsbopt_t(struct socket *, struct sockopt *);
typedef void	pr_abort_t(struct socket *);
typedef int	pr_accept_t(struct socket *, struct sockaddr **);
typedef int	pr_attach_t(struct socket *, int, struct thread *);
typedef int	pr_bind_t(struct socket *, struct sockaddr *, struct thread *);
typedef int	pr_connect_t(struct socket *, struct sockaddr *,
		    struct thread *);
typedef int	pr_connect2_t(struct socket *, struct socket *);
typedef int	pr_control_t(struct socket *, unsigned long, void *,
		    struct ifnet *, struct thread *);
typedef void	pr_detach_t(struct socket *);
typedef int	pr_disconnect_t(struct socket *);
typedef int	pr_listen_t(struct socket *, int, struct thread *);
typedef int	pr_peeraddr_t(struct socket *, struct sockaddr **);
typedef int	pr_rcvd_t(struct socket *, int);
typedef int	pr_rcvoob_t(struct socket *, struct mbuf *, int);
typedef enum {
	PRUS_OOB =		0x1,
	PRUS_EOF =		0x2,
	PRUS_MORETOCOME =	0x4,
	PRUS_NOTREADY =		0x8,
	PRUS_IPV6 =		0x10,
} pr_send_flags_t;
typedef int	pr_send_t(struct socket *, int, struct mbuf *,
		    struct sockaddr *, struct mbuf *, struct thread *);
typedef int	pr_ready_t(struct socket *, struct mbuf *, int);
typedef int	pr_sense_t(struct socket *, struct stat *);
typedef int	pr_shutdown_t(struct socket *);
typedef int	pr_flush_t(struct socket *, int);
typedef int	pr_sockaddr_t(struct socket *, struct sockaddr **);
typedef int	pr_sosend_t(struct socket *, struct sockaddr *, struct uio *,
		    struct mbuf *, struct mbuf *, int, struct thread *);
typedef int	pr_soreceive_t(struct socket *, struct sockaddr **,
		    struct uio *, struct mbuf **, struct mbuf **, int *);
typedef int	pr_sopoll_t(struct socket *, int, struct ucred *,
		    struct thread *);
typedef void	pr_sosetlabel_t(struct socket *);
typedef void	pr_close_t(struct socket *);
typedef int	pr_bindat_t(int, struct socket *, struct sockaddr *,
		    struct thread *);
typedef int	pr_connectat_t(int, struct socket *, struct sockaddr *,
		    struct thread *);
typedef int	pr_aio_queue_t(struct socket *, struct kaiocb *);

struct protosw {
	short	pr_type;		/* socket type used for */
	short	pr_protocol;		/* protocol number */
	short	pr_flags;		/* see below */
	short	pr_unused;
	struct	domain	*pr_domain;	/* domain protocol a member of */

	pr_soreceive_t	*pr_soreceive;	/* recv(2) */
	pr_rcvd_t	*pr_rcvd;	/* soreceive_generic() if PR_WANTRCVD */
	pr_sosend_t	*pr_sosend;	/* send(2) */
	pr_send_t	*pr_send;	/* send(2) via sosend_generic() */
	pr_ready_t	*pr_ready;	/* sendfile/ktls readyness */
	pr_sopoll_t	*pr_sopoll;	/* poll(2) */
/* Cache line #2 */
	pr_attach_t	*pr_attach;	/* creation: socreate(), sonewconn() */
	pr_detach_t	*pr_detach;	/* destruction: sofree() */
	pr_connect_t	*pr_connect;	/* connect(2) */
	pr_disconnect_t	*pr_disconnect;	/* sodisconnect() */
	pr_close_t	*pr_close;	/* close(2) */
	pr_shutdown_t	*pr_shutdown;	/* shutdown(2) */
	pr_abort_t	*pr_abort;	/* abrupt tear down: soabort() */
	pr_aio_queue_t	*pr_aio_queue;	/* aio(9) */
/* Cache line #3 */
	pr_bind_t	*pr_bind;	/* bind(2) */
	pr_bindat_t	*pr_bindat;	/* bindat(2) */
	pr_listen_t	*pr_listen;	/* listen(2) */
	pr_accept_t	*pr_accept;	/* accept(2) */
	pr_connectat_t	*pr_connectat;	/* connectat(2) */
	pr_connect2_t	*pr_connect2;	/* socketpair(2) */
	pr_control_t	*pr_control;	/* ioctl(2) */
	pr_rcvoob_t	*pr_rcvoob;	/* soreceive_rcvoob() */
/* Cache line #4 */
	pr_ctloutput_t	*pr_ctloutput;	/* control output (from above) */
	pr_peeraddr_t	*pr_peeraddr;	/* getpeername(2) */
	pr_sockaddr_t	*pr_sockaddr;	/* getsockname(2) */
	pr_sense_t	*pr_sense;	/* stat(2) */
	pr_flush_t	*pr_flush;	/* XXXGL: merge with pr_shutdown_t! */
	pr_sosetlabel_t	*pr_sosetlabel;	/* MAC, XXXGL: remove */
	pr_setsbopt_t	*pr_setsbopt;	/* Socket buffer ioctls */
};
/*#endif*/

/*
 * Values for pr_flags.
 * PR_ADDR requires PR_ATOMIC;
 * PR_ADDR and PR_CONNREQUIRED are mutually exclusive.
 * PR_IMPLOPCL means that the protocol allows sendto without prior connect,
 *	and the protocol understands the MSG_EOF flag.  The first property is
 *	is only relevant if PR_CONNREQUIRED is set (otherwise sendto is allowed
 *	anyhow).
 * PR_SOCKBUF requires protocol to initialize and destroy its socket buffers
 * in its pr_attach and pr_detach.
 */
#define	PR_ATOMIC	0x01		/* exchange atomic messages only */
#define	PR_ADDR		0x02		/* addresses given with messages */
#define	PR_CONNREQUIRED	0x04		/* connection required by protocol */
#define	PR_WANTRCVD	0x08		/* want PRU_RCVD calls */
#define	PR_RIGHTS	0x10		/* passes capabilities */
#define PR_IMPLOPCL	0x20		/* implied open/close */
/* was	PR_LASTHDR	0x40		   enforce ipsec policy; last header */
#define	PR_CAPATTACH	0x80		/* socket can attach in cap mode */
#define	PR_SOCKBUF	0x100		/* private implementation of buffers */

/*
 * The arguments to the ctlinput routine are
 *	(*protosw[].pr_ctlinput)(cmd, sa, arg);
 * where cmd is one of the commands below, sa is a pointer to a sockaddr,
 * and arg is a `void *' argument used within a protocol family.
 */
#define	PRC_ROUTEDEAD		1	/* select new route if possible ??? */
/* was	PRC_QUENCH2		3	DEC congestion bit says slow down */
/* was	PRC_QUENCH		4	Deprecated by RFC 6633 */
#define	PRC_MSGSIZE		5	/* message size forced drop */
#define	PRC_HOSTDEAD		6	/* host appears to be down */
#define	PRC_HOSTUNREACH		7	/* deprecated (use PRC_UNREACH_HOST) */
#define	PRC_UNREACH_NET		8	/* no route to network */
#define	PRC_UNREACH_HOST	9	/* no route to host */
#define	PRC_UNREACH_PROTOCOL	10	/* dst says bad protocol */
#define	PRC_UNREACH_PORT	11	/* bad port # */
/* was	PRC_UNREACH_NEEDFRAG	12	   (use PRC_MSGSIZE) */
#define	PRC_UNREACH_SRCFAIL	13	/* source route failed */
#define	PRC_REDIRECT_NET	14	/* net routing redirect */
#define	PRC_REDIRECT_HOST	15	/* host routing redirect */
#define	PRC_REDIRECT_TOSNET	16	/* redirect for type of service & net */
#define	PRC_REDIRECT_TOSHOST	17	/* redirect for tos & host */
#define	PRC_TIMXCEED_INTRANS	18	/* packet lifetime expired in transit */
#define	PRC_TIMXCEED_REASS	19	/* lifetime expired on reass q */
#define	PRC_PARAMPROB		20	/* header incorrect */
#define	PRC_UNREACH_ADMIN_PROHIB	21	/* packet administrativly prohibited */

#define	PRC_NCMDS		22

#define	PRC_IS_REDIRECT(cmd)	\
	((cmd) >= PRC_REDIRECT_NET && (cmd) <= PRC_REDIRECT_TOSHOST)

#ifdef PRCREQUESTS
char	*prcrequests[] = {
	"IFDOWN", "ROUTEDEAD", "IFUP", "DEC-BIT-QUENCH2",
	"QUENCH", "MSGSIZE", "HOSTDEAD", "#7",
	"NET-UNREACH", "HOST-UNREACH", "PROTO-UNREACH", "PORT-UNREACH",
	"#12", "SRCFAIL-UNREACH", "NET-REDIRECT", "HOST-REDIRECT",
	"TOSNET-REDIRECT", "TOSHOST-REDIRECT", "TX-INTRANS", "TX-REASS",
	"PARAMPROB", "ADMIN-UNREACH"
};
#endif

/*
 * The arguments to ctloutput are:
 *	(*protosw[].pr_ctloutput)(req, so, level, optname, optval, p);
 * req is one of the actions listed below, so is a (struct socket *),
 * level is an indication of which protocol layer the option is intended.
 * optname is a protocol dependent socket option request,
 * optval is a pointer to a mbuf-chain pointer, for value-return results.
 * The protocol is responsible for disposal of the mbuf chain *optval
 * if supplied,
 * the caller is responsible for any space held by *optval, when returned.
 * A non-zero return from ctloutput gives an
 * UNIX error number which should be passed to higher level software.
 */
#define	PRCO_GETOPT	0
#define	PRCO_SETOPT	1

#define	PRCO_NCMDS	2

#ifdef PRCOREQUESTS
char	*prcorequests[] = {
	"GETOPT", "SETOPT",
};
#endif

#ifdef _KERNEL
struct domain *pffinddomain(int family);
struct protosw *pffindproto(int family, int type, int proto);
int protosw_register(struct domain *, struct protosw *);
int protosw_unregister(struct protosw *);

/* Domains that are known to be avaliable for protosw_register(). */
extern struct domain inetdomain;
extern struct domain inet6domain;
#endif

#endif
