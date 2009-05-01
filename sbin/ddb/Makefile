# $FreeBSD: src/sbin/ddb/Makefile,v 1.1.2.2.4.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	ddb
SRCS=	ddb.c ddb_capture.c ddb_script.c
MAN=	ddb.8
WARNS=	3

DPADD=	${LIBKVM}
LDADD=	-lkvm

.include <bsd.prog.mk>
