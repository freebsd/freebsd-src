#	$Id: Makefile,v 1.30 1997/11/16 22:15:02 brian Exp $

PROG=	ppp
SRCS=	alias_cmd.c arp.c async.c auth.c ccp.c chap.c chat.c \
	command.c defs.c filter.c fsm.c hdlc.c id.c ip.c ipcp.c lcp.c \
	loadalias.c log.c lqr.c main.c mbuf.c modem.c os.c pap.c phase.c \
	pred.c route.c server.c sig.c slcompress.c systems.c throughput.c \
	timer.c tun.c vars.c vjcomp.c
CFLAGS+=-Wall -Wmissing-prototypes
LDADD+= -lmd -lcrypt -lutil
DPADD+= ${LIBMD} ${LIBCRYPT} ${LIBUTIL}
MAN8=	ppp.8
BINMODE=4550
BINOWN=	root
BINGRP=	network

.if exists(${.CURDIR}/../../secure) && !defined(NOCRYPT) && !defined(NOSECURE)
CFLAGS+=-DHAVE_DES
SRCS+=	chap_ms.c
LDADD+= -ldes
DPADD+= ${LIBDES}
.endif

.include <bsd.prog.mk>
