# $FreeBSD$

PROG =	rpc.rstatd
SRCS =	rstatd.c rstat_proc.c
MAN =	rpc.rstatd.8

DPADD=	${LIBRPCSVC} ${LIBUTIL} ${LIBDEVSTAT} ${LIBKVM}
LDADD=	-lrpcsvc -lutil -ldevstat -lkvm

WARNS?=	1

.include <bsd.prog.mk>
