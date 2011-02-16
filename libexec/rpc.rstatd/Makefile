# $FreeBSD: src/libexec/rpc.rstatd/Makefile,v 1.8.18.1.8.1 2010/12/21 17:10:29 kensmith Exp $

PROG =	rpc.rstatd
SRCS =	rstatd.c rstat_proc.c
MAN =	rpc.rstatd.8

DPADD=	${LIBRPCSVC} ${LIBUTIL} ${LIBDEVSTAT}
LDADD=	-lrpcsvc -lutil -ldevstat

.include <bsd.prog.mk>
