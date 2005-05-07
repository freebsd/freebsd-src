#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/pwd_mkdb/Makefile,v 1.10 2003/04/04 17:49:17 obrien Exp $

.PATH: ${.CURDIR}/../../lib/libc/gen		# for pw_scan.c

PROG=	pwd_mkdb
MAN=	pwd_mkdb.8
SRCS=	pw_scan.c pwd_mkdb.c

WARNS?=	4
CFLAGS+= -I${.CURDIR}/../../lib/libc/gen		# for pw_scan.h

.include <bsd.prog.mk>
