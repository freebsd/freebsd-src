# $FreeBSD: src/sbin/geom/class/Makefile,v 1.5.2.3.10.1 2008/10/02 02:57:24 kensmith Exp $

SUBDIR=	concat
.if !defined(NO_CRYPT) && !defined(NO_OPENSSL)
SUBDIR+=eli
.endif
SUBDIR+=label
SUBDIR+=mirror
SUBDIR+=nop
SUBDIR+=raid3
SUBDIR+=shsec
SUBDIR+=stripe

.include <bsd.subdir.mk>
