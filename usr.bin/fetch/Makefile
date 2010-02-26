# $FreeBSD$

.include <bsd.own.mk>

PROG=		fetch
CSTD?=		c99
DPADD=		${LIBFETCH} ${LIBMD}
LDADD=		-lfetch -lmd
.if ${MK_OPENSSL} != "no"
DPADD+=		${LIBSSL} ${LIBCRYPTO}
LDADD+=		-lssl -lcrypto
.endif

.include <bsd.prog.mk>
