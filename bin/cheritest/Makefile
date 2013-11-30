# $FreeBSD$

PROG=	cheritest
SRCS=	cheritest.c cheritest_sandbox.S
NO_MAN=yes

#DPADD=  ${LIBDEVSTAT} ${LIBKVM} ${LIBMEMSTAT} ${LIBUTIL}
LDADD=  -lcheri

# XXXRW: Until ELF types are right
LDFLAGS+=	-Wl,--no-warn-mismatch

NO_SHARED?=	YES

.include <bsd.prog.mk>
