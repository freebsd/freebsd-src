#	$NetBSD: Makefile,v 1.12 1998/02/19 14:18:36 drochner Exp $

BASE= 		loader
PROG= 		${BASE}
NOMAN=
NEWVERSWHAT=	"SRM disk boot"

.PATH:		${.CURDIR}/../common

# Alpha-specific bootstrap sources
SRCS+=		main.c conf.c

# Always add MI sources 
.PATH:		${.CURDIR}/../../common
.include	<${.CURDIR}/../../common/Makefile.inc>
CFLAGS+=	-mno-fp-regs
CFLAGS+=	-I${.CURDIR}/../../common -I${.CURDIR}
CFLAGS+=	-I${.CURDIR}/../../.. -I.
CFLAGS+=	-DLOADER
CFLAGS+=	-DPRIMARY_LOAD_ADDRESS=${PRIMARY_LOAD_ADDRESS} \
		-DSECONDARY_LOAD_ADDRESS=${SECONDARY_LOAD_ADDRESS}

CLEANFILES+=	vers.c vers.o gensetdefs.o gensetdefs setdef0.o setdef1.o \
		setdefs.h start.o
CLEANFILES+=	${BASE} ${BASE}.sym ${BASE}.list

CFLAGS+=	-Wall

CFLAGS+=	-I${LIBSTANDDIR}
CFLAGS+=	-I${.CURDIR}/..
CRT=		start.o
STRIP=
BINDIR?=	/boot

all: ${BASE}

vers.o: ${.CURDIR}/newvers.sh ${.CURDIR}/Makefile
	sh ${.CURDIR}/newvers.sh ${.CURDIR}/version ${NEWVERSWHAT}
	${CC} -c vers.c

${BASE}: ${BASE}.sym ${BASE}.help
	objcopy -O binary ${BASE}.sym ${BASE}

${BASE}.sym: ${OBJS} ${LIBSTAND} ${LIBALPHA} ${CRT} vers.o setdef0.o setdef1.o
	${LD} -o ${BASE}.sym -M -e start -N -Ttext ${SECONDARY_LOAD_ADDRESS} \
	${CRT} setdef0.o ${OBJS} setdef1.o \
	vers.o ${LIBSTAND} ${LIBALPHA} ${LIBSTAND} >${.OBJDIR}/${BASE}.list

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
start.o:	${.CURDIR}/../libalpha/start.S
	${CC} -c ${CFLAGS} $<

setdef0.o: setdefs.h

setdef1.o: setdefs.h

machine:
	ln -sf ${.CURDIR}/../../../alpha/include machine

CLEANFILES+=	machine

.include <bsd.prog.mk>

setdefs.h: gensetdefs ${OBJS}
	@echo Generating linker sets
	@./gensetdefs ${OBJS} >setdefs.h

gensetdefs: gensetdefs.o
	${CC} -static gensetdefs.o -o $@

gensetdefs.o: gensetdefs.c
	${CC} -c $<

beforedepend ${OBJS}: machine
