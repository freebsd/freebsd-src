# $FreeBSD: src/libexec/rpc.rwalld/Makefile,v 1.3.2.1 1999/08/29 15:03:52 peter Exp $

PROG =	rpc.rwalld
SRCS =	rwalld.c
MAN8 =	rpc.rwalld.8

DPADD=	${LIBRPCSVC} ${LIBUTIL}
LDADD=	-lrpcsvc -lutil

.include <bsd.prog.mk>
