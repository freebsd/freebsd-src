# $FreeBSD: src/gnu/lib/libssp/libssp_nonshared/Makefile,v 1.1 2007/05/19 04:25:56 kan Exp $

GCCDIR=	${.CURDIR}/../../../../contrib/gcc
GCCLIB=	${.CURDIR}/../../../../contrib/gcclibs

.PATH: ${GCCLIB}/libssp ${GCCLIB}/libssp/ssp

LIB=	ssp_nonshared
NO_PIC=
NO_PROFILE=

SRCS=	ssp-local.c

CFLAGS+= -DHAVE_CONFIG_H
CFLAGS+= -I${.CURDIR}/..  -I${GCCLIB}/libssp  -I${GCCLIB}/include
CFLAGS+= -fPIC -DPIC

.include <bsd.lib.mk>
