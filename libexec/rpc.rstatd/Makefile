# $FreeBSD: src/libexec/rpc.rstatd/Makefile,v 1.4.2.1 1999/08/29 15:03:47 peter Exp $

PROG =	rpc.rstatd
SRCS =	rstatd.c rstat_proc.c
MAN8 =	rpc.rstatd.8

DPADD=	${LIBRPCSVC} ${LIBUTIL} ${LIBKVM} ${LIBDEVSTAT}
LDADD=	-lrpcsvc -lutil -lkvm -ldevstat

.include <bsd.prog.mk>
