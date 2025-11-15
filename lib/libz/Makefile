PACKAGE=	zlib
LIB=		z
SHLIBDIR?=	/lib
SHLIB_MAJOR=	6
MAN=		zlib.3 zopen.3
MLINKS+=	zopen.3 zdopen.3

ZLIBSRC=	${SRCTOP}/sys/contrib/zlib

.PATH:		${ZLIBSRC}

#CFLAGS=-O -DMAX_WBITS=14 -DMAX_MEM_LEVEL=7
#CFLAGS=-g -DDEBUG
#CFLAGS=-O3 -Wall -Wwrite-strings -Wpointer-arith -Wconversion \
#           -Wstrict-prototypes -Wmissing-prototypes

CFLAGS+=	-D_LARGEFILE64_SOURCE=1 -DHAVE_HIDDEN -I${.CURDIR}

WARNS?=		3

CLEANFILES+=	example.o example foo.gz minigzip.o minigzip

SRCS+=		adler32.c
SRCS+=		compress.c
SRCS+=		crc32.c
SRCS+=		deflate.c
SRCS+=		gzclose.c
SRCS+=		gzlib.c
SRCS+=		gzread.c
SRCS+=		gzwrite.c
SRCS+=		infback.c
SRCS+=		inffast.c
SRCS+=		inflate.c
SRCS+=		inftrees.c
SRCS+=		trees.c
SRCS+=		uncompr.c
SRCS+=		zutil.c

# Wrapper relies on FreeBSD-specific fpos_t representation and non-portable
# funopen. Not needed for bootstrapping, so just disable it.
.if ${.MAKE.OS} == "FreeBSD" || !defined(BOOTSTRAPPING)
SRCS+=		zopen.c
.endif

.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "amd64"
CFLAGS+=	-DUNALIGNED_OK
.endif

VERSION_DEF=	${.CURDIR}/Versions.def
SYMBOL_MAPS=	${.CURDIR}/Symbol.map

INCS=		zconf.h zlib.h

.PATH:		${ZLIBSRC}/test

ZLIB_VERSION!=	sed -n '/define.*ZLIB_VERSION/{s,[^0-9.],,gp;q;}' ${ZLIBSRC}/zlib.h

zlib.pc: zlib.pc.in
	sed -e 's,@prefix@,/usr,g ; \
		s,@exec_prefix@,$${prefix},g ; \
		s,@libdir@,${LIBDIR},g ; \
		s,@sharedlibdir@,${SHLIBDIR},g ; \
		s,@includedir@,${INCLUDEDIR},g ; \
		s,@VERSION@,${ZLIB_VERSION},g ;' \
		${.ALLSRC} > ${.TARGET}

minigzip:	all minigzip.o
	$(CC) -o minigzip minigzip.o -L. -lz

example:	all example.o
	$(CC) -o example example.o -L. -lz

test: example minigzip
	(export LD_LIBRARY_PATH=. ; ./example )
	(export LD_LIBRARY_PATH=. ; \
		echo hello world | ./minigzip | ./minigzip -d )

PCFILES=	zlib.pc

.include <bsd.lib.mk>

CWARNFLAGS+=	${NO_WDEPRECATED_NON_PROTOTYPE} ${NO_WSTRICT_PROTOTYPES}
