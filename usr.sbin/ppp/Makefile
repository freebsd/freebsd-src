#	$Id: Makefile,v 1.26 1997/09/27 19:11:39 brian Exp $

PROG=	ppp
SRCS=	alias_cmd.c arp.c async.c auth.c ccp.c chap.c chat.c command.c \
	filter.c fsm.c hdlc.c ip.c ipcp.c lcp.c loadalias.c log.c lqr.c \
	main.c mbuf.c modem.c os.c pap.c pred.c route.c server.c sig.c \
	slcompress.c systems.c timer.c vars.c vjcomp.c
CFLAGS+=-Wall
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
