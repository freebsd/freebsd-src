#	@(#)Makefile	8.3 (Berkeley) 5/8/95
# $FreeBSD$

PROG=	df
SRCS=	df.c vfslist.c

MOUNT=	${.CURDIR}/../../sbin/mount
CFLAGS+= -I${MOUNT}
.PATH:	${MOUNT}

DPADD=	${LIBM}
LDADD=	-lm

.include <bsd.prog.mk>
