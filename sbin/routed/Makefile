#	From: @(#)Makefile	8.1 (Berkeley) 6/19/93
#	$Id: Makefile,v 1.2 1996/09/16 17:03:27 wollman Exp $

PROG=	routed
SRCS=	if.c input.c main.c output.c parms.c radix.c rdisc.c table.c \
	trace.c
MAN8=	routed.8
SUBDIR=	rtquery
LDADD+=	-lmd
DPADD+=	${LIBMD}
#COPTS=	-g -DDEBUG -Wall

.include <bsd.prog.mk>
