# $FreeBSD$

.include <bsd.own.mk>

PROG=		ubldr
NEWVERSWHAT=	"U-Boot loader" ${MACHINE_ARCH}
BINDIR?=	/boot
INSTALLFLAGS=	-b
WARNS?=		1
# Address at which ubldr will be loaded.
# This varies for different boards and SOCs.
UBLDR_LOADADDR?=	0x1000000

# Architecture-specific loader code
SRCS=		start.S conf.c vers.c

.if !defined(LOADER_NO_DISK_SUPPORT)
LOADER_DISK_SUPPORT?=	yes
.else
LOADER_DISK_SUPPORT=	no
.endif
LOADER_UFS_SUPPORT?=	yes
LOADER_CD9660_SUPPORT?=	no
LOADER_EXT2FS_SUPPORT?=	no
.if ${MK_NAND} != "no"
LOADER_NANDFS_SUPPORT?= yes
.else
LOADER_NANDFS_SUPPORT?= no
.endif
LOADER_NET_SUPPORT?=	yes
LOADER_NFS_SUPPORT?=	yes
LOADER_TFTP_SUPPORT?=	no
LOADER_GZIP_SUPPORT?=	no
LOADER_BZIP2_SUPPORT?=	no
.if ${MK_FDT} != "no"
LOADER_FDT_SUPPORT=	yes
.else
LOADER_FDT_SUPPORT=	no
.endif

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
.if ${LOADER_NANDFS_SUPPORT} == "yes"
CFLAGS+=	-DLOADER_NANDFS_SUPPORT
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
CFLAGS+=	-I${.CURDIR}/../../fdt
CFLAGS+=	-I${.OBJDIR}/../../fdt
CFLAGS+=	-DLOADER_FDT_SUPPORT
LIBFDT=		${.OBJDIR}/../../fdt/libfdt.a
.endif

.if !defined(NO_FORTH)
# Enable BootForth
BOOT_FORTH=	yes
CFLAGS+=	-DBOOT_FORTH -I${.CURDIR}/../../ficl -I${.CURDIR}/../../ficl/arm
LIBFICL=	${.OBJDIR}/../../ficl/libficl.a
.endif

# Always add MI sources
.PATH:		${.CURDIR}/../../common
.include	"${.CURDIR}/../../common/Makefile.inc"
CFLAGS+=	-I${.CURDIR}/../../common
CFLAGS+=	-I.

CLEANFILES+=	vers.c loader.help

CFLAGS+=	-ffreestanding -msoft-float

LDFLAGS=	-nostdlib -static
LDFLAGS+=	-T ldscript.generated
LDFLAGS+=	-T ${.CURDIR}/ldscript.${MACHINE_CPUARCH}

# Pull in common loader code
.PATH:		${.CURDIR}/../../uboot/common
.include	"${.CURDIR}/../../uboot/common/Makefile.inc"
CFLAGS+=	-I${.CURDIR}/../../uboot/common

# U-Boot standalone support library
LIBUBOOT=	${.OBJDIR}/../../uboot/lib/libuboot.a
CFLAGS+=	-I${.CURDIR}/../../uboot/lib
CFLAGS+=	-I${.OBJDIR}/../../uboot/lib

# where to get libstand from
CFLAGS+=	-I${.CURDIR}/../../../../lib/libstand/

# clang doesn't understand %D as a specifier to printf
NO_WERROR.clang=

DPADD=		${LIBFICL} ${LIBUBOOT} ${LIBFDT} ${LIBSTAND}
LDADD=		${LIBFICL} ${LIBUBOOT} ${LIBFDT} -lstand

vers.c:	${.CURDIR}/../../common/newvers.sh ${.CURDIR}/version
	sh ${.CURDIR}/../../common/newvers.sh ${.CURDIR}/version ${NEWVERSWHAT}

loader.help: help.common help.uboot ${.CURDIR}/../../fdt/help.fdt
	cat ${.ALLSRC} | \
	    awk -f ${.CURDIR}/../../common/merge_help.awk > ${.TARGET}

${PROG}: ldscript.generated ${.CURDIR}/ldscript.${MACHINE_CPUARCH}

ldscript.generated::
	rm -f ldscript.generated.tmp
	echo "UBLDR_LOADADDR = ${UBLDR_LOADADDR};" >ldscript.generated.tmp
	if diff ldscript.generated ldscript.generated.tmp > /dev/null; then \
		true; \
	else \
		rm -f ldscript.generated; \
		mv ldscript.generated.tmp ldscript.generated; \
	fi

.if !defined(LOADER_ONLY)
.PATH: ${.CURDIR}/../../forth
FILES+=	loader.help loader.4th support.4th loader.conf
FILES+=	screen.4th frames.4th beastie.4th
FILES+=	brand.4th check-password.4th color.4th delay.4th
FILES+=	menu.4th menu-commands.4th menusets.4th shortcuts.4th version.4th
FILESDIR_loader.conf=	/boot/defaults

# Put sample loader.rc and menu.rc on disk but don't enable them
# by default.
FILES+=	loader.rc
FILESNAME_loader.rc=	loader.rc.sample
FILES+=	menu.rc
FILESNAME_menu.rc=	menu.rc.sample
.endif

.include <bsd.prog.mk>
