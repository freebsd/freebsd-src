#	@(#)Makefile	8.3 (Berkeley) 5/8/95
# $FreeBSD: src/bin/df/Makefile,v 1.16 2003/06/13 07:04:01 markm Exp $

MOUNT=	${.CURDIR}/../../sbin/mount
.PATH:	${MOUNT}

PROG=	df
SRCS=	df.c vfslist.c

CFLAGS+= -I${MOUNT}

DPADD=	${LIBM}
LDADD=	-lm

.include <bsd.prog.mk>
