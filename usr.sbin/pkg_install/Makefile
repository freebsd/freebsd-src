# $FreeBSD: src/usr.sbin/pkg_install/Makefile,v 1.15 2004/01/17 13:41:16 ru Exp $

SUBDIR=	lib add create delete info ${_sign} version

.if !defined(NOCRYPT) && !defined(NO_OPENSSL)
_sign=	sign
.endif

.include <bsd.subdir.mk>
