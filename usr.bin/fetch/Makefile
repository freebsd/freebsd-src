# $FreeBSD: src/usr.bin/fetch/Makefile,v 1.18.10.3.2.1 2012/03/03 06:15:13 kensmith Exp $

.include <bsd.own.mk>

PROG=		fetch
CSTD?=		c99
WARNS?=		6
DPADD=		${LIBFETCH} ${LIBMD}
LDADD=		-lfetch -lmd
.if ${MK_OPENSSL} != "no"
DPADD+=		${LIBSSL} ${LIBCRYPTO}
LDADD+=		-lssl -lcrypto
.endif

.include <bsd.prog.mk>
