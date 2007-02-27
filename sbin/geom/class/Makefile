# $FreeBSD$

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
SUBDIR+=raid3
SUBDIR+=shsec
SUBDIR+=stripe

.include <bsd.subdir.mk>
