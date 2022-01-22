# $FreeBSD$

PACKAGE=	clibs
SRCDIR=		${SRCTOP}/contrib/libcxxrt

SHLIB_MAJOR=	1
SHLIBDIR?=	/lib

.PATH: ${SRCDIR}

LIB=		cxxrt

SRCS+=		auxhelper.cc
SRCS+=		dynamic_cast.cc
SRCS+=		exception.cc
SRCS+=		guard.cc
SRCS+=		libelftc_dem_gnu3.c
SRCS+=		memory.cc
SRCS+=		stdexcept.cc
SRCS+=		terminate.cc
SRCS+=		typeinfo.cc

WARNS?=		0
CFLAGS+=	-isystem ${SRCDIR} -nostdinc++
CXXSTD?=	c++14
VERSION_MAP=	${.CURDIR}/Version.map

.include <bsd.lib.mk>
