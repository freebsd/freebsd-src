#
# $FreeBSD$
#

LIB=		z
SHLIBDIR?=	/lib
SHLIB_MAJOR=	6
MAN=		zlib.3

#CFLAGS=-O -DMAX_WBITS=14 -DMAX_MEM_LEVEL=7
#CFLAGS=-g -DDEBUG
#CFLAGS=-O3 -Wall -Wwrite-strings -Wpointer-arith -Wconversion \
#           -Wstrict-prototypes -Wmissing-prototypes

CFLAGS+=	-DHAS_snprintf -DHAS_vsnprintf -I${.CURDIR}

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
SRCS+=		zopen.c
SRCS+=		zutil.c

.if ${MACHINE_ARCH} == "i386" && ${MACHINE_CPU:M*i686*}
.PATH:		${.CURDIR}/contrib/asm686
SRCS+=		match.S
CFLAGS+=	-DASMV -DNO_UNDERLINE
.endif

.if ${MACHINE_ARCH} == "amd64"
.PATH:		${.CURDIR}/contrib/gcc_gvmat64
SRCS+=		gvmat64.S
CFLAGS+=	-DASMV -DNO_UNDERLINE
.endif

VERSION_DEF=	${.CURDIR}/Versions.def
SYMBOL_MAPS=	${.CURDIR}/Symbol.map
CFLAGS+=	-DSYMBOL_VERSIONING

INCS=		zconf.h zlib.h

minigzip:	all minigzip.o
	$(CC) -o minigzip minigzip.o -L. -lz

example:	all example.o
	$(CC) -o example example.o -L. -lz

test: example minigzip
	(export LD_LIBRARY_PATH=. ; ./example )
	(export LD_LIBRARY_PATH=. ; \
		echo hello world | ./minigzip | ./minigzip -d )

.include <bsd.lib.mk>
