# $FreeBSD: src/usr.sbin/pkg_install/Makefile,v 1.14 2003/05/19 15:52:00 des Exp $

SUBDIR=	lib add create delete info version

.if !defined(NOCRYPT) && !defined(NO_OPENSSL)
DISTRIBUTION=	crypto
SUBDIR+= sign
.endif

.include <bsd.subdir.mk>
