# $Id: Makefile,v 1.32 1999/04/18 09:21:12 dcs Exp $

BASE=		loader
PROG=		${BASE}
MAN5=		loader.conf.5
MAN8=		loader.8 ../../forth/loader.4th.8
#NOMAN=
STRIP=
NEWVERSWHAT=	"bootstrap loader"
BINDIR?=	/boot

# architecture-specific loader code
SRCS=		main.c conf.c

# Enable PnP and ISA-PnP code.
HAVE_PNP=	yes
HAVE_ISABUS=	yes

# Enable BootForth
BOOT_FORTH=	yes
CFLAGS+=	-DBOOT_FORTH -I${.CURDIR}/../../ficl
.if exists(${.OBJDIR}/../../ficl/libficl.a)
LIBFICL=	${.OBJDIR}/../../ficl/libficl.a
.else
LIBFICL=	${.CURDIR}/../../ficl/libficl.a
.endif

# Always add MI sources 
.PATH:		${.CURDIR}/../../common
.include	<${.CURDIR}/../../common/Makefile.inc>
CFLAGS+=	-I${.CURDIR}/../../common
CFLAGS+=	-I${.CURDIR}/../../.. -I.

CLEANFILES+=	vers.c vers.o ${BASE}.list ${BASE}.bin ${BASE}.sym ${BASE}.help

CFLAGS+=	-Wall
LDFLAGS=	-nostdlib -static -Ttext 0x0

# i386 standalone support library
LIBI386=	${.OBJDIR}/../libi386/libi386.a
CFLAGS+=	-I${.CURDIR}/..

# where to get libstand from
LIBSTAND=	-lstand
#LIBSTAND=	${.CURDIR}/../../../lib/libstand/libstand.a
#CFLAGS+=	-I${.CURDIR}/../../../lib/libstand/

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

# New linker set code
CFLAGS+=	-DNEW_LINKER_SET

# Debug me!
#CFLAGS+=	-g
#LDFLAGS+=	-g

vers.o:
	sh ${.CURDIR}/newvers.sh ${.CURDIR}/version ${NEWVERSWHAT}
	${CC} -c vers.c

${BASE}: ${BASE}.bin ${BTXLDR} ${BTXKERN} ${BTXCRT} ${BASE}.help
	btxld -v -f aout -e 0x100000 -o ${.TARGET} -l ${BTXLDR} -b ${BTXKERN} \
		${BASE}.bin
#	/usr/bin/kzip ${.TARGET}
#	mv ${.TARGET}.kz ${.TARGET}

${BASE}.bin: ${BASE}.sym
	cp ${.ALLSRC} ${.TARGET}
	strip ${.TARGET}

${BASE}.help: help.common help.i386
	cat ${.ALLSRC} | awk -f ${.CURDIR}/../../common/merge_help.awk > ${.TARGET}

beforeinstall:
.if exists(${DESTDIR}/boot/loader)
	mv ${DESTDIR}/boot/loader ${DESTDIR}/boot/loader.old
.endif
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

# Cannot use ${OBJS} above this line
.include <bsd.prog.mk>

${BASE}.sym: ${OBJS} ${LIBI386} ${LIBSTAND} ${LIBFICL} vers.o
	${CC} ${LDFLAGS} -o ${.TARGET} ${BTXCRT} ${OBJS} vers.o \
		${LIBFICL} ${LIBSTAND} ${LIBI386} ${LIBSTAND}

# If it's not there, don't consider it a target
.if exists(${.CURDIR}/../../../i386/include)
beforedepend ${OBJS}: machine

machine:
	ln -sf ${.CURDIR}/../../../i386/include machine

.endif

CLEANFILES+=	machine


