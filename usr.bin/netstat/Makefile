# $FreeBSD: src/usr.bin/netstat/Makefile,v 1.14.2.1 2000/07/15 07:29:29 kris Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/12/93

PROG=	netstat
SRCS=	if.c inet.c inet6.c main.c mbuf.c mroute.c ipx.c route.c \
	unix.c atalk.c netgraph.c mroute6.c ipsec.c # iso.c ns.c tp_astring.c 
CFLAGS+=-Wall
CFLAGS+=-DIPSEC

#CFLAGS+=-g
#.PATH:	${.CURDIR}/../../sys/netiso
BINGRP=	kmem
BINMODE=2555
DPADD=	${LIBKVM} ${LIBIPX} ${LIBNETGRAPH}
LDADD=	-lkvm -lipx -lnetgraph
CFLAGS+=-DINET6 -DIPSEC

.include <bsd.prog.mk>
