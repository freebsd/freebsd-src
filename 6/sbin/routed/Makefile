# Make `routed` for FreeBSD
# $FreeBSD$

PROG=   routed
SRCS=   if.c input.c main.c output.c parms.c radix.c rdisc.c table.c trace.c
MAN=   routed.8
SUBDIR= rtquery
LDADD=	-lmd
DPADD=	${LIBMD}
WARNS?=	0

.include <bsd.prog.mk>
