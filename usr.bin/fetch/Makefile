# $FreeBSD: src/usr.bin/fetch/Makefile,v 1.12 2002/10/30 04:53:58 des Exp $

PROG=		fetch
WARNS?=		5
DPADD=		${LIBFETCH}
LDADD=		-lfetch
.if !defined(NOCRYPT) && !defined(NOSECURE) && !defined(NO_OPENSSL)
DPADD+=		${LIBSSL} ${LIBCRYPTO}
LDADD+=		-lssl -lcrypto
.endif

.include <bsd.prog.mk>
