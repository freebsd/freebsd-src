# $FreeBSD: src/usr.sbin/zic/zic/Makefile,v 1.12 2006/10/18 12:35:24 ru Exp $

.PATH: ${.CURDIR}/..

PROG=	zic
MAN=	zic.8
SRCS=	zic.c ialloc.c scheck.c

CFLAGS+= -DTM_GMTOFF=tm_gmtoff -DTM_ZONE=tm_zone -DSTD_INSPIRED -DPCTS
CFLAGS+= -DHAVE_LONG_DOUBLE -DTZDIR=\"/usr/share/zoneinfo\" -Demkdir=mkdir
CFLAGS+= -DHAVE_STRERROR -DHAVE_UNISTD_H
CFLAGS+= -I${.CURDIR}/.. -I${.CURDIR}/../../../lib/libc/stdtime

.include <bsd.prog.mk>
