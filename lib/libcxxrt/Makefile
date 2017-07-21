# $FreeBSD$

.include <src.opts.mk>

PACKAGE=	clibs
.if ${MK_CHERI} != "no"
WANT_CHERI?=	hybrid
ALLOW_SHARED_TEXTREL=yes
SRCDIR=		${SRCTOP}/contrib/cheri-libcxxrt/src
.else
SRCDIR=		${SRCTOP}/contrib/cheri-libcxxrt/src
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
