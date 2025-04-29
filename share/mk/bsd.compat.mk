
.if !targets(__<${_this:T}>__)
__<${_this:T}>__:	.NOTMAIN

.include <bsd.compat.pre.mk>

.if defined(_LIBCOMPATS)
COMPAT_ARCH?=	${TARGET_ARCH}
.for _LIBCOMPAT in ${_ALL_LIBCOMPATS}
LIB${_LIBCOMPAT}CPUTYPE?=	${CPUTYPE_${_LIBCOMPAT}}
.endfor
.if (defined(WANT_COMPILER_TYPE) && ${WANT_COMPILER_TYPE} == gcc) || \
    (defined(X_COMPILER_TYPE) && ${X_COMPILER_TYPE} == gcc)
COMPAT_COMPILER_TYPE=	gcc
.else
COMPAT_COMPILER_TYPE=	clang
.endif
.else
COMPAT_ARCH=	${MACHINE_ARCH}
.for _LIBCOMPAT in ${_ALL_LIBCOMPATS}
LIB${_LIBCOMPAT}CPUTYPE=	${CPUTYPE}
.endfor
.endif

# -------------------------------------------------------------------
# 32 bit world
.if ${COMPAT_ARCH} == "amd64"
HAS_COMPAT+=	32
.if empty(LIB32CPUTYPE)
LIB32CPUFLAGS=	-march=i686 -mmmx -msse -msse2
.else
LIB32CPUFLAGS=	-march=${LIB32CPUTYPE}
.endif
LIB32CPUFLAGS.clang+=	-target x86_64-unknown-freebsd${OS_REVISION}
LIB32CPUFLAGS+=	-m32
LIB32_MACHINE=	i386
LIB32_MACHINE_ARCH=	i386
LIB32WMAKEENV=	MACHINE_CPU="i686 mmx sse sse2"
LIB32WMAKEFLAGS=	\
		LD="${XLD} -m elf_i386_fbsd"

.elif ${COMPAT_ARCH} == "powerpc64"
HAS_COMPAT+=	32
.if empty(LIB32CPUTYPE)
LIB32CPUFLAGS=	-mcpu=powerpc
.else
LIB32CPUFLAGS=	-mcpu=${LIB32CPUTYPE}
.endif

LIB32CPUFLAGS.gcc+=	-m32
LIB32CPUFLAGS.clang+=	-target powerpc-unknown-freebsd${OS_REVISION}

LIB32_MACHINE=	powerpc
LIB32_MACHINE_ARCH=	powerpc
LIB32WMAKEFLAGS=	\
		LD="${XLD} -m elf32ppc_fbsd"

.elif ${COMPAT_ARCH} == "aarch64"
HAS_COMPAT+=	32
.if empty(LIB32CPUTYPE)
LIB32CPUFLAGS=	-march=armv7
.else
LIB32CPUFLAGS=	-mcpu=${LIB32CPUTYPE}
.endif

LIB32CPUFLAGS+=	-m32
LIB32CPUFLAGS.clang+=	-target armv7-unknown-freebsd${OS_REVISION}-gnueabihf

LIB32_MACHINE=	arm
LIB32_MACHINE_ARCH=	armv7
LIB32WMAKEFLAGS=	\
		LD="${XLD} -m armelf_fbsd"
.endif

LIB32WMAKEFLAGS+= NM="${XNM}"
LIB32WMAKEFLAGS+= OBJCOPY="${XOBJCOPY}"

LIB32DTRACE=	${DTRACE} -32
LIB32_MACHINE_ABI=	${MACHINE_ABI:N*64} long32 ptr32
.if ${COMPAT_ARCH} == "amd64"
LIB32_MACHINE_ABI+=	time32
.else
LIB32_MACHINE_ABI+=	time64
.endif

# -------------------------------------------------------------------
# In the program linking case, select LIBCOMPAT
.if defined(NEED_COMPAT)
.ifndef HAS_COMPAT
.warning NEED_COMPAT defined, but no LIBCOMPAT is available (COMPAT_ARCH == ${COMPAT_ARCH})
.elif !${HAS_COMPAT:M${NEED_COMPAT}} && ${NEED_COMPAT} != "any"
.error NEED_COMPAT (${NEED_COMPAT}) defined, but not in HAS_COMPAT (${HAS_COMPAT})
.elif ${NEED_COMPAT} == "any"
.endif
.ifdef WANT_COMPAT
.error Both WANT_COMPAT and NEED_COMPAT defined
.endif
WANT_COMPAT:=	${NEED_COMPAT}
.endif

