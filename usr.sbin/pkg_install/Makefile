# $FreeBSD: src/usr.sbin/pkg_install/Makefile,v 1.5.2.2 2001/03/05 04:33:54 wes Exp $

SUBDIR=lib add create delete info update version

.if !defined(NO_OPENSSL)
SUBDIR+=sign
.endif

.include <bsd.subdir.mk>
