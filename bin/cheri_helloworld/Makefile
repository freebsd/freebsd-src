# $FreeBSD$

PROG=	cheri_helloworld
SRCS=	cheri_helloworld.c

MAN=
USE_CHERI=	yes

DPADD=	${LIBCHERI}
LDADD=  -lcheri

NO_SHARED?=	YES

NO_WERROR=	YES

FILES=	cheri_helloworld.dump
CLEANFILES=	cheri_helloworld.dump

cheri_helloworld.dump: cheri_helloworld
	objdump -xsSD ${.ALLSRC} > ${.TARGET}

.include <src.opts.mk>
.include <bsd.prog.mk>
