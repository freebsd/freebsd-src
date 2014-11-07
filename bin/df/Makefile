#	@(#)Makefile	8.3 (Berkeley) 5/8/95
# $FreeBSD$

MOUNT=	${.CURDIR}/../../sbin/mount
.PATH: ${MOUNT}

PROG=	df
SRCS=	df.c vfslist.c

CFLAGS+= -I${MOUNT}

DPADD=	${LIBUTIL} ${LIBXO}
LDADD=	-lutil -lxo

.include <bsd.prog.mk>
