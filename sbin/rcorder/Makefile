#       $NetBSD: Makefile,v 1.1 1999/11/23 05:28:20 mrg Exp $
# $FreeBSD: src/sbin/rcorder/Makefile,v 1.6.10.1.6.1 2010/12/21 17:09:25 kensmith Exp $

PROG=   rcorder
SRCS=   ealloc.c hash.c rcorder.c
MAN=	rcorder.8

LDADD=	-lutil
DPADD=	${LIBUTIL}

WARNS?=	6
# XXX hack for make's hash.[ch]
CFLAGS+= -DORDER -I.

SRCS+=	util.h
CLEANFILES+=	util.h

util.h:
	ln -sf ${.CURDIR}/../../lib/libutil/libutil.h ${.TARGET}

.include <bsd.prog.mk>
