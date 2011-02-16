# $FreeBSD: src/gnu/usr.bin/cc/libdecnumber/Makefile,v 1.1.10.1.6.1 2010/12/21 17:09:25 kensmith Exp $

# Use our headers in preference to ones from ../cc_tools.
CFLAGS+=	-I${.CURDIR} -I.

.include "../Makefile.inc"

.PATH: ${GCCLIB}/libdecnumber

LIB=	decnumber
SRCS=	decNumber.c decContext.c decUtility.c \
        decimal32.c decimal64.c decimal128.c

INTERNALLIB=
WARNS?=	1

.include <bsd.lib.mk>
