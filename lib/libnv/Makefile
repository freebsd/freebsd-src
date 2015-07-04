# $FreeBSD$

SHLIBDIR?= /lib

.include <src.opts.mk>

LIB=	nv
SHLIB_MAJOR= 0

.PATH: ${.CURDIR}/../../sys/kern ${.CURDIR}/../../sys/sys
CFLAGS+=-I${.CURDIR}/../../sys -I${.CURDIR}

SRCS=	subr_dnvlist.c
SRCS+=	msgio.c
SRCS+=	subr_nvlist.c
SRCS+=	subr_nvpair.c

WARNS?=	6

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.lib.mk>
