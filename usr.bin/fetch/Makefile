# $FreeBSD: src/usr.bin/fetch/Makefile,v 1.18.6.1 2008/11/25 02:59:29 kensmith Exp $

.include <bsd.own.mk>

PROG=		fetch
CSTD?=		c99
WARNS?=		6
DPADD=		${LIBFETCH}
LDADD=		-lfetch
.if ${MK_OPENSSL} != "no"
DPADD+=		${LIBSSL} ${LIBCRYPTO}
LDADD+=		-lssl -lcrypto
.endif

.include <bsd.prog.mk>
