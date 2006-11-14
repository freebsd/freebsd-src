# $FreeBSD$

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
