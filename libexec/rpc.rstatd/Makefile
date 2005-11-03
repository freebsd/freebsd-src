# $FreeBSD: src/libexec/rpc.rstatd/Makefile,v 1.8 2004/02/04 10:20:43 ru Exp $

PROG =	rpc.rstatd
SRCS =	rstatd.c rstat_proc.c
MAN =	rpc.rstatd.8

DPADD=	${LIBRPCSVC} ${LIBUTIL} ${LIBDEVSTAT} ${LIBKVM}
LDADD=	-lrpcsvc -lutil -ldevstat -lkvm

.include <bsd.prog.mk>
