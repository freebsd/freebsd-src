# $FreeBSD: src/sbin/ddb/Makefile,v 1.1.2.2.6.1 2010/02/10 00:26:20 kensmith Exp $

PROG=	ddb
SRCS=	ddb.c ddb_capture.c ddb_script.c
MAN=	ddb.8
WARNS=	3

DPADD=	${LIBKVM}
LDADD=	-lkvm

.include <bsd.prog.mk>
