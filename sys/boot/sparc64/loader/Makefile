# $FreeBSD$

.include <bsd.init.mk>
MK_SSP=		no
MAN=

PROG?=		loader
NEWVERSWHAT?=	"bootstrap loader" sparc64
VERSION_FILE=	${.CURDIR}/../loader/version
INSTALLFLAGS=	-b

# Architecture-specific loader code
SRCS=		locore.S main.c metadata.c vers.c

LOADER_DISK_SUPPORT?=	yes
LOADER_UFS_SUPPORT?=	yes
LOADER_CD9660_SUPPORT?=	yes
LOADER_EXT2FS_SUPPORT?=	no
LOADER_MSDOS_SUPPORT?=	no
LOADER_ZFS_SUPPORT?=	no
LOADER_NET_SUPPORT?=	yes
LOADER_NFS_SUPPORT?=	yes
LOADER_TFTP_SUPPORT?=	yes
LOADER_GZIP_SUPPORT?=	yes
LOADER_BZIP2_SUPPORT?=	no
LOADER_DEBUG?=		no

.if ${LOADER_DEBUG} == "yes"
CFLAGS+=	-DLOADER_DEBUG
.endif
.if ${LOADER_ZFS_SUPPORT} == "yes"
CFLAGS+=	-DLOADER_ZFS_SUPPORT
CFLAGS+=	-I${BOOTSRC}/zfs
CFLAGS+=	-I${SYSDIR}/cddl/boot/zfs
LIBZFSBOOT=	${BOOTOBJ}/zfs/libzfsboot.a
.endif

# Always add MI sources
.include	"${BOOTSRC}/loader.mk"
CFLAGS+=	-I.

CLEANFILES+=	loader.help

LDFLAGS+=	-static

# Open Firmware standalone support library
LIBOFW=		${BOOTOBJ}/ofw/libofw/libofw.a
CFLAGS+=	-I${BOOTSRC}/ofw/libofw/

# Need sys/ for crypto/intake.h
CFLAGS+=	-I${SRCTOP}/sys

DPADD=		${LIBFICL} ${LIBZFSBOOT} ${LIBOFW} ${LIBSA}
LDADD=		${LIBFICL} ${LIBZFSBOOT} ${LIBOFW} ${LIBSA}

loader.help: help.common help.sparc64
	cat ${.ALLSRC} | \
	    awk -f ${LDRSRC}/merge_help.awk > ${.TARGET}

.PATH: ${BOOTSRC}/forth
.include	"${BOOTSRC}/forth/Makefile.inc"

FILES+= loader.rc menu.rc

.include <bsd.prog.mk>
