# $FreeBSD$

SUBDIR=	lib add create delete info ${_sign} version

.if !defined(NOCRYPT) && !defined(NO_OPENSSL)
_sign=	sign
.endif

.include <bsd.subdir.mk>
