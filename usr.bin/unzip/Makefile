# $FreeBSD: src/usr.bin/unzip/Makefile,v 1.1.2.2.4.1 2010/12/21 17:09:25 kensmith Exp $

PROG = unzip
WARNS ?= 6
CSTD = c99
DPADD = ${LIBARCHIVE} ${LIBZ}
LDADD = -larchive -lz

.include <bsd.prog.mk>
