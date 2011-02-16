# $FreeBSD: src/sbin/geom/class/Makefile,v 1.16.14.1 2010/12/21 17:10:29 kensmith Exp $

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
