# $FreeBSD: src/usr.sbin/pkg_install/Makefile,v 1.5.2.7 2003/01/09 12:05:52 des Exp $

SUBDIR=	lib add create delete info update version

.if !defined(NOCRYPT) && !defined(NOSECURE) && !defined(NO_OPENSSL)
DISTRIBUTION=	crypto
SUBDIR+= sign
.endif

.include <bsd.subdir.mk>
