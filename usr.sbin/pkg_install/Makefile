# $FreeBSD: src/usr.sbin/pkg_install/Makefile,v 1.5.2.5 2001/10/03 06:48:04 sobomax Exp $

SUBDIR=lib add create delete info update version

.if exists(${.CURDIR}/../../crypto) && !defined(NOCRYPT) && !defined(NO_OPENSSL)
DISTRIBUTION=crypto
SUBDIR+=sign
.endif

.include <bsd.subdir.mk>
