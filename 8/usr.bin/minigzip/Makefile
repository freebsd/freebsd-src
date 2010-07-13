# $FreeBSD$

SRCDIR=	${.CURDIR}/../../lib/libz
.PATH:	${SRCDIR}

PROG=	minigzip

CFLAGS+=-DUSE_MMAP
DPADD=	${LIBZ}
LDADD=	-lz

.include <bsd.prog.mk>
