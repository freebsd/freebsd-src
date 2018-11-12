# $FreeBSD$

.PATH: ${.CURDIR}/../../../contrib/tzcode/zic

PROG=	zic
MAN=	zic.8
SRCS=	zic.c ialloc.c scheck.c

CFLAGS+= -DTM_GMTOFF=tm_gmtoff -DTM_ZONE=tm_zone -DSTD_INSPIRED -DPCTS
CFLAGS+= -DHAVE_LONG_DOUBLE -DTZDIR=\"${SHAREDIR}/zoneinfo\" -Demkdir=mkdir
CFLAGS+= -DHAVE_STRERROR -DHAVE_UNISTD_H
CFLAGS+= -I${.CURDIR}/.. -I${.CURDIR}/../../../contrib/tzcode/stdtime

WARNS?=	2

.include <bsd.prog.mk>
