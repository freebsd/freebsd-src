# $FreeBSD$

.include <src.opts.mk>

.if !defined(__BOOT_DEFS_MK__)
__BOOT_DEFS_MK__=${MFILE}

BOOTDIR=	${SRCTOP}/sys/boot
FICLDIR=	${SRCTOP}/sys/boot/ficl
LDR_MI=		${BOOTDIR}/common
SASRC=		${SRCTOP}/sys/boot/libsa
SYSDIR=		${SRCTOP}/sys

# NB: The makefiles depend on these being empty when we don't build forth.
.if ${MK_FORTH} != "no"
LIBFICL=	${OBJTOP}/sys/boot/ficl/libficl.a
LIBFICL32=	${OBJTOP}/sys/boot/ficl32/libficl.a
.endif
LIBSA=		${OBJTOP}/sys/boot/libsa/libsa.a
LIBSA32=	${OBJTOP}/sys/boot/libsa32/libsa32.a

.endif # __BOOT_DEFS_MK__
