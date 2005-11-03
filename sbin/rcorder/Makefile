#       $NetBSD: Makefile,v 1.1 1999/11/23 05:28:20 mrg Exp $
# $FreeBSD: src/sbin/rcorder/Makefile,v 1.5 2004/02/23 20:13:55 johan Exp $

PROG=   rcorder
SRCS=   ealloc.c hash.c rcorder.c
MAN=	rcorder.8

LDADD=	-lutil
DPADD=	${LIBUTIL}

WARNS?=	0
# XXX hack for make's hash.[ch]
CFLAGS+= -DORDER -I.

SRCS+=	util.h
CLEANFILES+=	util.h

util.h:
	ln -sf ${.CURDIR}/../../lib/libutil/libutil.h ${.TARGET}

.include <bsd.prog.mk>
