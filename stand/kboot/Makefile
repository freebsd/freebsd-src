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
SRCS=		vers.c main.c host_syscalls.c hostcons.c hostdisk.c kbootfdt.c gfx_fb_stub.c

CFLAGS.gfx_fb_stub.c += -I${SRCTOP}/contrib/pnglite -I${SRCTOP}/sys/teken

.include	"${BOOTSRC}/fdt.mk"

.PATH:	${.CURDIR}/arch/${MACHINE_ARCH}
.include "${.CURDIR}/arch/${MACHINE_ARCH}/Makefile.inc"

# Always add MI sources
.include	"${BOOTSRC}/loader.mk"
.PATH:		${SYSDIR}/libkern
CFLAGS+=	-I${.CURDIR} -I${.CURDIR}/arch/${MACHINE_ARCH}

CFLAGS+=	-Wall

DPADD=		${LDR_INTERP} ${LIBOFW} ${LIBFDT} ${LIBSA}
LDADD=		${LDR_INTERP} ${LIBOFW} ${LIBFDT} ${LIBSA}

.include <bsd.prog.mk>
