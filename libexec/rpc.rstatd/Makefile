# $FreeBSD: src/libexec/rpc.rstatd/Makefile,v 1.8.18.1.4.1 2009/04/15 03:14:26 kensmith Exp $

PROG =	rpc.rstatd
SRCS =	rstatd.c rstat_proc.c
MAN =	rpc.rstatd.8

DPADD=	${LIBRPCSVC} ${LIBUTIL} ${LIBDEVSTAT}
LDADD=	-lrpcsvc -lutil -ldevstat

.include <bsd.prog.mk>
