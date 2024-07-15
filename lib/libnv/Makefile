SHLIBDIR?= /lib

.include <src.opts.mk>

PACKAGE=	runtime
LIB=	nv
SHLIB_MAJOR= 1

VERSION_DEF=	${SRCTOP}/lib/libc/Versions.def
VERSION_MAP=	${.CURDIR}/Version.map

.PATH: ${SRCTOP}/sys/contrib/libnv ${SRCTOP}/sys/sys
CFLAGS+=-I${.CURDIR}

SRCS=	cnvlist.c
SRCS+=	dnvlist.c
SRCS+=	msgio.c
SRCS+=	nvlist.c
SRCS+=	bsd_nvpair.c

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.lib.mk>
