#	$NetBSD: Makefile,v 1.12 1998/02/19 14:18:36 drochner Exp $

BASE= 		netboot
PROG= 		${BASE}
NOMAN=
NEWVERSWHAT=	"SRM net boot"

.PATH:		${.CURDIR}/../common

# i386-specific bootstrap sources
SRCS+=		main.c conf.c dev_net.c

# Always add MI sources 
.PATH:		${.CURDIR}/../../common
.include	<${.CURDIR}/../../common/Makefile.inc>
CFLAGS+=	-mno-fp-regs
CFLAGS+=	-I${.CURDIR}/../../common -I${.CURDIR}
CFLAGS+=	-I${.CURDIR}/../../.. -I.
CFLAGS+=	-I${.OBJDIR}
CFLAGS+=	-DNETBOOT

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

${BASE}: ${BASE}.sym
	objcopy -O binary ${BASE}.sym ${BASE}

${BASE}.nosym: ${BASE}.sym
	cp ${BASE}.sym ${BASE}.nosym
	strip ${BASE}.nosym

${BASE}.sym: ${OBJS} ${LIBSTAND} ${LIBALPHA} ${CRT} vers.o setdef0.o setdef1.o
	${LD} -o ${BASE}.sym -M -e start -N -Ttext ${PRIMARY_LOAD_ADDRESS} \
	${CRT} setdef0.o ${OBJS} setdef1.o \
	vers.o ${LIBSTAND} ${LIBALPHA} ${LIBSTAND} >${.OBJDIR}/${BASE}.list

start.o:	${.CURDIR}/../libalpha/start.S
	${CC} -c ${CFLAGS} $<

setdef0.o: setdefs.h

setdef1.o: setdefs.h

.include <bsd.prog.mk>

setdefs.h: gensetdefs ${OBJS}
	@echo Generating linker sets
	@./gensetdefs ${OBJS} >setdefs.h

gensetdefs: gensetdefs.o
	${CC} -static gensetdefs.o -o $@

gensetdefs.o: gensetdefs.c
	${CC} -c $<

