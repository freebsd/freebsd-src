# Make `routed` for FreeBSD
# $FreeBSD: src/sbin/routed/Makefile,v 1.16 2004/08/13 14:18:24 ru Exp $

PROG=   routed
SRCS=   if.c input.c main.c output.c parms.c radix.c rdisc.c table.c trace.c
MAN=   routed.8
SUBDIR= rtquery
LDADD=	-lmd
DPADD=	${LIBMD}
WARNS?=	0

.include <bsd.prog.mk>
