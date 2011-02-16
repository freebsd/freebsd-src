# $FreeBSD: src/usr.bin/minigzip/Makefile,v 1.4.30.2.4.1 2010/12/21 17:09:25 kensmith Exp $

SRCDIR=	${.CURDIR}/../../lib/libz
.PATH:	${SRCDIR}

PROG=	minigzip

CFLAGS+=-DUSE_MMAP
DPADD=	${LIBZ}
LDADD=	-lz

.include <bsd.prog.mk>
