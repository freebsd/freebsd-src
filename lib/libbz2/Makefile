# $FreeBSD: src/lib/libbz2/Makefile,v 1.5.10.1 2005/07/22 17:29:03 kensmith Exp $

BZ2DIR=	${.CURDIR}/../../contrib/bzip2
.PATH: ${BZ2DIR}

LIB=		bz2
SHLIB_MAJOR=	2
SRCS=		bzlib.c blocksort.c compress.c crctable.c decompress.c \
		huffman.c randtable.c
INCS=		bzlib.h
CFLAGS+=	-I${BZ2DIR}

.include <bsd.lib.mk>
