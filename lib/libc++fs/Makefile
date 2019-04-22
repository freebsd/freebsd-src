# $FreeBSD$

.include <src.opts.mk>

PACKAGE=	clibs
SRCDIR=		${SRCTOP}/contrib/libc++

LIB=		c++fs
NO_PIC=
MK_PROFILE=	no

.PATH:		${SRCDIR}/src/filesystem

SRCS+=		directory_iterator.cpp
SRCS+=		int128_builtins.cpp
SRCS+=		operations.cpp

WARNS?=		0
CXXFLAGS+=	${PICFLAG}
CXXFLAGS+=	-isystem ${SRCDIR}/include
CXXFLAGS+=	-nostdinc++
CXXFLAGS+=	-nostdlib
CXXFLAGS+=	-D_LIBCPP_BUILDING_LIBRARY
CXXFLAGS+=	-DLIBCXXRT
CXXSTD?=	c++14

.include <bsd.lib.mk>
