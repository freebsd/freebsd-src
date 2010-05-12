# $FreeBSD$

.include "../Makefile.inc"

# Sometimes this is .include'd several times...
.if !defined(__CC_MAKEFILE_INC__)
__CC_MAKEFILE_INC__= ${MFILE}

GCCVER=	4.2
GCCDIR=	${.CURDIR}/../../../../contrib/gcc
GCCLIB=	${.CURDIR}/../../../../contrib/gcclibs

.include "Makefile.tgt"

# Machine description.
MD_FILE=	${GCCDIR}/config/${GCC_CPU}/${GCC_CPU}.md
GCC_TARGET=	${TARGET_ARCH}-undermydesk-freebsd

CFLAGS+=	-DGCCVER=\"${GCCVER}\"
CFLAGS+=	-DIN_GCC -DHAVE_CONFIG_H
CFLAGS+=	-DPREFIX=\"${TOOLS_PREFIX}/usr\"
#CFLAGS+=	-DWANT_COMPILER_INVARIANTS
CSTD?=	gnu89

# If building 64-bit longs for the i386, "_LARGE_LONG" should also be defined
# to get the proper sizes in limits.h
.if defined(LONG_TYPE_SIZE)
CFLAGS+=	-DLONG_TYPE_SIZE=${LONG_TYPE_SIZE}
.endif

.if ${TARGET_ARCH} != ${MACHINE_ARCH}
CFLAGS+=	-DCROSS_COMPILE
.endif

.if ${TARGET_ARCH} == "mips" && !defined(TARGET_BIG_ENDIAN)
CFLAGS += -DTARGET_ENDIAN_DEFAULT=0
.endif

.if defined(WANT_FORCE_OPTIMIZATION_DOWNGRADE)
CFLAGS+= -DFORCE_OPTIMIZATION_DOWNGRADE=${WANT_FORCE_OPTIMIZATION_DOWNGRADE}
.endif

.if exists(${.OBJDIR}/../cc_tools)
CFLAGS+=	-I${.OBJDIR}/../cc_tools
.endif
CFLAGS+=	-I${.CURDIR}/../cc_tools
# This must go after the -I for cc_tools to resolve ambiguities for hash.h
# correctly.
CFLAGS+=	-I${GCCDIR} -I${GCCDIR}/config

CFLAGS+=	-I${GCCLIB}/include
CFLAGS+=	-I${GCCLIB}/libcpp/include
CFLAGS+=	-I${GCCLIB}/libdecnumber

.if exists(${.OBJDIR}/../cc_int)
LIBBACKEND=	${.OBJDIR}/../cc_int/libbackend.a
.else
LIBBACKEND=	${.CURDIR}/../cc_int/libbackend.a
.endif

.if exists(${.OBJDIR}/../libiberty)
LIBIBERTY=	${.OBJDIR}/../libiberty/libiberty.a
.else
LIBIBERTY=	${.CURDIR}/../libiberty/libiberty.a
.endif

.if exists(${.OBJDIR}/../libcpp)
LIBCPP=		${.OBJDIR}/../libcpp/libcpp.a
.else
LIBCPP=		${.CURDIR}/../libcpp/libcpp.a
.endif

.if exists(${.OBJDIR}/../libdecnumber)
LIBDECNUMBER=	${.OBJDIR}/../libdecnumber/libdecnumber.a
.else
LIBDECNUMBER=	${.CURDIR}/../libdecnumber/libdecnumber.a
.endif

.endif # !__CC_MAKEFILE_INC__
