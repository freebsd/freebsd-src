# $FreeBSD: src/sbin/geom/class/Makefile,v 1.16.6.1 2008/11/25 02:59:29 kensmith Exp $

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
