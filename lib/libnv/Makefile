# $FreeBSD$

PACKAGE=lib${LIB}
SHLIBDIR?= /lib

.include <src.opts.mk>

LIB=	nv
SHLIB_MAJOR= 0

.PATH: ${SRCTOP}/sys/contrib/libnv ${SRCTOP}/sys/sys
CFLAGS+=-I${SRCTOP}/sys -I${.CURDIR}

SRCS=	cnvlist.c
SRCS+=	dnvlist.c
SRCS+=	msgio.c
SRCS+=	nvlist.c
SRCS+=	nvpair.c

INCSDIR=	${INCLUDEDIR}/sys
INCS=	${SRCTOP}/sys/sys/cnv.h ${SRCTOP}/sys/sys/nv.h

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.lib.mk>
