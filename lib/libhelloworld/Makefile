# $FreeBSD$

LIB=	helloworld
SRCS=	helloworld.c
INCS=	helloworld.h

LIBADD=	cheri

INCSDIR=	${INCLUDEDIR}/cheri

NEED_CHERI?=hybrid

# XXX: don't include when (eventually) building for libcheri
SUBDIR=	compartment

.include <bsd.lib.mk>
