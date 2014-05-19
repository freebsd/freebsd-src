# $FreeBSD$

PROG=	cheritest
SRCS=	cheritest.c							\
	cheritest_ccall.c						\
	cheritest_fault.c						\
	cheritest_libcheri.c						\
	cheritest_registers.c						\
	cheritest_sandbox.S						\
	cheritest_util.c

MAN=
USE_CHERI=	yes

#DPADD=  ${LIBDEVSTAT} ${LIBKVM} ${LIBMEMSTAT} ${LIBUTIL}
LDADD=  -lcheri

NO_SHARED?=	YES

NO_WERROR=	YES

FILES=	cheritest.dump
CLEANFILES=	cheritest.dump

cheritest.dump: cheritest
	objdump -xsSD ${.ALLSRC} > ${.TARGET}

.include <src.opts.mk>
.include <bsd.prog.mk>
