# $FreeBSD$

.include <bsd.own.mk>
MK_SSP=		no

PROG=		loader.ps3
NEWVERSWHAT=	"Playstation 3 loader" ${MACHINE_ARCH}
BINDIR?=	/boot
INSTALLFLAGS=	-b

# Architecture-specific loader code
SRCS=		start.S conf.c metadata.c vers.c main.c devicename.c ppc64_elf_freebsd.c
SRCS+=		lv1call.S ps3cons.c font.h ps3mmu.c ps3net.c ps3repo.c \
		ps3stor.c ps3disk.c ps3cdrom.c
SRCS+=		ucmpdi2.c

LOADER_DISK_SUPPORT?=	yes
LOADER_UFS_SUPPORT?=	yes
LOADER_CD9660_SUPPORT?=	yes
LOADER_EXT2FS_SUPPORT?=	yes
LOADER_NET_SUPPORT?=	yes
LOADER_NFS_SUPPORT?=	yes
LOADER_TFTP_SUPPORT?=	no
LOADER_GZIP_SUPPORT?=	yes
LOADER_FDT_SUPPORT?=	no
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
.if ${LOADER_FDT_SUPPORT} == "yes"
CFLAGS+=	-I${.CURDIR}/../../fdt
CFLAGS+=	-I${.OBJDIR}/../../fdt
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
CFLAGS+=	-DNETIF_OPEN_CLOSE_ONCE -mcpu=powerpc64

# Always add MI sources
.PATH:		${.CURDIR}/../../common ${.CURDIR}/../../../libkern
.include	"${.CURDIR}/../../common/Makefile.inc"
CFLAGS+=	-I${.CURDIR}/../../common -I${.CURDIR}/../../..
CFLAGS+=	-I.

CLEANFILES+=	vers.c loader.help

CFLAGS+=	-Wall -ffreestanding -msoft-float -DAIM
# load address. set in linker script
RELOC?=		0x0
CFLAGS+=	-DRELOC=${RELOC}

LDFLAGS=	-nostdlib -static -T ${.CURDIR}/ldscript.powerpc

# 64-bit bridge extensions
CFLAGS+= -Wa,-mppc64bridge

# Pull in common loader code
#.PATH:		${.CURDIR}/../../ofw/common
#.include	"${.CURDIR}/../../ofw/common/Makefile.inc"

# where to get libstand from
CFLAGS+=	-I${.CURDIR}/../../../../lib/libstand/

DPADD=		${LIBFICL} ${LIBOFW} ${LIBSTAND}
LDADD=		${LIBFICL} ${LIBOFW} -lstand

SC_DFLT_FONT=cp437

font.h:
	uudecode < /usr/share/syscons/fonts/${SC_DFLT_FONT}-8x16.fnt && file2c 'u_char dflt_font_16[16*256] = {' '};' < ${SC_DFLT_FONT}-8x16 > font.h && uudecode < /usr/share/syscons/fonts/${SC_DFLT_FONT}-8x14.fnt && file2c 'u_char dflt_font_14[14*256] = {' '};' < ${SC_DFLT_FONT}-8x14 >> font.h && uudecode < /usr/share/syscons/fonts/${SC_DFLT_FONT}-8x8.fnt && file2c 'u_char dflt_font_8[8*256] = {' '};' < ${SC_DFLT_FONT}-8x8 >> font.h

vers.c:	${.CURDIR}/../../common/newvers.sh ${.CURDIR}/version
	sh ${.CURDIR}/../../common/newvers.sh ${.CURDIR}/version ${NEWVERSWHAT}

loader.help: help.common help.ps3
	cat ${.ALLSRC} | \
	    awk -f ${.CURDIR}/../../common/merge_help.awk > ${.TARGET}

.PATH: ${.CURDIR}/../../forth
FILES=	loader.help loader.4th support.4th loader.conf
FILES+=	screen.4th frames.4th
FILES+=	beastie.4th brand.4th check-password.4th color.4th delay.4th
FILES+=	menu.4th menu-commands.4th shortcuts.4th version.4th
FILESDIR_loader.conf=	/boot/defaults

.if !exists(${DESTDIR}/boot/loader.rc)
FILES+= loader.rc
.endif

.if !exists(${DESTDIR}/boot/menu.rc)
FILES+= menu.rc
.endif

.include <bsd.prog.mk>
