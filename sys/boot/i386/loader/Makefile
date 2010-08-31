# $FreeBSD$

.include <bsd.own.mk>
MK_SSP=		no

LOADER?=	loader
PROG=		${LOADER}.sym
INTERNALPROG=
NEWVERSWHAT?=	"bootstrap loader" x86

# architecture-specific loader code
SRCS=		main.c conf.c vers.c

# Put LOADER_FIREWIRE_SUPPORT=yes in /etc/make.conf for FireWire/dcons support
.if defined(LOADER_FIREWIRE_SUPPORT)
CFLAGS+=	-DLOADER_FIREWIRE_SUPPORT
LIBFIREWIRE=	${.OBJDIR}/../libfirewire/libfirewire.a
.endif

# Set by zfsloader Makefile
.if defined(LOADER_ZFS_SUPPORT)
CFLAGS+=	-DLOADER_ZFS_SUPPORT
LIBZFS=		${.OBJDIR}/../../zfs/libzfsboot.a
.else
LIBZFS=
.endif

# Enable PXE TFTP or NFS support, not both.
.if defined(LOADER_TFTP_SUPPORT)
CFLAGS+=	-DLOADER_TFTP_SUPPORT
.else
CFLAGS+=	-DLOADER_NFS_SUPPORT
.endif

# Include bcache code.
HAVE_BCACHE=	yes

# Enable PnP and ISA-PnP code.
HAVE_PNP=	yes
HAVE_ISABUS=	yes

.if ${MK_FORTH} != "no"
# Enable BootForth
BOOT_FORTH=	yes
CFLAGS+=	-DBOOT_FORTH -I${.CURDIR}/../../ficl -I${.CURDIR}/../../ficl/i386
LIBFICL=	${.OBJDIR}/../../ficl/libficl.a
.endif

.if defined(LOADER_BZIP2_SUPPORT)
CFLAGS+=	-DLOADER_BZIP2_SUPPORT
.endif
.if !defined(LOADER_NO_GZIP_SUPPORT)
CFLAGS+=	-DLOADER_GZIP_SUPPORT
.endif
.if !defined(LOADER_NO_GPT_SUPPORT)
CFLAGS+=	-DLOADER_GPT_SUPPORT
.endif

# Always add MI sources 
.PATH:		${.CURDIR}/../../common
.include	"${.CURDIR}/../../common/Makefile.inc"
CFLAGS+=	-I${.CURDIR}/../../common
CFLAGS+=	-I.

CLEANFILES=	vers.c ${LOADER} ${LOADER}.bin loader.help

CFLAGS+=	-Wall
LDFLAGS=	-static -Ttext 0x0

# i386 standalone support library
LIBI386=	${.OBJDIR}/../libi386/libi386.a
CFLAGS+=	-I${.CURDIR}/..

# BTX components
CFLAGS+=	-I${.CURDIR}/../btx/lib

# Debug me!
#CFLAGS+=	-g
#LDFLAGS+=	-g

# Pick up ../Makefile.inc early.
.include <bsd.init.mk>

vers.c:	${.CURDIR}/../../common/newvers.sh ${.CURDIR}/../loader/version
	sh ${.CURDIR}/../../common/newvers.sh ${.CURDIR}/../loader/version \
		${NEWVERSWHAT}

${LOADER}: ${LOADER}.bin ${BTXLDR} ${BTXKERN}
	btxld -v -f aout -e ${LOADER_ADDRESS} -o ${.TARGET} -l ${BTXLDR} \
		-b ${BTXKERN} ${LOADER}.bin

${LOADER}.bin: ${LOADER}.sym
	cp ${.ALLSRC} ${.TARGET}
	strip -R .comment -R .note ${.TARGET}

loader.help: help.common help.i386
	cat ${.ALLSRC} | awk -f ${.CURDIR}/../../common/merge_help.awk > ${.TARGET}

FILES=	${LOADER}
# XXX INSTALLFLAGS_loader= -b
FILESMODE_${LOADER}= ${BINMODE} -b

.if !defined(LOADER_ONLY)
.PATH: ${.CURDIR}/../../forth 
FILES+=	loader.help loader.4th support.4th loader.conf
FILES+= screen.4th frames.4th beastie.4th
FILESDIR_loader.conf=	/boot/defaults

.if !exists(${DESTDIR}/boot/loader.rc)
FILES+=	loader.rc
.endif
.endif

# XXX crt0.o needs to be first for pxeboot(8) to work
OBJS=	${BTXCRT} 

DPADD=	${LIBFICL} ${LIBFIREWIRE} ${LIBZFS} ${LIBI386} ${LIBSTAND}
LDADD=	${LIBFICL} ${LIBFIREWIRE} ${LIBZFS} ${LIBI386} ${LIBSTAND}

.include <bsd.prog.mk>

.if ${MACHINE_CPUARCH} == "amd64"
beforedepend ${OBJS}: machine
CLEANFILES+=	machine
machine:
	ln -sf ${.CURDIR}/../../../i386/include machine
.endif
