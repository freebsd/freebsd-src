# $FreeBSD: src/usr.sbin/zic/zdump/Makefile,v 1.4.2.1 2001/04/25 12:11:16 ru Exp $

.PATH:	${.CURDIR}/..

PROG=	zdump

SRCS=	zdump.c ialloc.c scheck.c
MAN=	${.CURDIR}/../zdump.8
CFLAGS+= -I${.CURDIR}/.. -I${.CURDIR}/../../../lib/libc/stdtime
CFLAGS+= -DTM_GMTOFF=tm_gmtoff -DTM_ZONE=tm_zone -DSTD_INSPIRED -DPCTS
CFLAGS+= -DHAVE_LONG_DOUBLE -DTZDIR=\"/usr/share/zoneinfo\" -Demkdir=mkdir

.include <bsd.prog.mk>
