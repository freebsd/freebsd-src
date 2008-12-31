# $FreeBSD: src/sbin/ddb/Makefile,v 1.1.2.2.2.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	ddb
SRCS=	ddb.c ddb_capture.c ddb_script.c
MAN=	ddb.8
WARNS=	3

DPADD=	${LIBKVM}
LDADD=	-lkvm

.include <bsd.prog.mk>
