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

WARNS?=		3

CLEANFILES+=	example.o example foo.gz minigzip.o minigzip

SRCS = adler32.c compress.c crc32.c gzio.c uncompr.c deflate.c trees.c \
       zutil.c inflate.c inftrees.c inffast.c zopen.c infback.c
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
