# $FreeBSD: src/usr.bin/fetch/Makefile,v 1.16 2004/08/06 07:27:06 cperciva Exp $

PROG=		fetch
CSTD?=		c99
WARNS?=		6
DPADD=		${LIBFETCH}
LDADD=		-lfetch
.if !defined(NOCRYPT) && !defined(NO_OPENSSL)
DPADD+=		${LIBSSL} ${LIBCRYPTO}
LDADD+=		-lssl -lcrypto
.endif

.include <bsd.prog.mk>
