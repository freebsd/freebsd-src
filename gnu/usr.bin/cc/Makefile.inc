# $FreeBSD$

.include "../Makefile.inc"

# Sometimes this is .include'd several times...
.if !defined(__CC_MAKEFILE_INC__)
__CC_MAKEFILE_INC__= ${MFILE}

GCCDIR=		${.CURDIR}/../../../../contrib/gcc

TARGET_ARCH?=	${MACHINE_ARCH}

# Machine description.
MD_FILE=	${GCCDIR}/config/${TARGET_ARCH}/${TARGET_ARCH}.md
target=		${TARGET_ARCH}-unknown-freebsd

# These architectures are Cygnus's default for enabling Haifa.
.if ${TARGET_ARCH} == "alpha"
USE_EGCS_HAIFA=	1
.endif

version!=	grep version_string ${GCCDIR}/version.c \
		    | sed -e 's/.*\"\([^ \"]*\)[ \"].*/\1/'

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

.if exists(${.OBJDIR}/../cc_int)
LIBCC_INT=	${.OBJDIR}/../cc_int/libcc_int.a
.else
LIBCC_INT=	${.CURDIR}/../cc_int/libcc_int.a
.endif

.endif # !__CC_MAKEFILE_INC__
