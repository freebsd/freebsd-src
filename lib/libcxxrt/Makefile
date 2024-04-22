PACKAGE=	clibs
SHLIBDIR?=	/lib

.include <src.opts.mk>

SRCDIR=		${SRCTOP}/contrib/libcxxrt

SHLIB_MAJOR=	1

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

.if exists(Version.map.${MACHINE})
VERSION_MAP=	${.CURDIR}/Version.map.${MACHINE}
.else
.if ${MACHINE_ABI:Mlong32}
VERSION_MAP=	Version-32.map
.else
VERSION_MAP=	Version-64.map
.endif

Version-32.map: Version.map
	sed 's/%%NEW_DELETE_TYPE%%/int/' ${.ALLSRC} > ${.TARGET}

Version-64.map: Version.map
	sed 's/%%NEW_DELETE_TYPE%%/long/' ${.ALLSRC} > ${.TARGET}
.endif

.include <bsd.lib.mk>
