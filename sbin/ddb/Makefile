# $FreeBSD: src/sbin/ddb/Makefile,v 1.2.2.1.8.1 2012/03/03 06:15:13 kensmith Exp $

PROG=	ddb
SRCS=	ddb.c ddb_capture.c ddb_script.c
MAN=	ddb.8
WARNS=	3

DPADD=	${LIBKVM}
LDADD=	-lkvm

.include <bsd.prog.mk>
