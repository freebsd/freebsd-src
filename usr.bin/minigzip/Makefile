# $FreeBSD: src/usr.bin/minigzip/Makefile,v 1.4.30.2.2.1 2010/06/14 02:09:06 kensmith Exp $

SRCDIR=	${.CURDIR}/../../lib/libz
.PATH:	${SRCDIR}

PROG=	minigzip

CFLAGS+=-DUSE_MMAP
DPADD=	${LIBZ}
LDADD=	-lz

.include <bsd.prog.mk>
