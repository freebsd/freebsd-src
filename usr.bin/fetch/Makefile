# $FreeBSD: src/usr.bin/fetch/Makefile,v 1.3.6.4 2003/01/09 11:54:58 des Exp $

MAINTAINER=	des@freebsd.org
PROG=		fetch
WARNS?=		2
DPADD=		${LIBFETCH}
LDADD=		-lfetch
.if !defined(NOCRYPT) && !defined(NOSECURE) && !defined(NO_OPENSSL)
DPADD+=		${LIBCRYPTO} ${LIBSSL}
LDADD+=		-lcrypto -lssl
.endif

.include <bsd.prog.mk>
