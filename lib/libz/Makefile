#
# $Id: Makefile,v 1.8 1998/04/17 07:26:51 phk Exp $
#

MAINTAINER=peter@FreeBSD.org

LIB=		z
MAN3=           zlib.3

#CFLAGS+=	-DMAX_WBITS=14 -DMAX_MEM_LEVEL=7
#CFLAGS+=	-g -DDEBUG
#CFLAGS+=	-Wall -Wwrite-strings -Wpointer-arith -Wconversion \
#		-Wstrict-prototypes -Wmissing-prototypes

CLEANFILES+=	example.o example foo.gz minigzip.o minigzip

SRCS = adler32.c compress.c crc32.c gzio.c uncompr.c deflate.c trees.c \
       zutil.c inflate.c infblock.c inftrees.c infcodes.c infutil.c inffast.c

beforeinstall:
.for hdr in zconf.h zlib.h
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/${hdr} \
		${DESTDIR}/usr/include
.endfor

minigzip:	all minigzip.o
	$(CC) -o minigzip minigzip.o -L. -lz

example:	all example.o
	$(CC) -o example example.o -L. -lz

test: example minigzip
	(export LD_LIBRARY_PATH=. ; ./example )
	(export LD_LIBRARY_PATH=. ; \
		echo hello world | ./minigzip | ./minigzip -d )

.include <bsd.lib.mk>
