# $FreeBSD$

.ifndef LLVM_SRCS
.error Please define LLVM_SRCS before including this file
.endif

.ifndef SRCDIR
.error Please define SRCDIR before including this file
.endif

.PATH:		${LLVM_SRCS}/${SRCDIR}

CFLAGS+=	-I${.CURDIR}/../../../lib/clang/include
CFLAGS+=	-I${LLVM_SRCS}/include
CFLAGS+=	-DLLVM_ON_UNIX
CFLAGS+=	-DLLVM_ON_FREEBSD
CFLAGS+=	-D__STDC_LIMIT_MACROS
CFLAGS+=	-D__STDC_CONSTANT_MACROS
#CFLAGS+=	-DNDEBUG

TARGET_ARCH?=	${MACHINE_ARCH}
BUILD_ARCH?=	${MACHINE_ARCH}

# Armv6 uses hard float abi, unless the CPUTYPE has soft in it.
# arm (for armv4 and armv5 CPUs) always uses the soft float ABI.
# For all other targets, we stick with 'unknown'.
.if ${TARGET_ARCH:Marmv6*} && (!defined(CPUTYPE) || ${CPUTYPE:M*soft*} == "")
TARGET_ABI=	gnueabihf
.elif ${TARGET_ARCH:Marm*}
TARGET_ABI=	gnueabi
.else
TARGET_ABI=	unknown
.endif
OS_VERSION=	freebsd12.0

TARGET_TRIPLE?=	${TARGET_ARCH:C/amd64/x86_64/:C/arm64/aarch64/}-${TARGET_ABI}-${OS_VERSION}
BUILD_TRIPLE?=	${BUILD_ARCH:C/amd64/x86_64/:C/arm64/aarch64/}-unknown-${OS_VERSION}

CFLAGS+=	-DLLVM_DEFAULT_TARGET_TRIPLE=\"${TARGET_TRIPLE}\"
CFLAGS+=	-DLLVM_HOST_TRIPLE=\"${BUILD_TRIPLE}\"
CFLAGS+=	-DDEFAULT_SYSROOT=\"${TOOLS_PREFIX}\"

CXXFLAGS+=	-std=c++11
CXXFLAGS+=	-fno-exceptions
CXXFLAGS+=	-fno-rtti
CXXFLAGS.clang+= -stdlib=libc++

.if ${MACHINE_CPUARCH} == "arm"
STATIC_CXXFLAGS+= -mlong-calls
.endif
