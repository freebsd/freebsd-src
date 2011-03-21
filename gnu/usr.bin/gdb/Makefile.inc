# $FreeBSD$

VERSION= "6.1.1 [FreeBSD]"
VENDOR= marcel

BMAKE_GDB= ${.CURDIR}/..
BMAKE_ROOT= ${BMAKE_GDB}/..
BMAKE_BU= ${BMAKE_ROOT}/binutils

CNTRB_ROOT= ${BMAKE_ROOT}/../../contrib
CNTRB_BU= ${CNTRB_ROOT}/binutils
CNTRB_GDB= ${CNTRB_ROOT}/gdb
CNTRB_RL= ${CNTRB_ROOT}/libreadline

OBJ_ROOT= ${.OBJDIR}/../..
OBJ_BU= ${OBJ_ROOT}/binutils
OBJ_GDB= ${OBJ_ROOT}/gdb

# These assignments duplicate much of the functionality of
# MACHINE_CPUARCH, but there's no easy way to export make functions...

.if defined(TARGET_ARCH)
TARGET_CPUARCH=${TARGET_ARCH:C/mips.*e[bl]/mips/:C/armeb/arm/:C/powerpc64/powerpc/}
.else
TARGET_CPUARCH=${MACHINE_CPUARCH}
.endif
TARGET_ARCH?=	${MACHINE_ARCH}
TARGET_SUBDIR= ${BMAKE_GDB}/arch/${TARGET_CPUARCH}

.if ${TARGET_ARCH} != ${MACHINE_ARCH}
GDB_CROSS_DEBUGGER=
.endif

.if !defined(INFO)

.PATH:	${CNTRB_GDB}/gdb ${CNTRB_GDB}/gdb/cli ${CNTRB_GDB}/gdb/mi	\
	${CNTRB_GDB}/gdb/signals ${CNTRB_GDB}/gdb/tui ${TARGET_SUBDIR}

CFLAGS+= -DHAVE_CONFIG_H -DRL_NO_COMPAT -DMI_OUT=1 -DTUI=1
CFLAGS+= -I.
CFLAGS+= -I${TARGET_SUBDIR}
CFLAGS+= -I${BMAKE_BU}/libbfd -I${BMAKE_BU}/libbfd/${TARGET_CPUARCH}
CFLAGS+= -I${CNTRB_GDB}/gdb
CFLAGS+= -I${CNTRB_GDB}/gdb/config
CFLAGS+= -I${CNTRB_BU}/include
CFLAGS+= -I${CNTRB_GDB}/include
CFLAGS+= -I${CNTRB_BU}/bfd

GENSRCS+= nm.h tm.h

.if defined(GDB_CROSS_DEBUGGER)
CFLAGS+= -DCROSS_DEBUGGER -I${BMAKE_ROOT}/../..
GDB_SUFFIX= -${TARGET_ARCH}
NO_MAN=
.endif

.include "${TARGET_SUBDIR}/Makefile"

SRCS+= ${GENSRCS}
CLEANFILES+= ${GENSRCS}

.else

.PATH:	${CNTRB_GDB}/gdb ${CNTRB_GDB}/gdb/doc ${CNTRB_RL}/doc

.endif

.include "../Makefile.inc"
