#	@(#)Makefile	8.2 (Berkeley) 3/27/94
#
# $FreeBSD: src/sbin/idmapd/Makefile,v 1.4.6.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	idmapd
MAN=	idmapd.8

CFLAGS+= -DNFS -I${.CURDIR}/../../sys
WARNS?=	2

.include <bsd.prog.mk>
