#
# $Id: Makefile.inc,v 1.17 1996/10/01 03:44:29 peter Exp $
#

# Sometimes this is .include'd several times...
.if !defined(GCCDIR)
GCCDIR=		${.CURDIR}/../../../../contrib/gcc
.PATH:		../cc_tools ${GCCDIR} ${GCCDIR}/cp ${GCCDIR}/objc

BISON?=		bison

# Machine description.
MD_FILE=	${GCCDIR}/config/i386/i386.md
OUT_FILE=	i386.c
OUT_OBJ=	i386
.PATH:		${GCCDIR}/config/i386

CFLAGS+=	-DFREEBSD_AOUT
target=		i386-unknown-freebsd

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
