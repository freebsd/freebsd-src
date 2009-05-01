#	@(#)Makefile	8.3 (Berkeley) 5/8/95
# $FreeBSD: src/bin/df/Makefile,v 1.18.28.1 2009/04/15 03:14:26 kensmith Exp $

MOUNT=	${.CURDIR}/../../sbin/mount
.PATH: ${MOUNT}

PROG=	df
SRCS=	df.c vfslist.c

CFLAGS+= -I${MOUNT}

DPADD=	${LIBUTIL}
LDADD=	-lutil

.include <bsd.prog.mk>
