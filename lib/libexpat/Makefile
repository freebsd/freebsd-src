# $FreeBSD$

EXPAT=	${.CURDIR}/../../contrib/expat

# Please use the same SHLIB_MAJOR as for the corresponding expat
# version in ports, as long as this is applicable.

SHLIB_MAJOR=	4

LIB=	expat

.PATH:	${EXPAT}/lib

CFLAGS += -I${EXPAT}/lib -I${EXPAT} -I${.CURDIR}

SRCS=	xmlparse.c xmlrole.c xmltok.c

INCS=	expat.h

.include <bsd.lib.mk>
