# $FreeBSD$

.include <src.opts.mk>
MK_SSP=		no
MAN=

PROG=		loader.ps3
NEWVERSWHAT=	"Playstation 3 loader" ${MACHINE_ARCH}
BINDIR?=	/boot
INSTALLFLAGS=	-b

# Architecture-specific loader code
SRCS=		start.S conf.c metadata.c vers.c main.c devicename.c ppc64_elf_freebsd.c
SRCS+=		lv1call.S ps3cons.c font.h ps3mmu.c ps3net.c ps3repo.c \
		ps3stor.c ps3disk.c ps3cdrom.c
SRCS+=		ucmpdi2.c

LOADER_UFS_SUPPORT?=	yes
LOADER_CD9660_SUPPORT?=	yes
LOADER_EXT2FS_SUPPORT?=	yes
LOADER_NET_SUPPORT?=	yes
LOADER_NFS_SUPPORT?=	yes
LOADER_TFTP_SUPPORT?=	no
LOADER_GZIP_SUPPORT?=	yes
LOADER_FDT_SUPPORT?=	no
LOADER_BZIP2_SUPPORT?=	no

.if ${LOADER_FDT_SUPPORT} == "yes"
CFLAGS+=	-I${.CURDIR}/../../fdt
CFLAGS+=	-I${.OBJDIR}/../../fdt
CFLAGS+=	-DLOADER_FDT_SUPPORT
LIBFDT=		${.OBJDIR}/../../fdt/libfdt.a
.endif

CFLAGS+=	-mcpu=powerpc64

# Always add MI sources
.include	"../../loader.mk"
.PATH:		${.CURDIR}/../../../libkern
CFLAGS+=	-I${.CURDIR}/../../..
CFLAGS+=	-I.

CLEANFILES+=	loader.help

CFLAGS+=	-Wall -ffreestanding -msoft-float -DAIM
# load address. set in linker script
RELOC?=		0x0
CFLAGS+=	-DRELOC=${RELOC}

LDFLAGS=	-nostdlib -static -T ${.CURDIR}/ldscript.powerpc

# Pull in common loader code
#.PATH:		${.CURDIR}/../../ofw/common
#.include	"${.CURDIR}/../../ofw/common/Makefile.inc"

DPADD=		${LIBFICL} ${LIBOFW} ${LIBSA32}
LDADD=		${LIBFICL} ${LIBOFW} ${LIBSA32}

SC_DFLT_FONT=cp437

font.h:
	uudecode < /usr/share/syscons/fonts/${SC_DFLT_FONT}-8x16.fnt && file2c 'u_char dflt_font_16[16*256] = {' '};' < ${SC_DFLT_FONT}-8x16 > font.h && uudecode < /usr/share/syscons/fonts/${SC_DFLT_FONT}-8x14.fnt && file2c 'u_char dflt_font_14[14*256] = {' '};' < ${SC_DFLT_FONT}-8x14 >> font.h && uudecode < /usr/share/syscons/fonts/${SC_DFLT_FONT}-8x8.fnt && file2c 'u_char dflt_font_8[8*256] = {' '};' < ${SC_DFLT_FONT}-8x8 >> font.h

loader.help: help.common help.ps3 ${.CURDIR}/../../fdt/help.fdt
	cat ${.ALLSRC} | \
	    awk -f ${LDR_MI}/merge_help.awk > ${.TARGET}

.PATH: ${.CURDIR}/../../forth
.include	"${.CURDIR}/../../forth/Makefile.inc"

FILES+= loader.rc menu.rc

.include <bsd.prog.mk>
