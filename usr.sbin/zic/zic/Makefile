# $FreeBSD: src/usr.sbin/zic/zic/Makefile,v 1.4.2.1 1999/08/29 15:50:09 peter Exp $

.PATH:	${.CURDIR}/..

PROG=	zic

SRCS=	zic.c ialloc.c scheck.c
MAN8=	${.CURDIR}/../zic.8
CFLAGS+= -I${.CURDIR}/.. -I${.CURDIR}/../../../lib/libc/stdtime
CFLAGS+= -DTM_GMTOFF=tm_gmtoff -DTM_ZONE=tm_zone -DSTD_INSPIRED -DPCTS
CFLAGS+= -DHAVE_LONG_DOUBLE -DTZDIR=\"/usr/share/zoneinfo\" -Demkdir=mkdir
CFLAGS+= -DHAVE_STRERROR -DHAVE_UNISTD_H

.include <bsd.prog.mk>
