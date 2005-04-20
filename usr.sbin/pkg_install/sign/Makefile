# $FreeBSD$
# $OpenBSD: Makefile.bsd-wrapper,v 1.2 1999/10/07 16:30:32 espie Exp $

PROG=	pkg_sign
LINKS=	${BINDIR}/pkg_sign ${BINDIR}/pkg_check
MLINKS=	pkg_sign.1 pkg_check.1
SRCS=	main.c check.c common.c gzip.c pgp_check.c pgp_sign.c \
	sha1.c sign.c stand.c x509.c

CFLAGS+= -I${.CURDIR}/../lib

DPADD=	${LIBINSTALL} ${LIBMD} ${LIBCRYPTO}
LDADD=	${LIBINSTALL} -lmd -lcrypto

.include <bsd.prog.mk>

