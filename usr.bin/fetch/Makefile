# $FreeBSD: src/usr.bin/fetch/Makefile,v 1.18.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

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
