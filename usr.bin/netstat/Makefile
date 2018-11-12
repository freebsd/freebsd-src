#	@(#)Makefile	8.1 (Berkeley) 6/12/93
# $FreeBSD$

.include <src.opts.mk>

PROG=	netstat
SRCS=	if.c inet.c main.c mbuf.c mroute.c netisr.c route.c \
	unix.c mroute6.c ipsec.c bpf.c pfkey.c sctp.c \
	flowtable.c

WARNS?=	3
CFLAGS+=-fno-strict-aliasing

CFLAGS+=-DIPSEC
CFLAGS+=-DSCTP

.if ${MK_INET_SUPPORT} != "no"
CFLAGS+=-DINET
.endif

.if ${MK_INET6_SUPPORT} != "no"
SRCS+=	inet6.c
CFLAGS+=-DINET6
.endif

.if ${MK_OFED} != "no"
CFLAGS+=-DSDP
.endif

.if ${MK_PF} != "no"
CFLAGS+=-DPF
.endif

BINGRP=	kmem
BINMODE=2555
LIBADD=	kvm memstat xo util

.if ${MK_NETGRAPH_SUPPORT} != "no"
SRCS+=	netgraph.c
LIBADD+=	netgraph
CFLAGS+=-DNETGRAPH
.endif

.include <bsd.prog.mk>
