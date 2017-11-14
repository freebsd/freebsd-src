# $FreeBSD$

LOADER_CD9660_SUPPORT?=	yes
LOADER_EXT2FS_SUPPORT?=	no
LOADER_MSDOS_SUPPORT?=	no
LOADER_UFS_SUPPORT?=	yes
LOADER_NET_SUPPORT?=	yes
LOADER_NFS_SUPPORT?=	yes
LOADER_TFTP_SUPPORT?=	no
LOADER_GZIP_SUPPORT?=	yes
LOADER_BZIP2_SUPPORT?=	no

.include <bsd.init.mk>
MK_SSP=		no
MAN=

PROG=		loader
NEWVERSWHAT=	"Open Firmware loader" ${MACHINE_ARCH}
INSTALLFLAGS=	-b

# Architecture-specific loader code
SRCS=		conf.c metadata.c vers.c start.c
SRCS+=		ucmpdi2.c

.include	"${BOOTSRC}/fdt.mk"
.if ${MK_FDT} == "yes"
SRCS+=		ofwfdt.c
.endif

HELP_FILES+=	${FDTSRC}/help.fdt

# Always add MI sources
.include	"${BOOTSRC}/loader.mk"

.PATH:		${SYSDIR}/libkern

CFLAGS+=	-ffreestanding -msoft-float
# load address. set in linker script
RELOC?=		0x1C00000
CFLAGS+=	-DRELOC=${RELOC}

LDFLAGS=	-nostdlib -static -T ${.CURDIR}/ldscript.powerpc

# Pull in common loader code
.PATH:		${BOOTSRC}/ofw/common
.include	"${BOOTSRC}/ofw/common/Makefile.inc"

# Open Firmware standalone support library
LIBOFW=		${BOOTOBJ}/ofw/libofw/libofw.a
CFLAGS+=	-I${BOOTSRC}/ofw/libofw

DPADD=		${LIBFICL} ${LIBOFW} ${LIBFDT} ${LIBSA}
LDADD=		${LIBFICL} ${LIBOFW} ${LIBFDT} ${LIBSA}

.include <bsd.prog.mk>
