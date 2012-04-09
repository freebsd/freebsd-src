# $FreeBSD: src/usr.bin/unzip/Makefile,v 1.1.2.2.6.1 2012/03/03 06:15:13 kensmith Exp $

PROG = unzip
WARNS ?= 6
CSTD = c99
DPADD = ${LIBARCHIVE} ${LIBZ}
LDADD = -larchive -lz

.include <bsd.prog.mk>
