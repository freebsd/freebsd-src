# $FreeBSD$

.include "../Makefile.inc"

# Sometimes this is .include'd several times...
.if !defined(GCCDIR)
#GCCDIR=		${.CURDIR}/../../../../contrib/gcc
GCCDIR=		/usr/src/contrib/gcc

.if ${MACHINE_ARCH} == "mipsel" || ${MACHINE_ARCH} == "mipseb"
GCC_ARCH=mips
.else
GCC_ARCH=${MACHINE_ARCH}
.endif

# Machine description.
MD_FILE=	${GCCDIR}/config/${GCC_ARCH}/${GCC_ARCH}.md
OUT_FILE=	${GCC_ARCH}.c
OUT_OBJ=	${GCC_ARCH}
target=		${MACHINE_ARCH}-unknown-freebsd

# These architectures are Cygnus's default for enabling Haifa
.if (${GCC_ARCH} == "alpha") || (${GCC_ARCH} == "sparc")
USE_EGCS_HAIFA=1
.endif

version!=	sed -e 's/.*\"\([^ \"]*\)[ \"].*/\1/' < ${GCCDIR}/version.c
#version!=	sed -e 's/.*\(egcs-[0-9\.]*\).*/\1/' < ${GCCDIR}/version.c 

CFLAGS+=	-DFREEBSD_NATIVE -DIN_GCC -DHAVE_CONFIG_H
CFLAGS+=	-DDEFAULT_TARGET_VERSION=\"$(version)\"
CFLAGS+=	-DDEFAULT_TARGET_MACHINE=\"$(target)\"

.if defined(USE_EGCS_HAIFA)
CFLAGS+=	-DHAIFA
.endif

# If hw.machine_arch doesn't match ${MACHINE_ARCH}, then we're cross 
# compiling.  gcc needs to know about this, so we set the -DCROSS_COMPILE
# define.
__M!=/sbin/sysctl -b hw.machine_arch
.if ${__M} != ${MACHINE_ARCH}
CFLAGS+=	-DCROSS_COMPILE
.endif

.if exists(${.OBJDIR}/../cc_tools)
CFLAGS+=	-I${.OBJDIR}/../cc_tools
.endif
CFLAGS+=	-I${.CURDIR}/../cc_tools
CFLAGS+=	-I${GCCDIR} -I${GCCDIR}/config

.if exists(${.OBJDIR}/../cc_int)
LIBDESTDIR=	${.OBJDIR}/..
.else
LIBDESTDIR=	${.CURDIR}/..
.endif

LIBCC_INT=	${LIBDESTDIR}/cc_int/libcc_int.a
LIBCC_DRV=	${LIBDESTDIR}/cc_drv/libcc_drv.a
LIBCC_FBSD=	${LIBDESTDIR}/cc_fbsd/libcc_fbsd.a

.endif # !GCCDIR
