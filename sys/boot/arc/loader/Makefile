#	$NetBSD: Makefile,v 1.12 1998/02/19 14:18:36 drochner Exp $

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
CFLAGS+=	-mno-fp-regs
CFLAGS+=	-I${.CURDIR}/../../common -I${.CURDIR}
CFLAGS+=	-I${.CURDIR}/../../.. -I.
CFLAGS+=	-DLOADER

CLEANFILES+=	vers.c vers.o gensetdefs.o gensetdefs setdef0.o setdef1.o \
		setdefs.h start.o
CLEANFILES+=	${BASE} ${BASE}.sym ${BASE}.list

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

${BASE}.exe: ${BASE} ${BASE}.help
	elf2exe ${BASE}.sym ${BASE}.exe

#${BASE}: ${OBJS} ${LIBSTAND} ${LIBARC} ${CRT} vers.o setdef0.o setdef1.o
#	${LD} -o ${BASE}.sym -M -e __start -N -Ttext ${LOAD_ADDRESS} \
#	${CRT} setdef0.o ${OBJS} setdef1.o \
#	vers.o ${LIBSTAND} ${LIBARC} ${LIBSTAND} >${.OBJDIR}/${BASE}.list

${BASE}: ${OBJS} ${LIBSTAND} ${LIBARC} ${CRT} vers.o setdef0.o setdef1.o
	${LD} -o ${BASE}.sym -M -N -Ttext ${LOAD_ADDRESS} \
	${CRT} setdef0.o ${OBJS} setdef1.o \
	vers.o ${LIBSTAND} ${LIBARC} ${LIBSTAND} >${.OBJDIR}/${BASE}.list

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
start.o:	${.CURDIR}/../lib/arch/${MACHINE}/start.S
	${CC} -c ${CFLAGS} $<

setdef0.o: setdefs.h

setdef1.o: setdefs.h

machine:
	ln -sf ${.CURDIR}/../../../alpha/include machine

CLEANFILES+=	machine setdefs.h gensetdefs ${BASE} ${BASE}.exe loader.help

.include <bsd.prog.mk>

setdefs.h: gensetdefs ${OBJS}
	@echo Generating linker sets
	@./gensetdefs ${OBJS} >setdefs.h

gensetdefs: gensetdefs.o
	${CC} -static gensetdefs.o -o $@

gensetdefs.o: gensetdefs.c
	${CC} -c $<

beforedepend ${OBJS}: machine
