# $FreeBSD: src/sys/boot/powerpc/loader/Makefile,v 1.19.6.1 2008/11/25 02:59:29 kensmith Exp $

.include <bsd.own.mk>

PROG=		loader
NEWVERSWHAT=	"bootstrap loader" "Open Firmware/PowerPC"
BINDIR?=	/boot
INSTALLFLAGS=	-b

# Architecture-specific loader code
SRCS=		conf.c metadata.c vers.c start.c

LOADER_DISK_SUPPORT?=	yes
LOADER_UFS_SUPPORT?=	yes
LOADER_CD9660_SUPPORT?=	yes
LOADER_EXT2FS_SUPPORT?=	no
LOADER_NET_SUPPORT?=	yes
LOADER_NFS_SUPPORT?=	yes
LOADER_TFTP_SUPPORT?=	yes
LOADER_GZIP_SUPPORT?=	yes
LOADER_BZIP2_SUPPORT?=	no

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

.if ${MK_FORTH} != "no"
# Enable BootForth
BOOT_FORTH=	yes
CFLAGS+=	-DBOOT_FORTH -I${.CURDIR}/../../ficl -I${.CURDIR}/../../ficl/powerpc
LIBFICL=	${.OBJDIR}/../../ficl/libficl.a
.endif

# Always add MI sources
.PATH:		${.CURDIR}/../../common
.include	"${.CURDIR}/../../common/Makefile.inc"
CFLAGS+=	-I${.CURDIR}/../../common
CFLAGS+=	-I.

CLEANFILES+=	vers.c loader.help

CFLAGS+=	-ffreestanding
# load address. set in linker script
RELOC?=		0x1C00000
CFLAGS+=	-DRELOC=${RELOC}

LDFLAGS=	-nostdlib -static -T ${.CURDIR}/ldscript.powerpc

# 64-bit bridge extensions
CFLAGS+= -Wa,-mppc64bridge

# Pull in common loader code
.PATH:		${.CURDIR}/../../ofw/common
.include	"${.CURDIR}/../../ofw/common/Makefile.inc"

# Open Firmware standalone support library
LIBOFW=		${.OBJDIR}/../../ofw/libofw/libofw.a
CFLAGS+=	-I${.CURDIR}/../../ofw/libofw

# where to get libstand from
CFLAGS+=	-I${.CURDIR}/../../../../lib/libstand/

DPADD=		${LIBFICL} ${LIBOFW} ${LIBSTAND}
LDADD=		${LIBFICL} ${LIBOFW} -lstand

vers.c:	${.CURDIR}/../../common/newvers.sh ${.CURDIR}/version
	sh ${.CURDIR}/../../common/newvers.sh ${.CURDIR}/version ${NEWVERSWHAT}

loader.help: help.common help.ofw
	cat ${.ALLSRC} | \
	    awk -f ${.CURDIR}/../../common/merge_help.awk > ${.TARGET}

.PATH: ${.CURDIR}/../../forth
FILES=	loader.help loader.4th support.4th loader.conf
FILESDIR_loader.conf=	/boot/defaults

.if !exists(${DESTDIR}/boot/loader.rc)
FILES+= loader.rc
.endif

.include <bsd.prog.mk>
