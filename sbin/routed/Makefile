#	From: @(#)Makefile	8.1 (Berkeley) 6/19/93
# $FreeBSD: src/sbin/routed/Makefile,v 1.5.2.1 1999/08/29 15:15:24 peter Exp $

PROG=	routed
SRCS=	if.c input.c main.c output.c parms.c radix.c rdisc.c table.c \
	trace.c
MAN8=	routed.8
SUBDIR=	rtquery
LDADD+=	-lmd
DPADD+=	${LIBMD}
#COPTS=	-g -DDEBUG -Wall

.include <bsd.prog.mk>
