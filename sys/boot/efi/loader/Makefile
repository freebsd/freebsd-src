# $FreeBSD$

.PATH: ${.CURDIR}/../common

BASE= 		loader
PROG= 		${BASE}.efi
NOMAN=
NEWVERSWHAT=	"EFI boot" ${MACHINE_ARCH}

SRCS+=		main.c conf.c dev_net.c

# Enable BootForth
BOOT_FORTH=	yes
CFLAGS+=	-I${.CURDIR}/../../ficl -I${.CURDIR}/../../ficl/${MACHINE_ARCH}
.if BOOT_FORTH
CFLAGS+=	-DBOOT_FORTH
.if exists(${.OBJDIR}/../../ficl/libficl.a)
LIBFICL=	${.OBJDIR}/../../ficl/libficl.a
.else
LIBFICL=	${.CURDIR}/../../ficl/libficl.a
.endif
.else
LIBFICL=
.endif

# where to get libstand from
.if exists(${.OBJDIR}/../../../../lib/libstand/libstand.a)
LIBSTAND=	${.OBJDIR}/../../../../lib/libstand/libstand.a
.else
LIBSTAND=	${.CURDIR}/../../../../lib/libstand/libstand.a
.endif

.if exists(${.OBJDIR}/../libefi/libefi.a)
LIBEFI=		${.OBJDIR}/../libefi/libefi.a
.else
LIBEFI=		${.CURDIR}/../libefi/libefi.a
.endif

# Always add MI sources 
.PATH: ${.CURDIR}/../../common
.include <${.CURDIR}/../../common/Makefile.inc>

CFLAGS+= -I-
CFLAGS+= -I${.CURDIR}/../include
CFLAGS+= -I${.CURDIR}/../include/${MACHINE_ARCH}
CFLAGS+= -I${.CURDIR}/../../common -I${.CURDIR}
CFLAGS+= -I${.CURDIR}/../../.. -I.
CFLAGS+= -I${.CURDIR}/../libefi
CFLAGS+= -I${.CURDIR}/../../../../lib/libstand
CFLAGS+= -DLOADER

LDSCRIPT=	${.CURDIR}/../libefi/arch/${MACHINE_ARCH}/ldscript.${MACHINE_ARCH}
LDFLAGS=	-nostdlib -T ${LDSCRIPT} -shared -Bsymbolic
OBJCOPY?=	objcopy

CLEANFILES+=	setdef0.c setdef0.o setdef1.c setdef1.o setdefs.h start.o \
		vers.c vers.o ${BASE}.efi ${BASE}.sym ${BASE}.list
CLEANFILES+=	loader.help
CLEANFILES+=	machine

CRT=	start.o

all: ${BASE}

vers.o: ${.CURDIR}/../../common/newvers.sh ${.CURDIR}/version
	sh ${.CURDIR}/../../common/newvers.sh ${.CURDIR}/version ${NEWVERSWHAT}
	${CC} -c vers.c

${BASE}:	${BASE}.efi ${BASE}.help

${BASE}.efi: ${BASE}.sym
	${OBJCOPY} -j .text \
		-j .hash \
		-j .data \
		-j .sdata \
		-j .dynamic \
		-j .rela \
		-j .reloc \
		-j .dynsym \
		-j .dynstr \
		--target=efi-app-${MACHINE_ARCH} \
		${BASE}.sym ${BASE}.efi

${BASE}.help:	help.common
	cat ${.ALLSRC} | awk -f ${.CURDIR}/../../common/merge_help.awk \
	    > ${.TARGET}

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


# Other fragments still to be brought in from ../Makfile.booters?
start.o: ${.CURDIR}/../libefi/arch/${MACHINE_ARCH}/start.S
	${CC} -c ${CFLAGS} ${.IMPSRC}

machine:
	ln -sf ${.CURDIR}/../../../${MACHINE_ARCH}/include machine

.include <bsd.prog.mk>

${BASE}.sym: ${OBJS} ${LIBFICL} ${LIBEFI} ${LIBSTAND} ${CRT} vers.o
	${LD} ${LDFLAGS} -o ${BASE}.sym -M ${CRT} ${OBJS} vers.o \
	    ${LIBFICL} ${LIBEFI} ${LIBSTAND} > ${.OBJDIR}/${BASE}.list

beforedepend ${OBJS}: machine
