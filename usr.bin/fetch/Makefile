# $FreeBSD: src/usr.bin/fetch/Makefile,v 1.18 2006/03/17 18:54:38 ru Exp $

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
