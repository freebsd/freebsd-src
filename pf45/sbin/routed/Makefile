# Make `routed` for FreeBSD
# $FreeBSD$

PROG=	routed
MAN=	routed.8
SRCS=	if.c input.c main.c output.c parms.c radix.c rdisc.c table.c trace.c
WARNS?=	3
DPADD=	${LIBMD}
LDADD=	-lmd

SUBDIR= rtquery

.include <bsd.prog.mk>
