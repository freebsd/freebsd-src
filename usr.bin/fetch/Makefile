# $FreeBSD: src/usr.bin/fetch/Makefile,v 1.3.6.5 2004/12/13 13:50:01 marius Exp $

MAINTAINER=	des@freebsd.org
PROG=		fetch
WARNS?=		2
DPADD=		${LIBFETCH}
LDADD=		-lfetch
.if !defined(NOCRYPT) && !defined(NO_OPENSSL)
DPADD+=		${LIBCRYPTO} ${LIBSSL}
LDADD+=		-lcrypto -lssl
.endif

.include <bsd.prog.mk>
