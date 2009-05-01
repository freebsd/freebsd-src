#	@(#)Makefile	8.2 (Berkeley) 3/27/94
#
# $FreeBSD: src/sbin/idmapd/Makefile,v 1.4.8.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	idmapd
MAN=	idmapd.8

CFLAGS+= -DNFS -I${.CURDIR}/../../sys
WARNS?=	2

.include <bsd.prog.mk>
