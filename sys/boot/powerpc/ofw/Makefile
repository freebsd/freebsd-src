# $FreeBSD$

BASE=		loader
PROG=		${BASE}
NOMAN=
STRIP=
NEWVERSWHAT=	"bootstrap loader" OpenFirmware/PowerPC
BINDIR?=	/boot
INSTALLFLAGS=	-b

# architecture-specific loader code
SRCS=		conf.c metadata.c vers.c start.c

LOADER_DISK_SUPPORT?=	yes
LOADER_UFS_SUPPORT?=	yes
LOADER_CD9660_SUPPORT?=	yes
LOADER_EXT2FS_SUPPORT?=	no
LOADER_NET_SUPPORT?=	yes
LOADER_NFS_SUPPORT?=	yes
LOADER_TFTP_SUPPORT?=	yes
LOADER_GZIP_SUPPORT?=	yes
LOADER_BZIP2_SUPPORT?=	no

.if defined(LOADER_DISK_SUPPORT)
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
.if defined(LOADER_NET_SUPPORT)
CFLAGS+=	-DLOADER_NET_SUPPORT
.endif
.if ${LOADER_NFS_SUPPORT} == "yes"
CFLAGS+=	-DLOADER_NFS_SUPPORT
.endif
.if ${LOADER_TFTP_SUPPORT} == "yes"
CFLAGS+=	-DLOADER_TFTP_SUPPORT
.endif
.if ${LOADER_GZIP_SUPPORT} == "yes"
CFLAGS+=	-DLOADER_GZIP_SUPPORT
.endif
.if ${LOADER_BZIP2_SUPPORT} == "yes"
CFLAGS+=	-DLOADER_BZIP2_SUPPORT
.endif

.if !defined(NOFORTH)
# Enable BootForth
BOOT_FORTH=	yes
CFLAGS+=	-DBOOT_FORTH -I${.CURDIR}/../../ficl -I${.CURDIR}/../../ficl/powerpc
.if exists(${.OBJDIR}/../../ficl/libficl.a)
LIBFICL=	${.OBJDIR}/../../ficl/libficl.a
.else
LIBFICL=	${.CURDIR}/../../ficl/libficl.a
.endif
.endif

# Always add MI sources 
.PATH:		${.CURDIR}/../../common
.include	<${.CURDIR}/../../common/Makefile.inc>
CFLAGS+=	-I${.CURDIR}/../../common
CFLAGS+=	-I${.CURDIR}/../../.. -I.

CLEANFILES+=	vers.c ${BASE}.help

CFLAGS+=	-ffreestanding
# load address
RELOC?=		0x1C00000
CFLAGS+=	-DRELOC=${RELOC}
LDFLAGS=	-nostdlib -static -Ttext ${RELOC}

# Pull in common loader code
.PATH:		${.CURDIR}/../../ofw/common
.include	<${.CURDIR}/../../ofw/common/Makefile.inc>

# OpenFirmware standalone support library
LIBOFW=		${.OBJDIR}/../../ofw/libofw/libofw.a
CFLAGS+=	-I${.CURDIR}/../../ofw/libofw

# where to get libstand from
#XXX need a better way to do this
LIBSTAND=	${.CURDIR}/../../../../lib/libstand/libstand.a
.if !exists(${LIBSTAND})
LIBSTAND=	${.OBJDIR}/../../../../lib/libstand/libstand.a
.if !exists(${LIBSTAND})
LIBSTAND=	-lstand
.endif
.endif
CFLAGS+=	-I${.CURDIR}/../../../../lib/libstand/

LDADD=		${LIBFICL} ${LIBOFW} ${LIBSTAND}

# Debug me!
CFLAGS+=	-g
LDFLAGS+=	-g

vers.c:	${.CURDIR}/../../common/newvers.sh ${.CURDIR}/version
	sh ${.CURDIR}/../../common/newvers.sh ${.CURDIR}/version ${NEWVERSWHAT}

${BASE}.help: help.common help.ofw
	cat ${.ALLSRC} | \
	    awk -f ${.CURDIR}/../../common/merge_help.awk > ${.TARGET}

.PATH: ${.CURDIR}/../../forth
FILES=  ${BASE}.help loader.4th support.4th loader.conf
FILESDIR_loader.conf=	/boot/defaults

.if !exists(${DESTDIR}/boot/loader.rc)
FILES+= loader.rc
.endif

# Cannot use ${OBJS} above this line
.include <bsd.prog.mk>

.if exists(${.CURDIR}/../../../powerpc/include)
beforedepend ${OBJS}: machine

machine:
	ln -sf ${.CURDIR}/../../../powerpc/include machine
.endif

CLEANFILES+=	machine
