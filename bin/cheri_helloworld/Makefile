# $FreeBSD$

PROG=	cheri_helloworld
SRCS=	cheri_helloworld.c

MAN=
USE_CHERI=	yes
WANT_DUMP=	yes

DPADD=	${LIBCHERI}
LDADD=  -lcheri

NO_SHARED?=	YES

NO_WERROR=	YES

.include <src.opts.mk>
.include <bsd.prog.mk>
