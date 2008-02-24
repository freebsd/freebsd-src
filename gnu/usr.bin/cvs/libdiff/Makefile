# $FreeBSD: src/gnu/usr.bin/cvs/libdiff/Makefile,v 1.10 2005/06/03 15:08:30 imp Exp $

.include "${.CURDIR}/../Makefile.inc"

.PATH:	${CVSDIR}/diff

LIB=		diff
INTERNALLIB=

# gnu must be before lib to pick correct regex.h
CFLAGS+= -I../lib -I${DESTDIR}/usr/include/gnu \
	 -I${CVSDIR}/lib -DHAVE_CONFIG_H

SRCS =	diff.c diff3.c analyze.c cmpbuf.c cmpbuf.h io.c context.c ed.c \
        normal.c ifdef.c util.c dir.c version.c diff.h side.c

.include <bsd.lib.mk>
