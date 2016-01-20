# $FreeBSD$

LIB=	helloworld
SRCS=	helloworld.c
INCS=	helloworld.h

LIBADD=	cheri

INCSDIR=	${INCLUDEDIR}/cheri

NEED_CHERI?=hybrid

.ifndef LIBCHERI
SUBDIR=	compartment
.endif

.include <bsd.lib.mk>
