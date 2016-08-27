# $FreeBSD$

PACKAGE=	lib${LIB}
LIB=		ifc
# Don't build shared library, for now.
NO_PIC= 

SHLIBDIR?=	/lib
SHLIB_MAJOR=	1
SRCS=		libifc.c libifc_internal.c

INCSDIR=	${INCLUDEDIR}
INCS=		libifc.h

#MAN=		libifco.3

CFLAGS+= -I${.CURDIR}
WARNS?=6

.include <bsd.lib.mk>
