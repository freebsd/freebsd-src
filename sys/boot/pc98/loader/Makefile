# $FreeBSD$

.include <bsd.own.mk>
MK_SSP=		no

PROG=		loader.sym
INTERNALPROG=
NEWVERSWHAT=	"bootstrap loader" pc98

# architecture-specific loader code
SRCS=		main.c conf.c vers.c
.PATH:		${.CURDIR}/../../i386/loader

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

# Always add MI sources 
.PATH:		${.CURDIR}/../../common
.include	"${.CURDIR}/../../common/Makefile.inc"
CFLAGS+=	-I${.CURDIR}/../../common
CFLAGS+=	-I${.CURDIR}/../../i386
CFLAGS+=	-I.

CLEANFILES=	vers.c loader loader.bin loader.help

CFLAGS+=	-Wall
LDFLAGS=	-static -Ttext 0x0

# pc98 standalone support library
LIBPC98=	${.OBJDIR}/../libpc98/libpc98.a
CFLAGS+=	-I${.CURDIR}/..

# BTX components
CFLAGS+=	-I${.CURDIR}/../btx/lib

# Debug me!
#CFLAGS+=	-g
#LDFLAGS+=	-g

# Pick up ../Makefile.inc early.
.include <bsd.init.mk>

vers.c:	${.CURDIR}/../../common/newvers.sh ${.CURDIR}/../../i386/loader/version
	sh ${.CURDIR}/../../common/newvers.sh ${.CURDIR}/../../i386/loader/version ${NEWVERSWHAT}

loader: loader.bin ${BTXLDR} ${BTXKERN}
	btxld -v -f aout -e ${LOADER_ADDRESS} -o ${.TARGET} -l ${BTXLDR} \
		-b ${BTXKERN} loader.bin

loader.bin: loader.sym
	cp ${.ALLSRC} ${.TARGET}
	strip -R .comment -R .note ${.TARGET}

loader.help: help.common help.pc98
	cat ${.ALLSRC} | awk -f ${.CURDIR}/../../common/merge_help.awk > ${.TARGET}

.PATH: ${.CURDIR}/../../forth 
FILES=	loader loader.help loader.4th support.4th loader.conf
FILES+= screen.4th frames.4th beastie.4th
FILES+= brand.4th check-password.4th color.4th delay.4th
FILES+= menu.4th menu-commands.4th shortcuts.4th version.4th
# XXX INSTALLFLAGS_loader= -b
FILESMODE_loader= ${BINMODE} -b
FILESDIR_loader.conf=	/boot/defaults

.if !exists(${DESTDIR}/boot/loader.rc)
FILES+=	${.CURDIR}/../../i386/loader/loader.rc
.endif

.if !exists(${DESTDIR}/boot/menu.rc)
FILES+= menu.rc
.endif

# XXX crt0.o needs to be first for pxeboot(8) to work
OBJS=	${BTXCRT} 

DPADD=	${LIBFICL} ${LIBPC98} ${LIBSTAND}
LDADD=	${LIBFICL} ${LIBPC98} ${LIBSTAND}

.include <bsd.prog.mk>
