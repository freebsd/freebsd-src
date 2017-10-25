# $FreeBSD$

.include <src.opts.mk>
MK_SSP=		no
MAN=

PROG=		loader
NEWVERSWHAT=	"Open Firmware loader" ${MACHINE_ARCH}
INSTALLFLAGS=	-b

# Architecture-specific loader code
SRCS=		conf.c metadata.c vers.c start.c
SRCS+=		ucmpdi2.c

LOADER_CD9660_SUPPORT?=	yes
LOADER_EXT2FS_SUPPORT?=	no
LOADER_MSDOS_SUPPORT?=	no
LOADER_UFS_SUPPORT?=	yes
LOADER_NET_SUPPORT?=	yes
LOADER_NFS_SUPPORT?=	yes
LOADER_TFTP_SUPPORT?=	no
LOADER_GZIP_SUPPORT?=	yes
LOADER_BZIP2_SUPPORT?=	no
LOADER_FDT_SUPPORT?=	yes

.if ${LOADER_FDT_SUPPORT} == "yes"
SRCS+=		ofwfdt.c
CFLAGS+=	-I${.CURDIR}/../../fdt
CFLAGS+=	-I${.OBJDIR}/../../fdt
CFLAGS+=	-I${.CURDIR}/../../../contrib/libfdt
CFLAGS+=	-DLOADER_FDT_SUPPORT
LIBFDT=		${.OBJDIR}/../../fdt/libfdt.a
.endif

# Always add MI sources
.include	"../../loader.mk"

.PATH:		${.CURDIR}/../../../libkern
CFLAGS+=	-I${.CURDIR}/../../..
CFLAGS+=	-I.

CLEANFILES+=	loader.help

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

DPADD=		${LIBFICL} ${LIBOFW} ${LIBFDT} ${LIBSA32}
LDADD=		${LIBFICL} ${LIBOFW} ${LIBFDT} ${LIBSA32}

loader.help: help.common help.ofw ${.CURDIR}/../../fdt/help.fdt
	cat ${.ALLSRC} | \
	    awk -f ${LDRSRC}/merge_help.awk > ${.TARGET}

.PATH: ${.CURDIR}/../../forth
.include	"${.CURDIR}/../../forth/Makefile.inc"

FILES+= loader.rc menu.rc

.include <bsd.prog.mk>
