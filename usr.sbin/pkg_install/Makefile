# $FreeBSD: src/usr.sbin/pkg_install/Makefile,v 1.5.2.8 2004/12/13 13:50:01 marius Exp $

SUBDIR=	lib add create delete info update version

.if !defined(NOCRYPT) && !defined(NO_OPENSSL)
DISTRIBUTION=	crypto
SUBDIR+= sign
.endif

.include <bsd.subdir.mk>
