# Make `routed` for FreeBSD
# $FreeBSD: src/sbin/routed/Makefile,v 1.18.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

PROG=   routed
SRCS=   if.c input.c main.c output.c parms.c radix.c rdisc.c table.c trace.c
MAN=   routed.8
SUBDIR= rtquery
LDADD=	-lmd
DPADD=	${LIBMD}
.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "amd64" 
WARNS?=	6
.else
WARNS?=	0
.endif

.include <bsd.prog.mk>
