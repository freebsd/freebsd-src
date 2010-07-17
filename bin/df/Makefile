#	@(#)Makefile	8.3 (Berkeley) 5/8/95
# $FreeBSD: src/bin/df/Makefile,v 1.18.30.1.4.1 2010/06/14 02:09:06 kensmith Exp $

MOUNT=	${.CURDIR}/../../sbin/mount
.PATH: ${MOUNT}

PROG=	df
SRCS=	df.c vfslist.c

CFLAGS+= -I${MOUNT}

DPADD=	${LIBUTIL}
LDADD=	-lutil

.include <bsd.prog.mk>
