#
# $Id$
#

# Sometimes this is .include'd several times...
.if !defined(GCCDIR)
GCCDIR=		${.CURDIR}/../../../../contrib/gcc
.PATH:		../cc_tools ${GCCDIR} ${GCCDIR}/cp ${GCCDIR}/objc

BISON?=		bison

# Machine description.
MD_FILE=	${GCCDIR}/config/i386/i386.md
OUT_FILE=	${GCCDIR}/config/i386/i386.c
OUT_OBJ=	i386

# Pick aout for now. the elf config is not binary compatable.
BINFORMAT=	aout
#BINFORMAT=	elf

.if ${BINFORMAT} == aout
CFLAGS+=	-DFREEBSD_AOUT
target=		i386-unknown-freebsd
.endif
.if ${BINFORMAT} == elf
CFLAGS+=	-DFREEBSD_ELF
target=		i386-unknown-freebsdelf
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

# XXX LDDESTDIR isn't a directory and there is no standard name for the dir
LDDESTDIR=	-L${LIBDESTDIR}
.if defined(SHARED_LIBCC_INT)
LIBCC_INT=	${LIBDESTDIR}/libcc_int.so.272.0
.else
LIBCC_INT=	${LIBDESTDIR}/libcc_int.a
.endif

.endif
