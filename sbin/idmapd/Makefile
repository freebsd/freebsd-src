#	@(#)Makefile	8.2 (Berkeley) 3/27/94
#
# $FreeBSD: src/sbin/idmapd/Makefile,v 1.4.10.1 2010/02/10 00:26:20 kensmith Exp $

PROG=	idmapd
MAN=	idmapd.8

CFLAGS+= -DNFS -I${.CURDIR}/../../sys
WARNS?=	2

.include <bsd.prog.mk>
