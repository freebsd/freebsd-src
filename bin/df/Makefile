#	@(#)Makefile	8.3 (Berkeley) 5/8/95
# $FreeBSD: src/bin/df/Makefile,v 1.15 2002/05/12 19:24:09 phk Exp $

PROG=	df
SRCS=	df.c vfslist.c

MOUNT=	${.CURDIR}/../../sbin/mount
CFLAGS+= -I${MOUNT}
.PATH:	${MOUNT}

DPADD=	${LIBM}
LDADD=	-lm

WARNS?=	5

.include <bsd.prog.mk>
