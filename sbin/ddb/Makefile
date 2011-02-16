# $FreeBSD: src/sbin/ddb/Makefile,v 1.1.2.2.8.1 2010/12/21 17:10:29 kensmith Exp $

PROG=	ddb
SRCS=	ddb.c ddb_capture.c ddb_script.c
MAN=	ddb.8
WARNS=	3

DPADD=	${LIBKVM}
LDADD=	-lkvm

.include <bsd.prog.mk>
