# $FreeBSD$

.include <bsd.own.mk>

PROG=		fetch
CSTD?=		c99
.if ${MK_OPENSSL} != "no"
DPADD=		${LIBFETCH} ${LIBSSL} ${LIBCRYPTO}
LDADD=		-lfetch -lssl -lcrypto
.else
DPADD=		${LIBFETCH} ${LIBMD}
LDADD=		-lfetch -lmd
.endif

.include <bsd.prog.mk>
