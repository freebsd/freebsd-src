# $FreeBSD$
#

NEED_CHERI=sandbox

NO_SHARED=yes

LIB=		cheri_syscalls
SRCS+=		cheri_syscalls.c

CFLAGS+=	-I${.CURDIR}/../libcheri
CFLAGS+=	-mxgot

.include <bsd.lib.mk>

CFLAGS+= ${CFLAGS.${.IMPSRC:T}}
