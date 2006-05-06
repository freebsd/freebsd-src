# $FreeBSD: src/sbin/geom/class/Makefile,v 1.5.2.3 2005/09/14 21:21:45 marius Exp $

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
