# $FreeBSD$

LOADER_CD9660_SUPPORT?=	yes
LOADER_MSDOS_SUPPORT?=	no
LOADER_EXT2FS_SUPPORT?=	yes
LOADER_UFS_SUPPORT?=	yes
LOADER_NET_SUPPORT?=	yes
LOADER_NFS_SUPPORT?=	yes
LOADER_TFTP_SUPPORT?=	no
LOADER_GZIP_SUPPORT?=	yes
LOADER_BZIP2_SUPPORT?=	no

.include <bsd.init.mk>

PROG=		loader.kboot
NEWVERSWHAT=	"kboot loader" ${MACHINE_ARCH}
INSTALLFLAGS=	-b

# Architecture-specific loader code
SRCS=		conf.c vers.c main.c ppc64_elf_freebsd.c
SRCS+=		host_syscall.S hostcons.c hostdisk.c kerneltramp.S kbootfdt.c
SRCS+=		ucmpdi2.c gfx_fb.c

CFLAGS.gfx_fb.c += -I${SRCTOP}/contrib/pnglite
CFLAGS.gfx_fb.c += -I${SRCTOP}/sys/teken

.include	"${BOOTSRC}/fdt.mk"

CFLAGS+=	-mcpu=powerpc64

# Always add MI sources
.include	"${BOOTSRC}/loader.mk"
.PATH:		${SYSDIR}/libkern

CFLAGS+=	-Wall -DAIM
# load address. set in linker script
RELOC?=		0x0
CFLAGS+=	-DRELOC=${RELOC}

LDFLAGS=	-nostdlib -static -T ${.CURDIR}/ldscript.powerpc

DPADD=		${LDR_INTERP} ${LIBOFW} ${LIBFDT} ${LIBSA}
LDADD=		${LDR_INTERP} ${LIBOFW} ${LIBFDT} ${LIBSA}

.include <bsd.prog.mk>
