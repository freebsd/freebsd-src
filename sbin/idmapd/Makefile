#	@(#)Makefile	8.2 (Berkeley) 3/27/94
#
# $FreeBSD: src/sbin/idmapd/Makefile,v 1.4 2006/10/15 13:51:49 ceri Exp $

PROG=	idmapd
MAN=	idmapd.8

CFLAGS+= -DNFS -I${.CURDIR}/../../sys
WARNS?=	2

.include <bsd.prog.mk>
