# $FreeBSD$

.include <bsd.own.mk>

SUBDIR=	concat
.if ${MK_OPENSSL} != "no"
SUBDIR+=eli
.endif
SUBDIR+=label
SUBDIR+=mirror
SUBDIR+=nop
SUBDIR+=raid3
SUBDIR+=shsec
SUBDIR+=stripe

.include <bsd.subdir.mk>
