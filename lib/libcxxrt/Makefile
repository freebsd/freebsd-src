# $FreeBSD$

SRCDIR=       ${.CURDIR}/../../contrib/libcxxrt

SHLIB_MAJOR=  1
SHLIBDIR?=    /lib

.PATH: ${SRCDIR}

LIB=          cxxrt

SRCS+=        libelftc_dem_gnu3.c\
              terminate.cc\
              dynamic_cast.cc\
              memory.cc\
              auxhelper.cc\
              exception.cc\
              stdexcept.cc\
              typeinfo.cc\
              guard.cc

WARNS=        0
CFLAGS+=      -I${SRCDIR}
LDADD+=       -pthread

.include <bsd.lib.mk>
