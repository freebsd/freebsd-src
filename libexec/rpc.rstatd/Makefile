# $FreeBSD: src/libexec/rpc.rstatd/Makefile,v 1.8.8.1.2.1 2008/10/02 02:57:24 kensmith Exp $

PROG =	rpc.rstatd
SRCS =	rstatd.c rstat_proc.c
MAN =	rpc.rstatd.8

DPADD=	${LIBRPCSVC} ${LIBUTIL} ${LIBDEVSTAT}
LDADD=	-lrpcsvc -lutil -ldevstat

.include <bsd.prog.mk>
