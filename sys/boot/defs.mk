# $FreeBSD$

.if !defined(__BOOT_DEFS_MK__)
__BOOT_DEFS_MK__=${MFILE}

BOOTDIR=	${SRCTOP}/sys/boot
FICLDIR=	${SRCTOP}/sys/boot/ficl
LDR_MI=		${BOOTDIR}/common
SASRC=		${SRCTOP}/sys/boot/libsa
SYSDIR=		${SRCTOP}/sys

# Normal Standalone library
LIBSA=		${OBJTOP}/sys/boot/libsa/libsa.a
# Standalone library compiled for 32-bit version of the processor
LIBSA32=	${OBJTOP}/sys/boot/libsa32/libsa32.a

.endif # __BOOT_DEFS_MK__
