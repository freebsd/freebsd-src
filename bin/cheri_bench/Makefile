# $FreeBSD$

PROG=	cheri_bench
SRCS=	cheri_bench.c

MAN=
WANT_CHERI=	yes
#NEED_CHERI=     hybrid
WANT_DUMP=	yes

NO_SHARED?=	YES

NO_WERROR=	YES

.include <bsd.prog.mk>
LIBADD+= pthread
LDADD+= -lpthread

.if ${MK_CHERI} != "no"
LIBADD+=  cheri
LDADD+=  -static -lcheri
.ifdef CLANG_CLEARREGS
CFLAGS+= -mllvm -cheri-use-clearregs
.endif
.endif
