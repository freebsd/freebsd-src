# $FreeBSD$

SRCDIR=	${.CURDIR}/../../lib/libz
.PATH:	${SRCDIR}

PROG=	minigzip

WARNS?=	5
CFLAGS+=-DUSE_MMAP
DPADD=	${LIBZ}
LDADD=	-lz

.include <bsd.prog.mk>
