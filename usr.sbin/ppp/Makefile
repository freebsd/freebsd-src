#	$Id: Makefile,v 1.9.2.9 1997/08/25 00:34:18 brian Exp $

PROG=	ppp
SRCS=	alias_cmd.c arp.c async.c auth.c ccp.c chap.c chat.c command.c \
	filter.c fsm.c hdlc.c ip.c ipcp.c lcp.c loadalias.c log.c lqr.c \
	main.c mbuf.c modem.c os.c pap.c passwdauth.c pred.c route.c \
	server.c sig.c slcompress.c systems.c timer.c vars.c vjcomp.c
CFLAGS+=-Wall
LDADD+= -lmd -lcrypt -lutil
DPADD+= ${LIBMD} ${LIBCRYPT} ${LIBUTIL}
MAN8=	ppp.8
BINMODE=4550
BINOWN=	root
BINGRP=	ppp

.include <bsd.prog.mk>
