# $FreeBSD$

PROG =	rpc.rstatd
SRCS =	rstatd.c rstat_proc.c

DPADD=	${LIBRPCSVC} ${LIBUTIL} ${LIBKVM} ${LIBDEVSTAT}
LDADD=	-lrpcsvc -lutil -lkvm -ldevstat

.include <bsd.prog.mk>
