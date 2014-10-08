# $FreeBSD$

PROG=	cheri_bench
SRCS=	cheri_bench.c

MAN=
USE_CHERI=	yes

DPADD=	${LIBCHERI}
LDADD=  -lcheri

NO_SHARED?=	YES

NO_WERROR=	YES

FILES=	cheri_bench.dump
CLEANFILES=	cheri_bench.dump

cheri_bench.dump: cheri_bench
	objdump -xsSD ${.ALLSRC} > ${.TARGET}

.include <src.opts.mk>
.include <bsd.prog.mk>
