# $FreeBSD$

SUBDIR=	lib add create delete info ${_sign} version

.if !defined(NO_CRYPT) && !defined(NO_OPENSSL)
_sign=	sign
.endif

.include <bsd.subdir.mk>
