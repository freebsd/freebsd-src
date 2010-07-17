# $FreeBSD: src/gnu/usr.bin/cc/libdecnumber/Makefile,v 1.1.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

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
