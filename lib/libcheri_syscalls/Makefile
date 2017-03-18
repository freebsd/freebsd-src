# $FreeBSD$
#

NEED_CHERI=sandbox

NO_SHARED=yes

LIB=		cheri_syscalls
SRCS+=		cheri_syscalls.c

CFLAGS+=	-I${.CURDIR}/../libcheri
CFLAGS+=	-mxgot
# Work around CTSRD-CHERI/clang#124
CFLAGS+=	-Wno-incompatible-pointer-types-discards-qualifiers

.include <bsd.lib.mk>

CFLAGS+= ${CFLAGS.${.IMPSRC:T}}
