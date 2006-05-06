# $FreeBSD: src/usr.bin/fetch/Makefile,v 1.17 2004/12/21 10:16:02 ru Exp $

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
