# $FreeBSD$

.include "../Makefile.inc"

# Sometimes this is .include'd several times...
.if !defined(GCCDIR)
GCCDIR=		${.CURDIR}/../../../../contrib/gcc

TARGET_ARCH?=	${MACHINE_ARCH}

.if ${TARGET_ARCH} == "mipsel" || ${TARGET_ARCH} == "mipseb"
GCC_ARCH=	mips
.else
GCC_ARCH=	${TARGET_ARCH}
.endif

# Machine description.
MD_FILE=	${GCCDIR}/config/${GCC_ARCH}/${GCC_ARCH}.md
OUT_FILE=	${GCC_ARCH}.c
OUT_OBJ=	${GCC_ARCH}
target=		${TARGET_ARCH}-unknown-freebsd

# These architectures are Cygnus's default for enabling Haifa.
.if ${GCC_ARCH} == "alpha" || ${GCC_ARCH} == "sparc" || ${GCC_ARCH} == "sparc64"
USE_EGCS_HAIFA=	1
.endif

version!=	sed -e 's/.*\"\([^ \"]*\)[ \"].*/\1/' < ${GCCDIR}/version.c

CFLAGS+=	-DIN_GCC -DHAVE_CONFIG_H
CFLAGS+=	-DPREFIX=\"${TOOLS_PREFIX}/usr\"

# If building 64-bit longs for the i386, "_LARGE_LONG" should also be defined
# to get the proper sizes in limits.h
.if defined(LONG_TYPE_SIZE)
CFLAGS+=	-DLONG_TYPE_SIZE=${LONG_TYPE_SIZE}
.endif

.if defined(USE_EGCS_HAIFA)
CFLAGS+=	-DHAIFA
.endif

.if ${TARGET_ARCH} != ${MACHINE_ARCH}
CFLAGS+=	-DCROSS_COMPILE
.endif

.if exists(${.OBJDIR}/../cc_tools)
CFLAGS+=	-I${.OBJDIR}/../cc_tools
.endif
CFLAGS+=	-I${.CURDIR}/../cc_tools
# This must go after the -I for cc_tools to resolve ambiguities for hash.h
# correctly.
CFLAGS+=	-I${GCCDIR} -I${GCCDIR}/config

.if exists(${.OBJDIR}/../cc_fbsd)
LIBCC_FBSD=	${.OBJDIR}/../cc_fbsd/libcc_fbsd.a
.else
LIBCC_FBSD=	${.CURDIR}/../cc_fbsd/libcc_fbsd.a
.endif

.if exists(${.OBJDIR}/../cc_int)
LIBCC_INT=	${.OBJDIR}/../cc_int/libcc_int.a
.else
LIBCC_INT=	${.CURDIR}/../cc_int/libcc_int.a
.endif

.endif # !GCCDIR
