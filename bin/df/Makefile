#	@(#)Makefile	8.3 (Berkeley) 5/8/95
# $FreeBSD$

PACKAGE=runtime
MOUNT=	${.CURDIR}/../../sbin/mount
.PATH: ${MOUNT}

PROG=	df
SRCS=	df.c vfslist.c

CFLAGS+= -I${MOUNT}

LIBADD=	xo util

.include <bsd.prog.mk>
