# $FreeBSD: src/sbin/ddb/Makefile,v 1.2.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	ddb
SRCS=	ddb.c ddb_capture.c ddb_script.c
MAN=	ddb.8
WARNS=	3

DPADD=	${LIBKVM}
LDADD=	-lkvm

.include <bsd.prog.mk>
