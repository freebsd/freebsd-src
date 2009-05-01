# $FreeBSD: src/lib/libbz2/Makefile,v 1.7.8.1 2009/04/15 03:14:26 kensmith Exp $

BZ2DIR=	${.CURDIR}/../../contrib/bzip2
.PATH: ${BZ2DIR}

LIB=		bz2
SHLIB_MAJOR=	3
SRCS=		bzlib.c blocksort.c compress.c crctable.c decompress.c \
		huffman.c randtable.c
INCS=		bzlib.h
CFLAGS+=	-I${BZ2DIR}

.include <bsd.lib.mk>
