# $FreeBSD$

SUBDIR=lib add create delete info update version

.if exists(../../crypto) && !defined(NOCRYPT) && !defined(NO_OPENSSL)
DISTRIBUTION=crypto
SUBDIR+=sign
.endif

.include <bsd.subdir.mk>
