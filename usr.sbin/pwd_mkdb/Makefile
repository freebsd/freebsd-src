#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/pwd_mkdb/Makefile,v 1.9 2002/07/11 21:16:52 alfred Exp $

.PATH: ${.CURDIR}/../../lib/libc/gen		# for pw_scan.c

PROG=	pwd_mkdb
MAN=	pwd_mkdb.8
SRCS=	pw_scan.c pwd_mkdb.c

CFLAGS+= -I${.CURDIR}/../../lib/libc/gen		# for pw_scan.h

WARNS?=	4

.include <bsd.prog.mk>
