# $FreeBSD$

PROG=	ddb
SRCS=	ddb.c ddb_capture.c ddb_script.c
MAN=	ddb.8
WARNS?=	3

DPADD=	${LIBKVM}
LDADD=	-lkvm

.include <bsd.prog.mk>
