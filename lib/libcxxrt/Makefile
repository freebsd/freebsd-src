# $FreeBSD$

PACKAGE=	clibs
.if defined(LIBCHERI)
# XXXAR: TODO: do this for MIPS too
SRCDIR=		${SRCTOP}/contrib/cheri-libcxxrt
.else
SRCDIR=		${SRCTOP}/contrib/libcxxrt
.endif
SHLIB_MAJOR=	1
SHLIBDIR?=	/lib

.PATH: ${SRCDIR}

LIB=		cxxrt

SRCS+=		libelftc_dem_gnu3.c\
		terminate.cc\
		dynamic_cast.cc\
		memory.cc\
		auxhelper.cc\
		exception.cc\
		stdexcept.cc\
		typeinfo.cc\
		guard.cc

WARNS=		0
CFLAGS+=	-isystem ${SRCDIR} -nostdinc++
.if empty(CXXFLAGS:M-std=*)
CXXFLAGS+=	-std=c++11
.endif
VERSION_MAP=	${.CURDIR}/Version.map

.include <bsd.lib.mk>
