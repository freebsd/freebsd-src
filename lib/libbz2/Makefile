# $FreeBSD$

MAINTAINER=	obrien@FreeBSD.org

BZ2DIR=	${.CURDIR}/../../contrib/bzip2
.PATH: ${BZ2DIR}

LIB=		bz2
SHLIB_MAJOR=	1
SHLIB_MINOR=	0
SRCS=		bzlib.c blocksort.c compress.c crctable.c decompress.c \
		huffman.c randtable.c
INCS=		bzlib.h
CFLAGS+=	-I${BZ2DIR}

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${BZ2DIR}/bzlib.h \
	    ${DESTDIR}/usr/include

.include <bsd.lib.mk>
