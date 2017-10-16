# $FreeBSD$

.include <src.opts.mk>

PROG=		ubldr
NEWVERSWHAT=	"U-Boot loader" ${MACHINE_ARCH}
BINDIR?=	/boot
INSTALLFLAGS=	-b
MAN=

# Architecture-specific loader code
SRCS=		start.S conf.c vers.c
SRCS+=		ucmpdi2.c

LOADER_UFS_SUPPORT?=	yes
LOADER_CD9660_SUPPORT?=	no
LOADER_EXT2FS_SUPPORT?=	no
LOADER_NET_SUPPORT?=	yes
LOADER_NFS_SUPPORT?=	yes
LOADER_TFTP_SUPPORT?=	no
LOADER_GZIP_SUPPORT?=	no
LOADER_BZIP2_SUPPORT?=	no
LOADER_FDT_SUPPORT=	${MK_FDT}

.if ${LOADER_FDT_SUPPORT} == "yes"
CFLAGS+=	-I${.CURDIR}/../../fdt
CFLAGS+=	-I${.OBJDIR}/../../fdt
CFLAGS+=	-DLOADER_FDT_SUPPORT
LIBUBOOT_FDT=	${.OBJDIR}/../../uboot/fdt/libuboot_fdt.a
LIBFDT=		${.OBJDIR}/../../fdt/libfdt.a
.endif

# Always add MI sources
.include	"../../loader.mk"
.PATH:		${.CURDIR}/../../../libkern
CFLAGS+=	-I${.CURDIR}/../../..
CFLAGS+=	-I.

CLEANFILES+=	${PROG}.help

CFLAGS+=	-ffreestanding

LDFLAGS=	-nostdlib -static -T ${.CURDIR}/ldscript.powerpc

# Pull in common loader code
.PATH:		${.CURDIR}/../../uboot/common
.include	"${.CURDIR}/../../uboot/common/Makefile.inc"
CFLAGS+=	-I${.CURDIR}/../../uboot/common

# U-Boot standalone support library
LIBUBOOT=	${.OBJDIR}/../../uboot/lib/libuboot.a
CFLAGS+=	-I${.CURDIR}/../../uboot/lib
CFLAGS+=	-I${.OBJDIR}/../../uboot/lib

DPADD=		${LIBFICL} ${LIBUBOOT} ${LIBFDT} ${LIBUBOOT_FDT} ${LIBSA32}
LDADD=		${LIBFICL} ${LIBUBOOT} ${LIBFDT} ${LIBUBOOT_FDT} ${LIBSA32}

loader.help: help.common help.uboot ${.CURDIR}/../../fdt/help.fdt
	cat ${.ALLSRC} | \
	    awk -f ${LDR_MI}/merge_help.awk > ${.TARGET}

.PATH: ${.CURDIR}/../../forth
FILES=	loader.help

.include <bsd.prog.mk>
