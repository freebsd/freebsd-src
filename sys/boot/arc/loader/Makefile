#	$NetBSD: Makefile,v 1.12 1998/02/19 14:18:36 drochner Exp $
# $FreeBSD$

BASE= 		loader
PROG= 		${BASE}.exe
NOMAN=
NEWVERSWHAT=	"ARC disk boot" ${MACHINE_ARCH}

.PATH:		${.CURDIR}/../common

# i386-specific bootstrap sources
SRCS+=		main.c conf.c

# Always add MI sources 
.PATH:		${.CURDIR}/../../common
.include	<${.CURDIR}/../../common/Makefile.inc>
CFLAGS+=	-ffreestanding -mno-fp-regs
CFLAGS+=	-I${.CURDIR}/../../common -I${.CURDIR}
CFLAGS+=	-I${.CURDIR}/../../.. -I.
CFLAGS+=	-DLOADER

CLEANFILES+=	setdef0.c setdef0.o setdef1.c setdef1.o setdefs.h start.o \
		vers.c vers.o ${BASE}.exe ${BASE}.sym ${BASE}.list
CLEANFILES+=	loader.help
CLEANFILES+=	machine

CFLAGS+=	-Wall

CFLAGS+=	-I${LIBSTANDDIR}
CFLAGS+=	-I${.CURDIR}/../include
CRT=		start.o
STRIP=
BINDIR?=	/boot
LOAD_ADDRESS?=	0xffffffff80900000

all: ${PROG}

vers.o: ${.CURDIR}/../../common/newvers.sh ${.CURDIR}/version
	sh ${.CURDIR}/../../common/newvers.sh ${.CURDIR}/version ${NEWVERSWHAT}
	${CC} -c vers.c

${BASE}.exe: ${BASE}.sym ${BASE}.help
	elf2exe ${BASE}.sym ${BASE}.exe

${BASE}.sym: ${OBJS} ${LIBSTAND} ${LIBARC} ${CRT} vers.o
	${LD} -o ${BASE}.sym -M -N -Ttext ${LOAD_ADDRESS} \
	${CRT} ${OBJS} vers.o -L${DESTDIR}${LIBDIR} \
	${LIBSTAND} ${LIBARC} ${LIBSTAND} >${.OBJDIR}/${BASE}.list

${BASE}.help: help.common help.alpha
	cat ${.ALLSRC} | awk -f ${.CURDIR}/../../common/merge_help.awk > ${.TARGET}

beforeinstall:
.if exists(${.OBJDIR}/loader.help)
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
		${.OBJDIR}/${BASE}.help ${DESTDIR}/boot
.else
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
		${.CURDIR}/${BASE}.help ${DESTDIR}/boot
.endif

# Other fragments still to be brought in from ../Makfile.booters?
start.o:	${.CURDIR}/../lib/arch/${MACHINE_ARCH}/start.S
	${CC} -c ${CFLAGS} $<

machine:
	ln -sf ${.CURDIR}/../../../alpha/include machine

.include <bsd.prog.mk>

beforedepend ${OBJS}: machine
