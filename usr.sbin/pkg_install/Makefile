# $FreeBSD: src/usr.sbin/pkg_install/Makefile,v 1.11 2002/05/08 18:44:18 paul Exp $

SUBDIR=	lib add create delete info version

.if exists(${.CURDIR}/../../crypto) && !defined(NOCRYPT) && !defined(NO_OPENSSL)
DISTRIBUTION=	crypto
SUBDIR+= sign
.endif

.include <bsd.subdir.mk>
