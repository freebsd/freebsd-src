# $FreeBSD: src/libexec/rpc.rstatd/Makefile,v 1.8.18.1.6.1 2010/02/10 00:26:20 kensmith Exp $

PROG =	rpc.rstatd
SRCS =	rstatd.c rstat_proc.c
MAN =	rpc.rstatd.8

DPADD=	${LIBRPCSVC} ${LIBUTIL} ${LIBDEVSTAT}
LDADD=	-lrpcsvc -lutil -ldevstat

.include <bsd.prog.mk>
