# $FreeBSD: src/libexec/rpc.rwalld/Makefile,v 1.4.2.1 2001/04/25 10:40:57 ru Exp $

PROG =	rpc.rwalld
SRCS =	rwalld.c
MAN =	rpc.rwalld.8

DPADD=	${LIBRPCSVC} ${LIBUTIL}
LDADD=	-lrpcsvc -lutil

.include <bsd.prog.mk>
