# $FreeBSD$

PROG=	cheritest
SRCS=	cheritest.c							\
	cheritest_ccall.c						\
	cheritest_fault.c						\
	cheritest_libcheri.c						\
	cheritest_registers.c						\
	cheritest_sandbox.S

MK_MAN=	no
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
