#       $NetBSD: Makefile,v 1.1 1999/11/23 05:28:20 mrg Exp $

PROG=   rcorder
SRCS=   ealloc.c hash.c rcorder.c
MAN=	rcorder.8

LDADD+=	-lutil
DPADD+=	${LIBUTIL}

# XXX hack for make's hash.[ch]
CPPFLAGS+= -DORDER

.include <bsd.prog.mk>
