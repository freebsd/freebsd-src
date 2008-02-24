# $FreeBSD: src/usr.sbin/pkg_install/Makefile,v 1.17 2006/03/17 18:54:42 ru Exp $

.include <bsd.own.mk>

SUBDIR=	lib add create delete info ${_sign} version

.if ${MK_OPENSSL} != "no"
_sign=	sign
.endif

.include <bsd.subdir.mk>
