#
# $Id: Makefile,v 1.1 1996/08/18 17:59:48 peter Exp $
#

MAINTAINER=peter@FreeBSD.org

LIB=		z

#CFLAGS+=	-DMAX_WBITS=14 -DMAX_MEM_LEVEL=7
#CFLAGS+=	-g -DDEBUG
#CFLAGS+=	-Wall -Wwrite-strings -Wpointer-arith -Wconversion \
#		-Wstrict-prototypes -Wmissing-prototypes

CLEANFILES+=	example.o example minigzip.o minigzip

SRCS = adler32.c compress.c crc32.c gzio.c uncompr.c deflate.c trees.c \
       zutil.c inflate.c infblock.c inftrees.c infcodes.c infutil.c inffast.c

beforeinstall:
	${INSTALL} -c -m 644 -o ${BINOWN} -g ${BINGRP} ${.CURDIR}/zlib.h \
		${.CURDIR}/zconf.h ${DESTDIR}/usr/include

minigzip:	all minigzip.o
	$(CC) -o minigzip minigzip.o -L. -lz

example:	all example.o
	$(CC) -o example example.o -L. -lz

test: example minigzip
	(export LD_LIBRARY_PATH=. ; ./example )
	(export LD_LIBRARY_PATH=. ; \
		echo hello world | ./minigzip | ./minigzip -d )


.include <bsd.lib.mk>
