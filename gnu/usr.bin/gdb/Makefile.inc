# $FreeBSD$

.include <src.opts.mk>

WARNS?=	0

VERSION= "6.1.1 [FreeBSD]"
VENDOR= marcel

PACKAGE=	gdb

BMAKE_GDB= ${.CURDIR:H}
BMAKE_ROOT= ${BMAKE_GDB:H}
BMAKE_BU= ${BMAKE_ROOT}/binutils

CNTRB_BU= ${SRCTOP}/contrib/binutils
CNTRB_GDB= ${SRCTOP}/contrib/gdb

OBJ_BU= ${OBJTOP}/gnu/usr.bin/binutils
OBJ_GDB= ${OBJTOP}/gnu/usr.bin/gdb

# These assignments duplicate much of the functionality of
# MACHINE_CPUARCH, but there's no easy way to export make functions...

.if defined(TARGET_ARCH)
TARGET_CPUARCH=${TARGET_ARCH:${__TO_CPUARCH}}
.else
TARGET_CPUARCH=${MACHINE_CPUARCH}
.endif
TARGET_ARCH?=	${MACHINE_ARCH}
TARGET_SUBDIR= ${BMAKE_GDB}/arch/${TARGET_CPUARCH}

.if ${TARGET_ARCH} != ${MACHINE_ARCH}
GDB_CROSS_DEBUGGER=
.endif

.PATH:	${CNTRB_GDB}/gdb ${CNTRB_GDB}/gdb/cli ${CNTRB_GDB}/gdb/mi	\
	${CNTRB_GDB}/gdb/signals ${CNTRB_GDB}/gdb/tui ${TARGET_SUBDIR}

CFLAGS+= -DHAVE_CONFIG_H -DRL_NO_COMPAT -DMI_OUT=1
CFLAGS+= -DDEBUGDIR=\"${DEBUGDIR}\"
CFLAGS+= -I.
CFLAGS+= -I${TARGET_SUBDIR}
CFLAGS+= -I${BMAKE_BU}/libbfd -I${BMAKE_BU}/libbfd/${TARGET_CPUARCH}
CFLAGS+= -I${CNTRB_GDB}/gdb
CFLAGS+= -I${CNTRB_GDB}/gdb/config
CFLAGS+= -I${CNTRB_BU}/include
CFLAGS+= -I${CNTRB_GDB}/include
CFLAGS+= -I${CNTRB_BU}/bfd
CFLAGS+= -I${SYSROOT:U${DESTDIR}}/${INCLUDEDIR}/edit

# Some bits here currently rely on some of the linker-merging magic that happens
# with -fcommon.  While this is the default right now, explicitly set -fcommon
# so that it continues to build when the default flips.
CFCOMMONFLAG= -fcommon

GENSRCS+= nm.h tm.h

.if defined(GDB_CROSS_DEBUGGER)
CFLAGS+= -DCROSS_DEBUGGER -I${BMAKE_ROOT:H:H}
GDB_SUFFIX= -${TARGET_ARCH}
MAN=
.else
BINDIR?=	/usr/libexec
MAN=
.endif

.include "${TARGET_SUBDIR}/Makefile"

SRCS+= ${GENSRCS}
CLEANFILES+= ${GENSRCS}

.include "../Makefile.inc"
