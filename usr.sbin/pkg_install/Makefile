# $FreeBSD: src/usr.sbin/pkg_install/Makefile,v 1.5.2.6 2002/08/31 18:39:08 obrien Exp $

SUBDIR=	lib add create delete info update version

.if exists(${.CURDIR}/../../crypto) && !defined(NOCRYPT) && !defined(NO_OPENSSL)
DISTRIBUTION=	crypto
SUBDIR+= sign
.endif

.include <bsd.subdir.mk>
