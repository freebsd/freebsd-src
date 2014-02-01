# $FreeBSD$

.include <bsd.own.mk>
MK_SSP=		no

PROG?=		loader
NEWVERSWHAT=	"BERI loader" ${MACHINE_CPUARCH}
INSTALLFLAGS=	-b

# Architecture-specific loader code
SRCS=		start.S main.c exec.c metadata.c vers.c arch.c
SRCS+=		beri_console.c altera_jtag_uart.c beri_disk.c cfi.c

LOADER_DISK_SUPPORT?=	yes
LOADER_UFS_SUPPORT?=	yes
#LOADER_CD9660_SUPPORT?=	yes
#LOADER_ZFS_SUPPORT?=	yes
#LOADER_NET_SUPPORT?=	yes
#LOADER_NFS_SUPPORT?=	yes
#LOADER_TFTP_SUPPORT?=	yes
LOADER_GZIP_SUPPORT?=	yes
LOADER_BZIP2_SUPPORT?=	yes
#LOADER_DEBUG?=		yes

.if defined(${LOADER_DEBUG})
CFLAGS+=	-DLOADER_DEBUG
.endif
.if defined(${LOADER_DISK_SUPPORT})
CFLAGS+=	-DLOADER_DISK_SUPPORT
.endif
.if defined(${LOADER_UFS_SUPPORT})
CFLAGS+=	-DLOADER_UFS_SUPPORT
.endif
.if defined(${LOADER_CD9660_SUPPORT})
CFLAGS+=	-DLOADER_CD9660_SUPPORT
.endif
.if defined(${LOADER_ZFS_SUPPORT})
CFLAGS+=	-DLOADER_ZFS_SUPPORT
CFLAGS+=	-I${.CURDIR}/../../../zfs
CFLAGS+=	-I${.CURDIR}/../../../../cddl/boot/zfs
LIBZFSBOOT=	${.OBJDIR}/../../../zfs/libzfsboot.a
.endif
.if defined(${LOADER_GZIP_SUPPORT})
CFLAGS+=	-DLOADER_GZIP_SUPPORT
.endif
.if defined(${LOADER_BZIP2_SUPPORT})
CFLAGS+=	-DLOADER_BZIP2_SUPPORT
.endif
.if defined(${LOADER_NET_SUPPORT})
CFLAGS+=	-DLOADER_NET_SUPPORT
.endif
.if defined(${LOADER_NFS_SUPPORT})
CFLAGS+=	-DLOADER_NFS_SUPPORT
.endif
.if defined(${LOADER_TFTP_SUPPORT})
CFLAGS+=	-DLOADER_TFTP_SUPPORT
.endif

.if ${MK_FORTH} != "no"
# Enable BootForth
BOOT_FORTH=	yes
CFLAGS+=	-DBOOT_FORTH -I${.CURDIR}/../../../ficl
CFLAGS+=	-I${.CURDIR}/../../../ficl/mips
LIBFICL=	${.OBJDIR}/../../../ficl/libficl.a
.endif

# Common code across BERI boot loader parts
.PATH:		${.CURDIR}/../common
CFLAGS+=	-I${.CURDIR}/../common

# Always add MI sources
.PATH:		${.CURDIR}/../../../common
.include	"${.CURDIR}/../../../common/Makefile.inc"
CFLAGS+=	-I${.CURDIR}/../../../common

# BERI files common to boot2 and loader
.PATH:		${.CURDIR}/../common
CFLAGS+=	-I${.CURDIR}/../common

# Loader-specific MD headers
CFLAGS+=	-I${.CURDIR}

CLEANFILES+=	vers.c loader.help

# Generate code appropriate for the loader environment
CFLAGS+=	-G0					\
		-fno-pic				\
		-mno-abicalls				\
		-g

LDFLAGS=	-nostdlib				\
		-static					\
		-T ${.CURDIR}/loader.ldscript		\
		-e __start

# where to get libstand from
CFLAGS+=	-I${.CURDIR}/../../../../../lib/libstand/
LIBSTAND=	${.OBJDIR}/../../../../../lib/libstand/libstand.a

DPADD=		${LIBFICL} ${LIBSTAND}
LDADD=		${LIBFICL} ${LIBSTAND}

vers.c: ${.CURDIR}/../../../common/newvers.sh ${.CURDIR}/version
	sh ${.CURDIR}/../../../common/newvers.sh ${.CURDIR}/version	\
	    ${NEWVERSWHAT}

loader.help: help.common help.mips
	cat ${.ALLSRC} | \
	    awk -f ${.CURDIR}/../../../common/merge_help.awk > ${.TARGET}

.PATH: ${.CURDIR}/../../../forth
FILES=	loader.help loader.4th support.4th loader.conf
FILES+=	screen.4th frames.4th
FILES+=	beastie.4th brand.4th check-password.4th color.4th delay.4th
FILES+=	menu.4th menu-commands.4th menusets.4th shortcuts.4th version.4th
FILESDIR_loader.conf=	/boot/defaults

.if !exists(${DESTDIR}/boot/loader.rc)
FILES+= loader.rc
.endif

.if !exists(${DESTDIR}/boot/menu.rc)
FILES+= menu.rc
.endif

loader.dump: loader
	objdump -dS loader > loader.dump

all: loader loader.dump

.include <bsd.prog.mk>