.if defined(HAS_COMPAT) && defined(WANT_COMPAT)
.if ${WANT_COMPAT} == "any"
USE_COMPAT:=	${HAS_COMPAT:[1]}
.else
USE_COMPAT:=	${WANT_COMPAT}
.endif

_LIBCOMPATS=	${USE_COMPAT}
.endif

libcompats=	${_LIBCOMPATS:tl}

# Update MACHINE and MACHINE_ARCH so they can be used in bsd.opts.mk via
# bsd.compiler.mk
.if defined(USE_COMPAT)
_LIBCOMPAT_MAKEVARS=	_MACHINE _MACHINE_ARCH
.for _var in ${_LIBCOMPAT_MAKEVARS}
.if !empty(LIB${USE_COMPAT}${_var})
LIBCOMPAT${_var}?=	${LIB${USE_COMPAT}${_var}}
.endif
.endfor

MACHINE=	${LIBCOMPAT_MACHINE}
MACHINE_ARCH=	${LIBCOMPAT_MACHINE_ARCH}
.endif

.if !defined(COMPAT_COMPILER_TYPE)
.include <bsd.compiler.mk>
COMPAT_COMPILER_TYPE=${COMPILER_TYPE}
.endif

# -------------------------------------------------------------------
# Generic code for each type.
# Set defaults based on type.
.for _LIBCOMPAT _libcompat in ${_LIBCOMPATS:@v@${v} ${v:tl}@}
WORLDTMP?=		${SYSROOT}

# Shared flags
LIB${_LIBCOMPAT}_OBJTOP?=	${OBJTOP}/obj-lib${_libcompat}

LIB${_LIBCOMPAT}CFLAGS+=	${LIB${_LIBCOMPAT}CPUFLAGS} \
				${LIB${_LIBCOMPAT}CPUFLAGS.${COMPAT_COMPILER_TYPE}} \
				-DCOMPAT_LIBCOMPAT=\"${_LIBCOMPAT}\" \
				-DCOMPAT_libcompat=\"${_libcompat}\" \
				-DCOMPAT_LIB${_LIBCOMPAT} \
				--sysroot=${WORLDTMP} \
				${BFLAGS}

LIB${_LIBCOMPAT}LDFLAGS+=	-L${WORLDTMP}/usr/lib${_libcompat}

LIB${_LIBCOMPAT}WMAKEFLAGS+=	COMPAT_LIBCOMPAT=${_LIBCOMPAT} \
				COMPAT_libcompat=${_libcompat}

LIB${_LIBCOMPAT}WMAKEENV+=	MACHINE=${LIB${_LIBCOMPAT}_MACHINE}
LIB${_LIBCOMPAT}WMAKEENV+=	MACHINE_ARCH=${LIB${_LIBCOMPAT}_MACHINE_ARCH}

# -B is needed to find /usr/lib32/crti.o for gcc.
LIB${_LIBCOMPAT}CFLAGS+=	-B${WORLDTMP}/usr/lib${_libcompat}
.endfor

.if defined(USE_COMPAT)
LIB${USE_COMPAT}CPUFLAGS+= ${LIB${USE_COMPAT}CPUFLAGS.${COMPAT_COMPILER_TYPE}}

libcompat=	${USE_COMPAT:tl}

_LIBCOMPAT_MAKEVARS=	_OBJTOP TMP CPUFLAGS CFLAGS CXXFLAGS LDFLAGS \
			_MACHINE_ABI \
			WMAKEENV WMAKEFLAGS WMAKE WORLDTMP
.for _var in ${_LIBCOMPAT_MAKEVARS}
.if !empty(LIB${USE_COMPAT}${_var})
LIBCOMPAT${_var}?=	${LIB${USE_COMPAT}${_var}}
.endif
.endfor

LIBDIR_BASE:=	/usr/lib${libcompat}
LIBDATADIR:=	/usr/lib${libcompat}
_LIB_OBJTOP=	${LIBCOMPAT_OBJTOP}
CFLAGS+=	${LIBCOMPATCFLAGS}
LDFLAGS+=	${CFLAGS} ${LIBCOMPATLDFLAGS}
.endif

.endif
