# $FreeBSD$

PROG=	cheritest
SRCS=	cheritest.c cheritest_sandbox.S
NO_MAN=yes
USE_CHERI=	yes

#DPADD=  ${LIBDEVSTAT} ${LIBKVM} ${LIBMEMSTAT} ${LIBUTIL}
LDADD=  -lcheri

NO_SHARED?=	YES

.include <bsd.prog.mk>
