#       $NetBSD: Makefile,v 1.1 1999/11/23 05:28:20 mrg Exp $
# $FreeBSD: src/sbin/rcorder/Makefile,v 1.5.8.1 2006/01/26 01:30:12 dougb Exp $

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
