# $FreeBSD$

.include <src.opts.mk>
MK_SSP=		no
MAN=

PROG?=		loader
NEWVERSWHAT?=	"bootstrap loader" sparc64
INSTALLFLAGS=	-b

# Architecture-specific loader code
SRCS=		locore.S main.c metadata.c vers.c

LOADER_DISK_SUPPORT?=	yes
LOADER_UFS_SUPPORT?=	yes
LOADER_CD9660_SUPPORT?=	yes
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
.if ${LOADER_DISK_SUPPORT} == "yes"
CFLAGS+=	-DLOADER_DISK_SUPPORT
.endif
.if ${LOADER_UFS_SUPPORT} == "yes"
CFLAGS+=	-DLOADER_UFS_SUPPORT
.endif
.if ${LOADER_CD9660_SUPPORT} == "yes"
CFLAGS+=	-DLOADER_CD9660_SUPPORT
.endif
.if ${LOADER_ZFS_SUPPORT} == "yes"
CFLAGS+=	-DLOADER_ZFS_SUPPORT
CFLAGS+=	-I${.CURDIR}/../../zfs
CFLAGS+=	-I${.CURDIR}/../../../cddl/boot/zfs
LIBZFSBOOT=	${.OBJDIR}/../../zfs/libzfsboot.a
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

.if ${MK_FORTH} != "no"
# Enable BootForth
BOOT_FORTH=	yes
CFLAGS+=	-DBOOT_FORTH -I${.CURDIR}/../../ficl
CFLAGS+=	-I${.CURDIR}/../../ficl/sparc64
LIBFICL=	${.OBJDIR}/../../ficl/libficl.a
.endif

# Always add MI sources
.PATH:		${.CURDIR}/../../common
.include	"${.CURDIR}/../../common/Makefile.inc"
CFLAGS+=	-I${.CURDIR}/../../common
CFLAGS+=	-I.

CLEANFILES+=	vers.c loader.help

LDFLAGS=	-static

# Open Firmware standalone support library
LIBOFW=		${.OBJDIR}/../../ofw/libofw/libofw.a
CFLAGS+=	-I${.CURDIR}/../../ofw/libofw/

# where to get libstand from
CFLAGS+=	-I${.CURDIR}/../../../../lib/libstand/

DPADD=		${LIBFICL} ${LIBZFSBOOT} ${LIBOFW} ${LIBSTAND}
LDADD=		${LIBFICL} ${LIBZFSBOOT} ${LIBOFW} -lstand

vers.c:	${.CURDIR}/../../common/newvers.sh ${.CURDIR}/../loader/version
	sh ${.CURDIR}/../../common/newvers.sh ${.CURDIR}/../loader/version \
	    ${NEWVERSWHAT}

loader.help: help.common help.sparc64
	cat ${.ALLSRC} | \
	    awk -f ${.CURDIR}/../../common/merge_help.awk > ${.TARGET}

.PATH: ${.CURDIR}/../../forth
.include	"${.CURDIR}/../../forth/Makefile.inc"

FILES+= loader.rc menu.rc

.include <bsd.prog.mk>
