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

TARGET_ARCH?= ${MACHINE_ARCH}
TARGET_SUBDIR= ${BMAKE_GDB}/arch/${TARGET_ARCH}

.if !defined(INFO)

.PATH:	${CNTRB_GDB}/gdb ${CNTRB_GDB}/gdb/cli ${CNTRB_GDB}/gdb/mi	\
	${CNTRB_GDB}/gdb/signals ${CNTRB_GDB}/gdb/tui ${TARGET_SUBDIR}

CFLAGS+= -DHAVE_CONFIG_H -DRL_NO_COMPAT -DMI_OUT=1 -DTUI=1
CFLAGS+= -I.
CFLAGS+= -I${TARGET_SUBDIR}
CFLAGS+= -I${BMAKE_BU}/libbfd -I${BMAKE_BU}/libbfd/${TARGET_ARCH}
CFLAGS+= -I${CNTRB_GDB}/gdb
CFLAGS+= -I${CNTRB_GDB}/gdb/config
CFLAGS+= -I${CNTRB_GDB}/include
CFLAGS+= -I${CNTRB_BU}/include
CFLAGS+= -I${CNTRB_BU}/bfd

GENSRCS+= nm.h tm.h

.include "${TARGET_SUBDIR}/Makefile"

SRCS+= ${GENSRCS}
CLEANFILES+= ${GENSRCS}

.else

.PATH:	${CNTRB_GDB}/gdb ${CNTRB_GDB}/gdb/doc ${CNTRB_RL}/doc

.endif

.include "../Makefile.inc"
