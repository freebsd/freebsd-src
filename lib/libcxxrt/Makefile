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

.if exists(Version.map.${MACHINE})
VERSION_MAP=	${.CURDIR}/Version.map.${MACHINE}
.else
.if ${MACHINE_ABI:Mlong32}
VERSION_MAP=	Version-32.map
.else
VERSION_MAP=	Version-64.map
.endif

# On powerpc64le long double is IEEE binary128, which clang mangles as
# u9__ieee128 ("__ieee128" demangled) instead of the generic "long double" (e).
# Rewrite the typeinfo entries so the version script matches the emitted symbols.
LDBL_SED=
.if ${MACHINE_ARCH} == "powerpc64le"
LDBL_SED=	-e 's/long double/__ieee128/g'
.endif

Version-32.map: Version.map
	sed -e 's/%%NEW_DELETE_TYPE%%/int/' ${LDBL_SED} ${.ALLSRC} > ${.TARGET}

Version-64.map: Version.map
	sed -e 's/%%NEW_DELETE_TYPE%%/long/' ${LDBL_SED} ${.ALLSRC} > ${.TARGET}
.endif

.include <bsd.lib.mk>

# gcc13 doesn't support _Float16 on riscv, powerpc64, and x86 or __int128
# on i386.  Disable checks for missing symbols to allow this to link.
#
# XXX: This is a hack that hides an ABI divergence between clang and gcc.
.if ${COMPILER_TYPE} == "gcc" && \
    (${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "i386" || \
     ${MACHINE_ARCH:Mpowerpc64*} || ${MACHINE_ARCH} == "riscv64")
LDFLAGS+=	-Wl,--undefined-version
.endif
