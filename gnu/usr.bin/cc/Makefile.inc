#
# $Id: Makefile.inc,v 1.20 1997/03/28 04:45:30 jdp Exp $
#

# Sometimes this is .include'd several times...
.if !defined(GCCDIR)
GCCDIR=		${.CURDIR}/../../../../contrib/gcc
.PATH:		../cc_tools ${GCCDIR} ${GCCDIR}/cp ${GCCDIR}/objc

BISON?=		bison

# Machine description.
MD_FILE=	${GCCDIR}/config/${MACHINE}/${MACHINE}.md
OUT_FILE=	${MACHINE}.c
OUT_OBJ=	${MACHINE}
.PATH:		${GCCDIR}/config/${MACHINE}

.if ${MACHINE} == "alpha"
BINFORMAT=	elf
.else
# Pick aout for now. the elf config is not binary compatable.
BINFORMAT?=	aout
#BINFORMAT?=	elf
.endif

.if ${BINFORMAT} == aout
CFLAGS+=	-DFREEBSD_AOUT
target=		${MACHINE}-unknown-freebsd
.endif
.if ${BINFORMAT} == elf
CFLAGS+=	-DFREEBSD_ELF
target=		${MACHINE}-unknown-freebsdelf
.endif

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
