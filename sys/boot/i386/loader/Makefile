# $FreeBSD$

LOADER_NET_SUPPORT?=	yes
LOADER_NFS_SUPPORT?=	yes
LOADER_TFTP_SUPPORT?=	yes
LOADER_CD9660_SUPPORT?=	no
LOADER_EXT2FS_SUPPORT?=	no
LOADER_MSDOS_SUPPORT?=	no
LOADER_UFS_SUPPORT?=	yes
LOADER_GZIP_SUPPORT?=	yes
LOADER_BZIP2_SUPPORT?=	yes

.include <bsd.init.mk>

MK_SSP=		no

LOADER?=	loader
PROG=		${LOADER}.sym
MAN=	
INTERNALPROG=
NEWVERSWHAT?=	"bootstrap loader" x86
VERSION_FILE=	${.CURDIR}/../loader/version

.PATH:		${BOOTSRC}/i386/loader

# architecture-specific loader code
SRCS=		main.c conf.c vers.c chain.c

# Include bcache code.
HAVE_BCACHE=	yes

# Enable PnP and ISA-PnP code.
HAVE_PNP=	yes
HAVE_ISABUS=	yes

.if ${MK_LOADER_FIREWIRE} == "yes"
CFLAGS+=	-DLOADER_FIREWIRE_SUPPORT
LIBFIREWIRE=	${BOOTOBJ}/i386/libfirewire/libfirewire.a
.endif

.if ${LOADER_GELI_SUPPORT:Uyes} == "yes"
CFLAGS+=	-DLOADER_GELI_SUPPORT
CFLAGS+=	-I${BOOTSRC}/geli
LIBGELIBOOT=	${BOOTOBJ}/geli/libgeliboot.a
.PATH:		${SYSDIR}/opencrypto
SRCS+=		xform_aes_xts.c
CFLAGS+=	-I${SYSDIR} -D_STAND
.endif

# Always add MI sources
.include	"${BOOTSRC}/loader.mk"
CFLAGS+=	-I.

CLEANFILES=	${LOADER} ${LOADER}.bin loader.help

CFLAGS+=	-Wall
LDFLAGS+=	-static -Ttext 0x0

# i386 standalone support library
LIBI386=	${BOOTOBJ}/i386/libi386/libi386.a
CFLAGS+=	-I${BOOTSRC}/i386

# BTX components
CFLAGS+=	-I${BTXLIB}

# Debug me!
#CFLAGS+=	-g
#LDFLAGS+=	-g

${LOADER}: ${LOADER}.bin ${BTXLDR} ${BTXKERN}
	btxld -v -f aout -e ${LOADER_ADDRESS} -o ${.TARGET} -l ${BTXLDR} \
		-b ${BTXKERN} ${LOADER}.bin

${LOADER}.bin: ${LOADER}.sym
	strip -R .comment -R .note -o ${.TARGET} ${.ALLSRC}

loader.help: help.common help.i386
	cat ${.ALLSRC} | awk -f ${LDRSRC}/merge_help.awk > ${.TARGET}

FILES=	${LOADER}
# XXX INSTALLFLAGS_loader= -b
FILESMODE_${LOADER}= ${BINMODE} -b

.if !defined(LOADER_ONLY)
.PATH: ${BOOTSRC}/forth
.include	"${BOOTSRC}/forth/Makefile.inc"
FILES+=	pcibios.4th

FILES+=	loader.rc menu.rc
.endif

# XXX crt0.o needs to be first for pxeboot(8) to work
OBJS=	${BTXCRT}

DPADD=	${LIBFICL32} ${LIBFIREWIRE} ${LIBZFSBOOT} ${LIBI386} ${LIBGELIBOOT} ${LIBSA32}
LDADD=	${LIBFICL32} ${LIBFIREWIRE} ${LIBZFSBOOT} ${LIBI386} ${LIBGELIBOOT} ${LIBSA32}

.if ${MACHINE_CPUARCH} == "amd64"
CFLAGS+=	-DLOADER_PREFER_AMD64
.endif

.include <bsd.prog.mk>
