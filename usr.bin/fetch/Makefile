# $FreeBSD: src/usr.bin/fetch/Makefile,v 1.17.12.1 2008/10/02 02:57:24 kensmith Exp $

PROG=		fetch
CSTD?=		c99
WARNS?=		6
DPADD=		${LIBFETCH}
LDADD=		-lfetch
.if !defined(NO_CRYPT) && !defined(NO_OPENSSL)
DPADD+=		${LIBSSL} ${LIBCRYPTO}
LDADD+=		-lssl -lcrypto
.endif

.include <bsd.prog.mk>
