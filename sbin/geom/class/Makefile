# $FreeBSD: src/sbin/geom/class/Makefile,v 1.16.8.1 2009/04/15 03:14:26 kensmith Exp $

.include <bsd.own.mk>

SUBDIR=	cache
SUBDIR+=concat
.if ${MK_OPENSSL} != "no"
SUBDIR+=eli
.endif
SUBDIR+=journal
SUBDIR+=label
SUBDIR+=mirror
SUBDIR+=multipath
SUBDIR+=nop
SUBDIR+=part
SUBDIR+=raid3
SUBDIR+=shsec
SUBDIR+=stripe
SUBDIR+=virstor

.include <bsd.subdir.mk>
