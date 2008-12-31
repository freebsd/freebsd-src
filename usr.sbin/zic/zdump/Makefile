# $FreeBSD: src/usr.sbin/zic/zdump/Makefile,v 1.11.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/..

PROG=	zdump
MAN=	zdump.8
SRCS=	zdump.c ialloc.c scheck.c

CFLAGS+= -DTM_GMTOFF=tm_gmtoff -DTM_ZONE=tm_zone -DSTD_INSPIRED -DPCTS
CFLAGS+= -DHAVE_LONG_DOUBLE -DTZDIR=\"/usr/share/zoneinfo\" -Demkdir=mkdir
CFLAGS+= -I${.CURDIR}/.. -I${.CURDIR}/../../../lib/libc/stdtime

.include <bsd.prog.mk>
