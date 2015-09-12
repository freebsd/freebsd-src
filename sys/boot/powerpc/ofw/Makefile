# $FreeBSD$

.include <src.opts.mk>
MK_SSP=		no
MAN=

PROG=		loader
NEWVERSWHAT=	"Open Firmware loader" ${MACHINE_ARCH}
BINDIR?=	/boot
INSTALLFLAGS=	-b

# Architecture-specific loader code
SRCS=		conf.c metadata.c vers.c start.c
SRCS+=		ucmpdi2.c

LOADER_DISK_SUPPORT?=	yes
LOADER_UFS_SUPPORT?=	yes
LOADER_CD9660_SUPPORT?=	yes
LOADER_EXT2FS_SUPPORT?=	no
LOADER_NET_SUPPORT?=	yes
LOADER_NFS_SUPPORT?=	yes
LOADER_TFTP_SUPPORT?=	no
LOADER_GZIP_SUPPORT?=	yes
LOADER_BZIP2_SUPPORT?=	no
LOADER_FDT_SUPPORT?=	yes

.if ${LOADER_DISK_SUPPORT} == "yes"
CFLAGS+=	-DLOADER_DISK_SUPPORT
.endif
.if ${LOADER_UFS_SUPPORT} == "yes"
CFLAGS+=	-DLOADER_UFS_SUPPORT
.endif
.if ${LOADER_CD9660_SUPPORT} == "yes"
CFLAGS+=	-DLOADER_CD9660_SUPPORT
.endif
.if ${LOADER_EXT2FS_SUPPORT} == "yes"
CFLAGS+=	-DLOADER_EXT2FS_SUPPORT
.endif
.if ${LOADER_GZIP_SUPPORT} == "yes"
CFLAGS+=	-DLOADER_GZIP_SUPPORT
.endif
.if ${LOADER_BZIP2_SUPPORT} == "yes"
CFLAGS+=	-DLOADER_BZIP2_SUPPORT
.endif
.if ${LOADER_NET_SUPPORT} == "yes"
CFLAGS+=	-DLOADER_NET_SUPPORT
.endif
.if ${LOADER_NFS_SUPPORT} == "yes"
CFLAGS+=	-DLOADER_NFS_SUPPORT
.endif
.if ${LOADER_TFTP_SUPPORT} == "yes"
CFLAGS+=	-DLOADER_TFTP_SUPPORT
.endif
.if ${LOADER_FDT_SUPPORT} == "yes"
SRCS+=		ofwfdt.c
CFLAGS+=	-I${.CURDIR}/../../fdt
CFLAGS+=	-I${.OBJDIR}/../../fdt
CFLAGS+=	-I${.CURDIR}/../../../contrib/libfdt
CFLAGS+=	-DLOADER_FDT_SUPPORT
LIBFDT=		${.OBJDIR}/../../fdt/libfdt.a
.endif

.if ${MK_FORTH} != "no"
# Enable BootForth
BOOT_FORTH=	yes
CFLAGS+=	-DBOOT_FORTH -I${.CURDIR}/../../ficl -I${.CURDIR}/../../ficl/powerpc
LIBFICL=	${.OBJDIR}/../../ficl/libficl.a
.endif

# Avoid the open-close-dance for every file access as some firmwares perform
# an auto-negotiation on every open of the network interface and thus causes
# netbooting to take horribly long.
CFLAGS+=	-DNETIF_OPEN_CLOSE_ONCE

# Always add MI sources
.PATH:		${.CURDIR}/../../common ${.CURDIR}/../../../libkern
.include	"${.CURDIR}/../../common/Makefile.inc"
CFLAGS+=	-I${.CURDIR}/../../common -I${.CURDIR}/../../..
CFLAGS+=	-I.

CLEANFILES+=	vers.c loader.help

CFLAGS+=	-ffreestanding -msoft-float
# load address. set in linker script
RELOC?=		0x1C00000
CFLAGS+=	-DRELOC=${RELOC}

LDFLAGS=	-nostdlib -static -T ${.CURDIR}/ldscript.powerpc

# Pull in common loader code
.PATH:		${.CURDIR}/../../ofw/common
.include	"${.CURDIR}/../../ofw/common/Makefile.inc"

# Open Firmware standalone support library
LIBOFW=		${.OBJDIR}/../../ofw/libofw/libofw.a
CFLAGS+=	-I${.CURDIR}/../../ofw/libofw

# where to get libstand from
LIBSTAND=	${.OBJDIR}/../../libstand32/libstand.a
CFLAGS+=	-I${.CURDIR}/../../../../lib/libstand/

DPADD=		${LIBFICL} ${LIBOFW} ${LIBFDT} ${LIBSTAND}
LDADD=		${LIBFICL} ${LIBOFW} ${LIBFDT} ${LIBSTAND}

vers.c:	${.CURDIR}/../../common/newvers.sh ${.CURDIR}/version
	sh ${.CURDIR}/../../common/newvers.sh ${.CURDIR}/version ${NEWVERSWHAT}

loader.help: help.common help.ofw ${.CURDIR}/../../fdt/help.fdt
	cat ${.ALLSRC} | \
	    awk -f ${.CURDIR}/../../common/merge_help.awk > ${.TARGET}

.PATH: ${.CURDIR}/../../forth
.include	"${.CURDIR}/../../forth/Makefile.inc"

FILES+= loader.rc menu.rc

.include <bsd.prog.mk>
