# $FreeBSD$

PACKAGE=	lib${LIB}
LIB=		ifconfig
PRIVATELIB=	true
# Don't build shared library, for now.
NO_PIC= 

SHLIBDIR?=	/lib
SHLIB_MAJOR=	1
SRCS=		libifconfig.c libifconfig_internal.c

INCSDIR=	${INCLUDEDIR}
INCS=		libifconfig.h

#MAN=		libifconfig.3

CFLAGS+= -I${.CURDIR}
WARNS?=6

.include <bsd.lib.mk>
