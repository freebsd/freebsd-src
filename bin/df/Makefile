#	@(#)Makefile	8.3 (Berkeley) 5/8/95
# $FreeBSD: src/bin/df/Makefile,v 1.18.24.1 2008/10/02 02:57:24 kensmith Exp $

MOUNT=	${.CURDIR}/../../sbin/mount
.PATH: ${MOUNT}

PROG=	df
SRCS=	df.c vfslist.c

CFLAGS+= -I${MOUNT}

DPADD=	${LIBUTIL}
LDADD=	-lutil

.include <bsd.prog.mk>
