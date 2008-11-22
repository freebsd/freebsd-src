# $FreeBSD$

PROG =	rpc.rstatd
SRCS =	rstatd.c rstat_proc.c
MAN =	rpc.rstatd.8

DPADD=	${LIBRPCSVC} ${LIBUTIL} ${LIBDEVSTAT}
LDADD=	-lrpcsvc -lutil -ldevstat

.include <bsd.prog.mk>
