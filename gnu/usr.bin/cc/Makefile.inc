#
# $Id: Makefile.inc,v 1.22 1998/01/25 09:49:00 jb Exp $
#

# Sometimes this is .include'd several times...
.if !defined(GCCDIR)
GCCDIR=		${.CURDIR}/../../../../contrib/gcc
.PATH:		../cc_tools ${GCCDIR} ${GCCDIR}/cp ${GCCDIR}/objc

BISON?=		bison

# Machine description.
MD_FILE=	${GCCDIR}/config/${MACHINE_ARCH}/${MACHINE_ARCH}.md
OUT_FILE=	${MACHINE_ARCH}.c
OUT_OBJ=	${MACHINE_ARCH}
.PATH:		${GCCDIR}/config/${MACHINE_ARCH}

.if ${MACHINE} == "alpha"
BINFORMAT=	elf
.else
# Pick aout for now. the elf config is not binary compatable.
BINFORMAT?=	aout
#BINFORMAT?=	elf
.endif

# same backend, just change compiled-in default.
.if ${BINFORMAT} == aout
CFLAGS+=	-DFREEBSD_AOUT
.endif
.if ${BINFORMAT} == elf
CFLAGS+=	-DFREEBSD_ELF
.endif

target=		${MACHINE_ARCH}-unknown-freebsd
version!=	sed -e 's/.*\"\([^ \"]*\)[ \"].*/\1/' < ${GCCDIR}/version.c

CFLAGS+=	-I${GCCDIR} -I${GCCDIR}/config
CFLAGS+=	-DFREEBSD_NATIVE
CFLAGS+=	-DDEFAULT_TARGET_VERSION=\"$(version)\"
CFLAGS+=	-DDEFAULT_TARGET_MACHINE=\"$(target)\"

.if exists(${.OBJDIR}/../cc_tools)
CFLAGS+=	-I${.OBJDIR}/../cc_tools
.else
CFLAGS+=	-I${.CURDIR}/../cc_tools
.endif

.if exists(${.OBJDIR}/../cc_int)
LIBDESTDIR=	${.OBJDIR}/../cc_int
.else
LIBDESTDIR=	${.CURDIR}/../cc_int
.endif

LIBCC_INT=	${LIBDESTDIR}/libcc_int.a

.endif
