# $FreeBSD$

PACKAGE=lib${LIB}
SHLIBDIR?= /lib

.include <src.opts.mk>

LIB=	nv
SHLIB_MAJOR= 0

.PATH: ${.CURDIR}/../../sys/contrib/libnv ${.CURDIR}/../../sys/sys
CFLAGS+=-I${.CURDIR}/../../sys -I${.CURDIR}

SRCS=	cnvlist.c
SRCS+=	dnvlist.c
SRCS+=	msgio.c
SRCS+=	nvlist.c
SRCS+=	nvpair.c

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.lib.mk>
