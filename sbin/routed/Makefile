# Make `routed` for FreeBSD
# $FreeBSD$

PROG=   routed
SRCS=   if.c input.c main.c output.c parms.c radix.c rdisc.c table.c trace.c
MAN=   routed.8
SUBDIR= rtquery
LDADD=	-lmd
DPADD=	${LIBMD}
.if ${MACHINE_ARCH} != "i386" && ${MACHINE_ARCH} != "amd64" 
WARNS?=	0
.endif

.include <bsd.prog.mk>
