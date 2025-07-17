.include <src.opts.mk>

PACKAGE=	clibs
SRCDIR=		${SRCTOP}/contrib/llvm-project/libcxx

LIB=		c++experimental
NO_PIC=

.PATH:		${SRCDIR}/src

SRCS+=		experimental/keep.cpp

WARNS?=		0
CXXFLAGS+=	${PICFLAG}
CFLAGS+=	-isystem ${.CURDIR}/../libc++
CXXFLAGS+=	-isystem ${SRCDIR}/include
CXXFLAGS+=	-nostdinc++
CXXFLAGS+=	-nostdlib
CXXFLAGS+=	-D_LIBCPP_BUILDING_LIBRARY
CXXFLAGS+=	-DLIBCXXRT
CXXSTD?=	c++20

.include <bsd.lib.mk>
