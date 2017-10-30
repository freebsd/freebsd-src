# $FreeBSD$

.include <src.opts.mk>

.if !defined(__BOOT_DEFS_MK__)
__BOOT_DEFS_MK__=${MFILE}

BOOTSRC=	${SRCTOP}/sys/boot
EFISRC=		${BOOTSRC}/efi
EFIINC=		${EFISRC}/include
EFIINCMD=	${EFIINC}/${MACHINE}
FDTSRC=		${BOOTSRC}/fdt
FICLSRC=	${BOOTSRC}/ficl
LDRSRC=		${BOOTSRC}/common
SASRC=		${BOOTSRC}/libsa
SYSDIR=		${SRCTOP}/sys
UBOOTSRC=	${BOOTSRC}/uboot

BOOTOBJ=	${OBJTOP}/sys/boot

# BINDIR is where we install
BINDIR?=	/boot

# NB: The makefiles depend on these being empty when we don't build forth.
.if ${MK_FORTH} != "no"
LIBFICL=	${BOOTOBJ}/ficl/libficl.a
LIBFICL32=	${BOOTOBJ}/ficl32/libficl.a
.endif
LIBSA=		${BOOTOBJ}/libsa/libsa.a
.if ${MACHINE} == "i386"
LIBSA32=	${LIBSA}
.else
LIBSA32=	${BOOTOBJ}/libsa32/libsa32.a
.endif

# Standard options:

# Filesystem support
.if ${LOADER_CD9660_SUPPORT:Uno} == "yes"
CFLAGS+=	-DLOADER_CD9660_SUPPORT
.endif
.if ${LOADER_EXT2FS_SUPPORT:Uno} == "yes"
CFLAGS+=	-DLOADER_EXT2FS_SUPPORT
.endif
.if ${LOADER_MSDOS_SUPPORT:Uno} == "yes"
CFLAGS+=	-DLOADER_MSDOS_SUPPORT
.endif
.if ${LOADER_NANDFS_SUPPORT:U${MK_NAND}} == "yes"
CFLAGS+=	-DLOADER_NANDFS_SUPPORT
.endif
.if ${LOADER_UFS_SUPPORT:Uyes} == "yes"
CFLAGS+=	-DLOADER_UFS_SUPPORT
.endif

# Compression
.if ${LOADER_GZIP_SUPPORT:Uno} == "yes"
CFLAGS+=	-DLOADER_GZIP_SUPPORT
.endif
.if ${LOADER_BZIP2_SUPPORT:Uno} == "yes"
CFLAGS+=	-DLOADER_BZIP2_SUPPORT
.endif

# Network related things
.if ${LOADER_NET_SUPPORT:Uno} == "yes"
CFLAGS+=	-DLOADER_NET_SUPPORT
.endif
.if ${LOADER_NFS_SUPPORT:Uno} == "yes"
CFLAGS+=	-DLOADER_NFS_SUPPORT
.endif
.if ${LOADER_TFTP_SUPPORT:Uno} == "yes"
CFLAGS+=	-DLOADER_TFTP_SUPPORT
.endif

# Disk and partition support
.if ${LOADER_DISK_SUPPORT:Uyes} == "yes"
CFLAGS+= -DLOADER_DISK_SUPPORT
.if ${LOADER_GPT_SUPPORT:Uyes} == "yes"
CFLAGS+= -DLOADER_GPT_SUPPORT
.endif
.if ${LOADER_MBR_SUPPORT:Uyes} == "yes"
CFLAGS+= -DLOADER_MBR_SUPPORT
.endif
.if ${LOADER_GELI_SUPPORT:Uyes} == "yes"
CFLAGS+= -DLOADER_GELI_SUPPORT
.endif
.endif

.endif # __BOOT_DEFS_MK__
