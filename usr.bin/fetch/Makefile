# $FreeBSD: src/usr.bin/fetch/Makefile,v 1.15 2003/07/22 13:54:31 ru Exp $

PROG=		fetch
CSTD?=		c99
WARNS?=		6
DPADD=		${LIBFETCH}
LDADD=		-lfetch
.if !defined(NOCRYPT) && !defined(NO_OPENSSL)
DISTRIBUTION=	crypto
DPADD+=		${LIBSSL} ${LIBCRYPTO}
LDADD+=		-lssl -lcrypto
.endif

.include <bsd.prog.mk>
