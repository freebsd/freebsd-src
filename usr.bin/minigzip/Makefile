# $FreeBSD: src/usr.bin/minigzip/Makefile,v 1.4.30.2.6.1 2012/03/03 06:15:13 kensmith Exp $

SRCDIR=	${.CURDIR}/../../lib/libz
.PATH:	${SRCDIR}

PROG=	minigzip

CFLAGS+=-DUSE_MMAP
DPADD=	${LIBZ}
LDADD=	-lz

.include <bsd.prog.mk>
