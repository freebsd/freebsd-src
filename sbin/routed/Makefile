# Make `routed` for FreeBSD
# $FreeBSD: src/sbin/routed/Makefile,v 1.16.24.1 2008/10/02 02:57:24 kensmith Exp $

PROG=   routed
SRCS=   if.c input.c main.c output.c parms.c radix.c rdisc.c table.c trace.c
MAN=   routed.8
SUBDIR= rtquery
LDADD=	-lmd
DPADD=	${LIBMD}
WARNS?=	0

.include <bsd.prog.mk>
