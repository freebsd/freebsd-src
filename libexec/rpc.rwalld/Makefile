#	$Id: Makefile,v 1.2 1993/11/10 03:46:23 smace Exp $

PROG =	rpc.rwalld
SRCS =	rwalld.c
MAN8 =	rpc.rwalld.8

DPADD=	${LIBRPCSVC} ${LIBUTIL}
LDADD=	-lrpcsvc -lutil

.include <bsd.prog.mk>
