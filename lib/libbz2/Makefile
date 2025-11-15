PACKAGE=	bzip2
LIB_PACKAGE=

BZ2DIR=	${SRCTOP}/contrib/bzip2
.PATH: ${BZ2DIR}

LIB=		bz2
SHLIB_MAJOR=	4
SRCS=		bzlib.c blocksort.c compress.c crctable.c decompress.c \
		huffman.c randtable.c
INCS=		bzlib.h
CFLAGS+=	-I${BZ2DIR}

WARNS?=		3

BZIP2_VERSION!=	sed -n '/bzip2\/libbzip2 version /{s/.*version //;s/ of.*//p;q;}' ${BZ2DIR}/bzlib.h

bzip2.pc: bzip2.pc.in
	sed -e 's,@prefix@,/usr,g ; \
		s,@exec_prefix@,$${prefix},g ; \
		s,@libdir@,${LIBDIR},g ; \
		s,@sharedlibdir@,${SHLIBDIR},g ; \
		s,@includedir@,${INCLUDEDIR},g ; \
		s,@VERSION@,${BZIP2_VERSION},g ;' \
		${.ALLSRC} > ${.TARGET}

PCFILES=	bzip2.pc

.include <bsd.lib.mk>
