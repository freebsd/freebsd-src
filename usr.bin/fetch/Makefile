# $FreeBSD$

MAINTAINER=	des@freebsd.org
PROG=		fetch
DPADD=		${LIBFETCH}
LDADD=		-lfetch
.if !defined(NOCRYPT)
DPADD+=		${LIBCRYPTO} ${LIBSSL}
LDADD+=		-lcrypto -lssl
.endif

.include <bsd.prog.mk>
