# $FreeBSD$

BASE=		loader
PROG=		${BASE}
NOMAN=
STRIP=
NEWVERSWHAT=	"bootstrap loader" OpenFirmware/PowerPC
BINDIR?=	/boot

LOADER_DISK_SUPPORT?=	yes
LOADER_NET_SUPPORT?=	yes

# architecture-specific loader code
SRCS=		conf.c

# Pull in common loader code
.PATH:		${.CURDIR}/../../ofw/common
.include	<${.CURDIR}/../../ofw/common/Makefile.inc>

.if defined(LOADER_DISK_SUPPORT)
CFLAGS+=	-DLOADER_DISK_SUPPORT
.endif
.if defined(LOADER_NET_SUPPORT)
CFLAGS+=	-DLOADER_NET_SUPPORT
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

CLEANFILES+=	vers.c vers.o ${BASE}.list ${BASE}.bin ${BASE}.sym ${BASE}.help

CFLAGS+=	-Wall
LDFLAGS=	-nostdlib -static -Ttext 6c0000

# OpenFirmware standalone support library
LIBOFW=		${.OBJDIR}/../../ofw/libofw/libofw.a
CFLAGS+=	-I${.CURDIR}/../../ofw/libofw

# where to get libstand from
#XXX need a better way to do this
LIBSTAND=	${.CURDIR}/../../../../lib/libstand/libstand.a
#.if !exists(${LIBSTAND})
#LIBSTAND=	${.OBJDIR}/../../../../lib/libstand/libstand.a
#.if !exists(${LIBSTAND})
#LIBSTAND=	-lstand
#.endif
#.endif
CFLAGS+=	-I${.CURDIR}/../../../../lib/libstand/

# OpenFirmware is expecting ELF components
CFLAGS+=	-elf

# New linker set code
#CFLAGS+=	-DNEW_LINKER_SET

# Debug me!
CFLAGS+=	-g
LDFLAGS+=	-g

vers.o:	${.CURDIR}/../../common/newvers.sh ${.CURDIR}/version
	sh ${.CURDIR}/../../common/newvers.sh ${.CURDIR}/version ${NEWVERSWHAT}
	${CC} -c vers.c

${BASE}.help: help.common help.ofw
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

${PROG}: ${OBJS} ${LIBOFW} ${LIBSTAND} ${LIBFICL} start.o vers.o
	${LD} ${LDFLAGS} -o ${.TARGET} start.o ${OBJS} \
		vers.o ${LIBFICL} ${LIBOFW} ${LIBSTAND}

machine:
	ln -sf ${.CURDIR}/../../../powerpc/include machine

# Cannot use ${OBJS} above this line
.include <bsd.prog.mk>

beforedepend ${OBJS}: machine

CLEANFILES+=	machine setdefs.h setdef0.c setdef1.c setdef0.o setdef1.o \
		start.o
