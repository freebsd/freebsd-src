# $FreeBSD$

PROG =	rpc.rusersd
SRCS =	rusersd.c rusers_proc.c extern.h
MAN =	rpc.rusersd.8

DPADD=	${LIBRPCSVC} ${LIBUTIL}
LDADD=	-lrpcsvc -lutil

.include <bsd.prog.mk>
