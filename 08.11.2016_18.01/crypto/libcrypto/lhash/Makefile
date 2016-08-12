# $NetBSD: Makefile,v 1.1 2009/02/13 20:58:15 jmmv Exp $

HELPER_NAME=	lhashtest
HELPER_DIR=	lhash
HELPER_SRCS=	lh_test.c

CPPFLAGS+=	-DMalloc=malloc
DPADD+=		${LIBCRYPTO} ${LIBCRYPT}

.include <bsd.init.mk>
