# $FreeBSD$

PROG=	cheri_bench
SRCS=	cheri_bench.c

MAN=
USE_CHERI=	yes
WANT_DUMP=	yes

LIBADD=  cheri
LDADD=  -static -lcheri

NO_SHARED?=	YES

NO_WERROR=	YES

.include <bsd.prog.mk>
