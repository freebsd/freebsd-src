# $FreeBSD$

PROG =	rpc.rwalld
SRCS =	rwalld.c
MAN =	rpc.rwalld.8

DPADD=	${LIBRPCSVC} ${LIBUTIL}
LDADD=	-lrpcsvc -lutil

.include <bsd.prog.mk>
