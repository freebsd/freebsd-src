# $FreeBSD$

PROG = unzip
WARNS ?= 6
CSTD = c99
DPADD = ${LIBARCHIVE} ${LIBZ}
LDADD = -larchive -lz

.include <bsd.prog.mk>
