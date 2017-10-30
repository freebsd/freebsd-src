# $FreeBSD$

.include <bsd.init.mk>

MK_SSP=		no

LOADER?=	loader
PROG=		${LOADER}.sym
MAN=	
INTERNALPROG=
NEWVERSWHAT?=	"bootstrap loader" x86
VERSION_FILE=	${.CURDIR}/../loader/version
LOADER_NET_SUPPORT?=	yes
LOADER_NFS_SUPPORT?=	yes
LOADER_TFTP_SUPPORT?=	yes

LOADER_CD9660_SUPPORT?=	no
LOADER_EXT2FS_SUPPORT?=	no
LOADER_MSDOS_SUPPORT?=	no
LOADER_UFS_SUPPORT?=	yes

# architecture-specific loader code
SRCS=		main.c conf.c vers.c chain.c

# Put LOADER_FIREWIRE_SUPPORT=yes in /etc/make.conf for FireWire/dcons support
.if defined(LOADER_FIREWIRE_SUPPORT)
CFLAGS+=	-DLOADER_FIREWIRE_SUPPORT
LIBFIREWIRE=	${BOOTSRC}/i386/libfirewire/libfirewire.a
.endif

# Set by zfsloader Makefile
.if defined(LOADER_ZFS_SUPPORT)
CFLAGS+=	-DLOADER_ZFS_SUPPORT
LIBZFSBOOT=	${BOOTOBJ}/zfs/libzfsboot.a
.endif

# Include bcache code.
HAVE_BCACHE=	yes

# Enable PnP and ISA-PnP code.
HAVE_PNP=	yes
HAVE_ISABUS=	yes

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

# XXX not happy with this construct, revisit
.if ${MACHINE_CPUARCH} == "amd64"
FICL=	${LIBFICL32}
.else
FICL=	${LIBFICL}
.endif

DPADD=	${FICL} ${LIBFIREWIRE} ${LIBZFSBOOT} ${LIBI386} ${LIBGELIBOOT} ${LIBSA32}
LDADD=	${FICL} ${LIBFIREWIRE} ${LIBZFSBOOT} ${LIBI386} ${LIBGELIBOOT} ${LIBSA32}

.include <bsd.prog.mk>

.if ${MACHINE_CPUARCH} == "amd64"
beforedepend ${OBJS}: machine
CLEANFILES+=	machine
CFLAGS+=	-DLOADER_PREFER_AMD64
machine: .NOMETA
	ln -sf ${SYSDIR}/i386/include machine
.endif
