# $FreeBSD$

.include <bsd.init.mk>
MK_SSP=		no
MAN=

PROG=		loader.kboot
NEWVERSWHAT=	"kboot loader" ${MACHINE_ARCH}
INSTALLFLAGS=	-b

# Architecture-specific loader code
SRCS=		conf.c metadata.c vers.c main.c ppc64_elf_freebsd.c
SRCS+=		host_syscall.S hostcons.c hostdisk.c kerneltramp.S kbootfdt.c
SRCS+=		ucmpdi2.c

LOADER_CD9660_SUPPORT?=	yes
LOADER_MSDOS_SUPPORT?=	no
LOADER_EXT2FS_SUPPORT?=	yes
LOADER_UFS_SUPPORT?=	yes
LOADER_NET_SUPPORT?=	yes
LOADER_NFS_SUPPORT?=	yes
LOADER_TFTP_SUPPORT?=	no
LOADER_GZIP_SUPPORT?=	yes
LOADER_FDT_SUPPORT=	yes
LOADER_BZIP2_SUPPORT?=	no

.if ${LOADER_FDT_SUPPORT} == "yes"
CFLAGS+=	-I${FDTSRC}
CFLAGS+=	-I${SYSDIR}/contrib/libfdt
CFLAGS+=	-DLOADER_FDT_SUPPORT
LIBFDT=		${BOOTOBJ}/fdt/libfdt.a
.endif

CFLAGS+=	-mcpu=powerpc64

# Always add MI sources
.include	"${BOOTSRC}/loader.mk"
.PATH:		${SYSDIR}/libkern
CFLAGS+=	-I${SYSDIR}
CFLAGS+=	-I.

CLEANFILES+=	loader.help

CFLAGS+=	-Wall -ffreestanding -msoft-float -DAIM
# load address. set in linker script
RELOC?=		0x0
CFLAGS+=	-DRELOC=${RELOC}

LDFLAGS=	-nostdlib -static -T ${.CURDIR}/ldscript.powerpc

# 64-bit bridge extensions
CFLAGS+= -Wa,-mppc64bridge

DPADD=		${LIBFICL} ${LIBOFW} ${LIBFDT} ${LIBSA}
LDADD=		${LIBFICL} ${LIBOFW} ${LIBFDT} ${LIBSA}

loader.help: help.common help.kboot ${FDTSRC}/help.fdt
	cat ${.ALLSRC} | \
	    awk -f ${LDRSRC}/merge_help.awk > ${.TARGET}

.PATH: ${BOOTSRC}/forth
.include	"${BOOTSRC}/forth/Makefile.inc"

FILES+= loader.rc menu.rc

.include <bsd.prog.mk>
