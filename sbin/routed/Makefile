# Make `routed` for FreeBSD
# $FreeBSD: src/sbin/routed/Makefile,v 1.16.26.1 2008/11/25 02:59:29 kensmith Exp $

PROG=   routed
SRCS=   if.c input.c main.c output.c parms.c radix.c rdisc.c table.c trace.c
MAN=   routed.8
SUBDIR= rtquery
LDADD=	-lmd
DPADD=	${LIBMD}
WARNS?=	0

.include <bsd.prog.mk>
