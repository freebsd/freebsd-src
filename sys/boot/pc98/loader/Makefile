# $FreeBSD$

BASE=		loader
PROG=		${BASE}
#NOMAN=
STRIP=
NEWVERSWHAT=	"bootstrap loader" pc98
BINDIR?=	/boot
INSTALLFLAGS=	-b

CFLAGS+=	-DPC98

# architecture-specific loader code
SRCS=		main.c conf.c
.PATH:		${.CURDIR}/../../i386/loader

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
.include	<${.CURDIR}/../../common/Makefile.inc>
CFLAGS+=	-I${.CURDIR}/../../common
CFLAGS+=	-I${.CURDIR}/../../.. -I. -I${.CURDIR}/../../i386

CLEANFILES+=	vers.c vers.o ${BASE}.list ${BASE}.bin ${BASE}.sym ${BASE}.help

CFLAGS+=	-Wall
LDFLAGS=	-nostdlib -static -Ttext 0x0

# pc98 standalone support library
LIBPC98=	${.OBJDIR}/../libpc98/libpc98.a
CFLAGS+=	-I${.CURDIR}/..

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

# BTX components
.if exists(${.OBJDIR}/../btx)
BTXDIR=		${.OBJDIR}/../btx
.else
BTXDIR=		${.CURDIR}/../btx
.endif
BTXLDR=		${BTXDIR}/btxldr/btxldr
BTXKERN=	${BTXDIR}/btx/btx
BTXCRT=		${BTXDIR}/lib/crt0.o
CFLAGS+=	-I${.CURDIR}/../btx/lib

# BTX is expecting ELF components
CFLAGS+=	-elf

# Debug me!
#CFLAGS+=	-g
#LDFLAGS+=	-g

vers.o:	${.CURDIR}/../../common/newvers.sh ${.CURDIR}/../../i386/loader/version
	sh ${.CURDIR}/../../common/newvers.sh ${.CURDIR}/../../i386/loader/version ${NEWVERSWHAT}
	${CC} -c vers.c

${BASE}: ${BASE}.bin ${BTXLDR} ${BTXKERN} ${BTXCRT} ${BASE}.help
	btxld -v -f aout -e ${LOADER_ADDRESS} -o ${.TARGET} -l ${BTXLDR} \
		-b ${BTXKERN} ${BASE}.bin
#	/usr/bin/kzip ${.TARGET}
#	mv ${.TARGET}.kz ${.TARGET}

${BASE}.bin: ${BASE}.sym
	cp ${.ALLSRC} ${.TARGET}
	strip -R .comment -R .note ${.TARGET}

${BASE}.help: help.common help.i386
	cat ${.ALLSRC} | awk -f ${.CURDIR}/../../common/merge_help.awk > ${.TARGET}

beforeinstall:
.if exists(${.OBJDIR}/loader.help)
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
		${.OBJDIR}/${BASE}.help ${DESTDIR}/boot
.else
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
		${.CURDIR}/${BASE}.help ${DESTDIR}/boot
.endif
.if !exists(${DESTDIR}/boot/loader.rc)
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
		${.CURDIR}/../../forth/loader.rc ${DESTDIR}/boot
.endif
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
		${.CURDIR}/../../forth/loader.4th ${DESTDIR}/boot
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
		${.CURDIR}/../../forth/support.4th ${DESTDIR}/boot
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
		${.CURDIR}/../../forth/loader.conf ${DESTDIR}/boot/defaults

.include <${.CURDIR}/../Makefile.inc>

# Cannot use ${OBJS} above this line
.include <bsd.prog.mk>

${BASE}.sym: ${OBJS} ${LIBPC98} ${LIBSTAND} ${LIBFICL} vers.o
	${CC} ${LDFLAGS} -o ${.TARGET} ${BTXCRT} ${OBJS} vers.o \
		${LIBFICL} ${LIBPC98} ${LIBSTAND}

# If it's not there, don't consider it a target
.if exists(${.CURDIR}/../../../i386/include)
beforedepend ${OBJS}: machine

machine:
	ln -sf ${.CURDIR}/../../../i386/include machine

.endif

CLEANFILES+=	machine
