# $FreeBSD: src/gnu/lib/libssp/libssp_nonshared/Makefile,v 1.4.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

GCCDIR=	${.CURDIR}/../../../../contrib/gcc
GCCLIB=	${.CURDIR}/../../../../contrib/gcclibs

.PATH: ${GCCLIB}/libssp ${GCCLIB}/libssp/ssp

LIB=	ssp_nonshared
NO_PIC=
NO_PROFILE=

SRCS=	ssp-local.c

CFLAGS+= -DHAVE_CONFIG_H
CFLAGS+= -I${.CURDIR}/..  -I${GCCLIB}/libssp  -I${GCCLIB}/include
CFLAGS+= -fPIC -DPIC -fvisibility=hidden

.include <bsd.lib.mk>
