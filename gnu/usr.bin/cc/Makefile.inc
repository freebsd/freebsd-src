#
# $Id: Makefile.inc,v 1.27 1999/04/04 17:11:00 obrien Exp $
#

# Sometimes this is .include'd several times...
.if !defined(GCCDIR)
GCCDIR=		${.CURDIR}/../../../../contrib/egcs/gcc

BISON?=		bison

# Machine description.
MD_FILE=	${GCCDIR}/config/${MACHINE_ARCH}/${MACHINE_ARCH}.md
OUT_FILE=	${MACHINE_ARCH}.c
OUT_OBJ=	${MACHINE_ARCH}
target=		${MACHINE_ARCH}-unknown-freebsd

# These architectures are Cygnus's default for enabling Haifa
.if (${MACHINE_ARCH} == "alpha") || (${MACHINE_ARCH} == "sparc")
USE_EGCS_HAIFA=1
.endif

version!=	sed -e 's/.*\"\([^ \"]*\)[ \"].*/\1/' < ${GCCDIR}/version.c
#version!=	sed -e 's/.*\(egcs-[0-9\.]*\).*/\1/' < ${GCCDIR}/version.c 

CFLAGS+=	-I${GCCDIR} -I${GCCDIR}/config
CFLAGS+=	-DFREEBSD_NATIVE
CFLAGS+=	-DDEFAULT_TARGET_VERSION=\"$(version)\"
CFLAGS+=	-DDEFAULT_TARGET_MACHINE=\"$(target)\"

.if defined(USE_EGCS_HAIFA)
CFLAGS+=	-DHAIFA
.endif

.if exists(${.OBJDIR}/../cc_tools)
CFLAGS+=	-I${.OBJDIR}/../cc_tools
.endif
CFLAGS+=	-I${.CURDIR}/../cc_tools

.if exists(${.OBJDIR}/../cc_int)
LIBDESTDIR=	${.OBJDIR}/../cc_int
LIBCC_DRV=	${.OBJDIR}/../cc_drv/libcc_drv.a
.else
LIBDESTDIR=	${.CURDIR}/../cc_int
LIBCC_DRV=	${.CURDIR}/../cc_drv/libcc_drv.a
.endif

LIBCC_INT=	${LIBDESTDIR}/libcc_int.a

.endif # !GCCDIR
