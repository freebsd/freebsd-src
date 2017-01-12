# $FreeBSD$

LIB=	helloworld
SRCS=	helloworld.c
INCS=	helloworld.h

LIBADD=	cheri
# libcheri is not built shared and is not PIC so creating a shared library
# linking against libcheri will not work correctly
NO_SHARED=	yes

INCSDIR=	${INCLUDEDIR}/cheri

NEED_CHERI?=hybrid

.ifndef LIBCHERI
SUBDIR=	compartment
.endif

.include <bsd.lib.mk>
