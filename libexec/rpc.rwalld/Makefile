# $FreeBSD: src/libexec/rpc.rwalld/Makefile,v 1.6 2001/03/26 14:22:07 ru Exp $

PROG =	rpc.rwalld
SRCS =	rwalld.c
MAN =	rpc.rwalld.8

DPADD=	${LIBRPCSVC} ${LIBUTIL}
LDADD=	-lrpcsvc -lutil

.include <bsd.prog.mk>
