# $FreeBSD$

.include <bsd.own.mk>

SUBDIR=	lib add create delete info ${_sign} updating version

.if ${MK_OPENSSL} != "no"
_sign=	sign
.endif

.include <bsd.subdir.mk>
