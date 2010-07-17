# $FreeBSD: src/usr.bin/unzip/Makefile,v 1.1.2.2.2.1 2010/06/14 02:09:06 kensmith Exp $

PROG = unzip
WARNS ?= 6
CSTD = c99
DPADD = ${LIBARCHIVE} ${LIBZ}
LDADD = -larchive -lz

.include <bsd.prog.mk>
