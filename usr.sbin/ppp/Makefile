#	$Id: Makefile,v 1.32 1997/11/22 03:37:21 brian Exp $

PROG=	ppp
SRCS=	arp.c async.c auth.c ccp.c chap.c chat.c command.c deflate.c \
	defs.c filter.c fsm.c hdlc.c id.c ip.c ipcp.c lcp.c \
	log.c lqr.c main.c mbuf.c modem.c os.c pap.c phase.c \
	pred.c route.c server.c sig.c slcompress.c systems.c throughput.c \
	timer.c tun.c vars.c vjcomp.c
CFLAGS+=-Wall
LDADD+= -lmd -lcrypt -lutil -lz
DPADD+= ${LIBMD} ${LIBCRYPT} ${LIBUTIL} ${LIBZ}
MAN8=	ppp.8
BINMODE=4550
BINOWN=	root
BINGRP=	network

.if defined(NOALIAS)
CFLAGS+=-DNOALIAS
.else
SRCS+=	alias_cmd.c loadalias.c
.endif

.if exists(${.CURDIR}/../../secure) && !defined(NOCRYPT) && !defined(NOSECURE)
CFLAGS+=-DHAVE_DES
SRCS+=	chap_ms.c
LDADD+= -ldes
DPADD+= ${LIBDES}
.endif

.include <bsd.prog.mk>
