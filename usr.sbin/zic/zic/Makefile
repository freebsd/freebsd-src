# $FreeBSD: src/usr.sbin/zic/zic/Makefile,v 1.10 2001/07/20 06:20:27 obrien Exp $

.PATH: ${.CURDIR}/..

PROG=	zic
MAN=	${.CURDIR}/../zic.8
SRCS=	zic.c ialloc.c scheck.c

CFLAGS+= -I${.CURDIR}/.. -I${.CURDIR}/../../../lib/libc/stdtime
CFLAGS+= -DTM_GMTOFF=tm_gmtoff -DTM_ZONE=tm_zone -DSTD_INSPIRED -DPCTS
CFLAGS+= -DHAVE_LONG_DOUBLE -DTZDIR=\"/usr/share/zoneinfo\" -Demkdir=mkdir
CFLAGS+= -DHAVE_STRERROR -DHAVE_UNISTD_H

.include <bsd.prog.mk>
