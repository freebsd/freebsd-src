# $FreeBSD: src/usr.sbin/pkg_install/Makefile,v 1.16 2004/12/21 10:16:03 ru Exp $

SUBDIR=	lib add create delete info ${_sign} version

.if !defined(NO_CRYPT) && !defined(NO_OPENSSL)
_sign=	sign
.endif

.include <bsd.subdir.mk>
