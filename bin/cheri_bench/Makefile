# $FreeBSD$

.include <bsd.own.mk>

PROG=	cheri_bench
SRCS=	cheri_bench.c

MAN=
WANT_CHERI=	yes
WANT_DUMP=	yes

NO_SHARED?=	YES

NO_WERROR=	YES

LIBADD+= pthread

.if ${MK_CHERI} != "no"
LIBADD+=  cheri
.ifdef CLANG_CLEARREGS
CFLAGS+= -mllvm -cheri-use-clearregs
.endif
.endif

.include <bsd.prog.mk>
