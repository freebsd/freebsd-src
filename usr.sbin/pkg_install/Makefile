# $FreeBSD: src/usr.sbin/pkg_install/Makefile,v 1.5.2.3 2001/04/24 08:51:33 wes Exp $

SUBDIR=lib add create delete info update version

.if exists(../../crypto) && !defined(NOCRYPT) && !defined(NO_OPENSSL)
DISTRIBUTION=crypto
SUBDIR+=sign
.endif

.include <bsd.subdir.mk>
