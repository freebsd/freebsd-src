#	@(#)Makefile	8.3 (Berkeley) 5/8/95
# $FreeBSD: src/bin/df/Makefile,v 1.9.2.3 2002/04/05 00:19:21 iedowse Exp $

PROG=	df
SRCS=	df.c vfslist.c

MOUNT=	${.CURDIR}/../../sbin/mount
CFLAGS+= -I${MOUNT}
.PATH:	${MOUNT}

DPADD=	${LIBM}
LDADD=	-lm

WARNS?=	2

.include <bsd.prog.mk>
