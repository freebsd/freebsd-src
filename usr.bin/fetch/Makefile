# $FreeBSD$

MAINTAINER=	des@freebsd.org
PROG=		fetch
DPADD=		${LIBFETCH}
LDADD=		-lfetch
.if !defined(NOCRYPT) && !defined(NOSECURE) && !defined(NO_OPENSSL)
DPADD+=		${LIBCRYPTO} ${LIBSSL}
LDADD+=		-lcrypto -lssl
.endif

.include <bsd.prog.mk>
