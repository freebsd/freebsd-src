# $FreeBSD: src/libexec/rpc.rwalld/Makefile,v 1.4 1999/08/28 00:09:57 peter Exp $

PROG =	rpc.rwalld
SRCS =	rwalld.c
MAN8 =	rpc.rwalld.8

DPADD=	${LIBRPCSVC} ${LIBUTIL}
LDADD=	-lrpcsvc -lutil

.include <bsd.prog.mk>
