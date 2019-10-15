# $FreeBSD$

.if !targets(__<${_this:T}>__)
__<${_this:T}>__:

# Makefile for the compatibility libraries.
# - 32-bit compat libraries on MIPS, PowerPC, and AMD64.

# -------------------------------------------------------------------
# 32 bit world
.if ${TARGET_ARCH} == "amd64"
.if empty(TARGET_CPUTYPE)
LIB32CPUFLAGS=	-march=i686 -mmmx -msse -msse2
.else
LIB32CPUFLAGS=	-march=${TARGET_CPUTYPE}
.endif
.if ${WANT_COMPILER_TYPE} == gcc || \
    (defined(X_COMPILER_TYPE) && ${X_COMPILER_TYPE} == gcc)
.else
LIB32CPUFLAGS+=	-target x86_64-unknown-freebsd13.0
.endif
LIB32CPUFLAGS+=	-m32
LIB32WMAKEENV=	MACHINE=i386 MACHINE_ARCH=i386 \
		MACHINE_CPU="i686 mmx sse sse2"
LIB32WMAKEFLAGS=	\
		AS="${XAS} --32" \
		LD="${XLD} -m elf_i386_fbsd -L${LIBCOMPATTMP}/usr/lib32"

.elif ${TARGET_ARCH} == "powerpc64"
.if empty(TARGET_CPUTYPE)
LIB32CPUFLAGS=	-mcpu=powerpc
.else
LIB32CPUFLAGS=	-mcpu=${TARGET_CPUTYPE}
.endif
LIB32CPUFLAGS+=	-m32
LIB32WMAKEENV=	MACHINE=powerpc MACHINE_ARCH=powerpc
LIB32WMAKEFLAGS=	\
		LD="${XLD} -m elf32ppc_fbsd"

.elif ${TARGET_ARCH:Mmips64*} != ""
.if ${WANT_COMPILER_TYPE} == gcc || \
    (defined(X_COMPILER_TYPE) && ${X_COMPILER_TYPE} == gcc)
.if empty(TARGET_CPUTYPE)
LIB32CPUFLAGS=	-march=mips3
.else
LIB32CPUFLAGS=	-march=${TARGET_CPUTYPE}
.endif
.else
.if ${TARGET_ARCH:Mmips64el*} != ""
LIB32CPUFLAGS=  -target mipsel-unknown-freebsd13.0
.else
LIB32CPUFLAGS=  -target mips-unknown-freebsd13.0
.endif
.endif
LIB32CPUFLAGS+= -mabi=32
LIB32WMAKEENV=	MACHINE=mips MACHINE_ARCH=mips
.if ${TARGET_ARCH:Mmips64el*} != ""
LIB32WMAKEFLAGS= LD="${XLD} -m elf32ltsmip_fbsd"
.else
LIB32WMAKEFLAGS= LD="${XLD} -m elf32btsmip_fbsd"
.endif
.endif

LIB32WMAKEFLAGS+= NM="${XNM}"
LIB32WMAKEFLAGS+= OBJCOPY="${XOBJCOPY}"

LIB32CFLAGS=	-DCOMPAT_32BIT
LIB32DTRACE=	${DTRACE} -32
LIB32WMAKEFLAGS+=	-DCOMPAT_32BIT

# -------------------------------------------------------------------
# soft-fp world
.if ${TARGET_ARCH:Marmv[67]*} != ""
LIBSOFTCFLAGS=        -DCOMPAT_SOFTFP
LIBSOFTCPUFLAGS= -mfloat-abi=softfp
LIBSOFTWMAKEENV= CPUTYPE=soft MACHINE=arm MACHINE_ARCH=${TARGET_ARCH}
LIBSOFTWMAKEFLAGS=        -DCOMPAT_SOFTFP
.endif


# -------------------------------------------------------------------
# Generic code for each type.
# Set defaults based on type.
libcompat=	${LIBCOMPAT:tl}
_LIBCOMPAT_MAKEVARS=	_OBJTOP TMP CPUFLAGS CFLAGS CXXFLAGS WMAKEENV \
			WMAKEFLAGS WMAKE
.for _var in ${_LIBCOMPAT_MAKEVARS}
.if !empty(LIB${LIBCOMPAT}${_var})
LIBCOMPAT${_var}?=	${LIB${LIBCOMPAT}${_var}}
.endif
.endfor

# Shared flags
LIBCOMPAT_OBJTOP?=	${OBJTOP}/obj-lib${libcompat}
LIBCOMPATTMP?=		${LIBCOMPAT_OBJTOP}/tmp

LIBCOMPATCFLAGS+=	${LIBCOMPATCPUFLAGS} \
			-L${LIBCOMPATTMP}/usr/lib${libcompat} \
			--sysroot=${LIBCOMPATTMP} \
			${BFLAGS}

# -B is needed to find /usr/lib32/crti.o for GCC and /usr/libsoft/crti.o for
# Clang/GCC.
LIBCOMPATCFLAGS+=	-B${LIBCOMPATTMP}/usr/lib${libcompat}

.endif
