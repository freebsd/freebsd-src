# $FreeBSD$

NEWVERSWHAT=	"bootstrap loader" pc98

CFLAGS+=	-DPC98

# architecture-specific loader code
SRCS=		main.c conf.c vers.c
.PATH:		${.CURDIR}/../../i386/loader

CFLAGS+=	-ffreestanding
# Enable PXE TFTP or NFS support, not both.
.if defined(LOADER_TFTP_SUPPORT)
CFLAGS+=	-DLOADER_TFTP_SUPPORT
.else
CFLAGS+=	-DLOADER_NFS_SUPPORT
.endif

# Enable PnP and ISA-PnP code.
HAVE_PNP=	yes
HAVE_ISABUS=	yes

.if !defined(NOFORTH)
# Enable BootForth
BOOT_FORTH=	yes
CFLAGS+=	-DBOOT_FORTH -I${.CURDIR}/../../ficl -I${.CURDIR}/../../ficl/i386
.if exists(${.OBJDIR}/../../ficl/libficl.a)
LIBFICL=	${.OBJDIR}/../../ficl/libficl.a
.else
LIBFICL=	${.CURDIR}/../../ficl/libficl.a
.endif
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

CLEANFILES=	vers.c loader loader.list loader.bin loader.sym loader.help

CFLAGS+=	-Wall
LDFLAGS=	-nostdlib -static -Ttext 0x0

# pc98 standalone support library
LIBPC98=	${.OBJDIR}/../libpc98/libpc98.a
CFLAGS+=	-I${.CURDIR}/..

# BTX components
CFLAGS+=	-I${.CURDIR}/../btx/lib

# BTX is expecting ELF components
CFLAGS+=	-elf

# Debug me!
#CFLAGS+=	-g
#LDFLAGS+=	-g

# Pick up ../Makefile.inc early.
.include <bsd.init.mk>

vers.c:	${.CURDIR}/../../common/newvers.sh ${.CURDIR}/../../i386/loader/version
	sh ${.CURDIR}/../../common/newvers.sh ${.CURDIR}/../../i386/loader/version ${NEWVERSWHAT}

loader: loader.bin ${BTXLDR} ${BTXKERN} ${BTXCRT}
	btxld -v -f aout -e ${LOADER_ADDRESS} -o ${.TARGET} -l ${BTXLDR} \
		-b ${BTXKERN} loader.bin
#	/usr/bin/kzip ${.TARGET}
#	mv ${.TARGET}.kz ${.TARGET}

loader.bin: loader.sym
	cp ${.ALLSRC} ${.TARGET}
	strip -R .comment -R .note ${.TARGET}

loader.help: help.common help.pc98
	cat ${.ALLSRC} | awk -f ${.CURDIR}/../../common/merge_help.awk > ${.TARGET}

.PATH: ${.CURDIR}/../../forth 
FILES=	loader loader.help loader.4th support.4th loader.conf
FILES+= screen.4th frames.4th beastie.4th
# XXX INSTALLFLAGS_loader= -b
FILESMODE_loader= ${BINMODE} -b
FILESDIR_loader.conf=	/boot/defaults

.if !exists(${DESTDIR}/boot/loader.rc)
FILES+=	${.CURDIR}/../../i386/loader/loader.rc
.endif

OBJS=	${SRCS:N*.h:R:S/$/.o/g}

loader.sym: ${OBJS} ${LIBFICL} ${LIBPC98} ${LIBSTAND}
	${CC} ${LDFLAGS} -o ${.TARGET} ${BTXCRT} ${OBJS} \
		${LIBFICL} ${LIBPC98} ${LIBSTAND}

.include <bsd.prog.mk>
