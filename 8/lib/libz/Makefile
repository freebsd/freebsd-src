#
# $FreeBSD$
#

LIB=		z
SHLIBDIR?=	/lib
MAN=		zlib.3

#CFLAGS+=	-DMAX_WBITS=14 -DMAX_MEM_LEVEL=7
#CFLAGS+=	-g -DDEBUG
#CFLAGS+=	-Wall -Wwrite-strings -Wpointer-arith -Wconversion \
#		-Wstrict-prototypes -Wmissing-prototypes

CFLAGS+=	-DHAS_snprintf -DHAS_vsnprintf

CLEANFILES+=	example.o example foo.gz minigzip.o minigzip

SRCS = adler32.c compress.c crc32.c gzio.c uncompr.c deflate.c trees.c \
       zutil.c inflate.c inftrees.c inffast.c zopen.c infback.c
INCS=		zconf.h zlib.h

.if ${MACHINE_ARCH} == "i386" && ${MACHINE_CPU:M*i686*}
.PATH:		${.CURDIR}/contrib/asm686
SRCS+=		match.S
CFLAGS+=	-DASMV -DNO_UNDERLINE
.endif

#.if ${MACHINE_ARCH} == "amd64"
#.PATH:		${.CURDIR}/contrib/gcc_gvmat64
#SRCS+=		gvmat64.S
#CFLAGS+=	-DASMV -DNO_UNDERLINE
#ACFLAGS+=	-Wa,--noexecstack
#.endif

minigzip:	all minigzip.o
	$(CC) -o minigzip minigzip.o -L. -lz

example:	all example.o
	$(CC) -o example example.o -L. -lz

test: example minigzip
	(export LD_LIBRARY_PATH=. ; ./example )
	(export LD_LIBRARY_PATH=. ; \
		echo hello world | ./minigzip | ./minigzip -d )

.include <bsd.lib.mk>
