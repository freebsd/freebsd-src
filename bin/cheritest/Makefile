# $FreeBSD$

PROG=	cheritest
SRCS=	cheritest.c cheritest_fault.c cheritest_sandbox.S
NO_MAN=yes
USE_CHERI=	yes

#DPADD=  ${LIBDEVSTAT} ${LIBKVM} ${LIBMEMSTAT} ${LIBUTIL}
LDADD=  -lcheri

NO_SHARED?=	YES

NO_WERROR=	YES

FILES=	cheritest.dump
CLEANFILES=	cheritest.dump

cheritest.dump: cheritest
	objdump -xsSD ${.ALLSRC} > ${.TARGET}

.include <bsd.prog.mk>
