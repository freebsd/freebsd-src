# Make `routed` for FreeBSD
# $FreeBSD$

PROG=   routed
SRCS=   if.c input.c main.c output.c parms.c radix.c rdisc.c table.c trace.c
MAN=   routed.8
SUBDIR= rtquery
LDADD=	-lmd
DPADD=	${LIBMD}
# XXX this is verboten
.if ${MACHINE_CPUARCH} != "i386" && ${MACHINE_CPUARCH} != "amd64" 
WARNS?=	0
.endif

.include <bsd.prog.mk>
