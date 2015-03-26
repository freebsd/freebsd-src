# $FreeBSD$

PROG=	cheri_bench
SRCS=	cheri_bench.c

MAN=
NEED_CHERI=	hybrid
WANT_DUMP=	yes

LIBADD=  cheri
LDADD=  -static -lcheri

NO_SHARED?=	YES

NO_WERROR=	YES

.include <bsd.prog.mk>
