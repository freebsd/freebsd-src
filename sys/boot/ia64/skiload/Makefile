# $FreeBSD$

.PATH: ${.CURDIR}/../common

PROG= 		skiload
NOMAN=
NEWVERSWHAT=	"ia64 SKI boot" ${MACHINE_ARCH}
BINDIR?=	/boot

SRCS+=		conf.c main.c start.S

CFLAGS+=	-ffreestanding

.if !defined(NOFORTH)
# Enable BootForth
BOOT_FORTH=	yes
CFLAGS+=	-DBOOT_FORTH
CFLAGS+=	-I${.CURDIR}/../../ficl -I${.CURDIR}/../../ficl/${MACHINE_ARCH}
.if exists(${.OBJDIR}/../../ficl/libficl.a)
LIBFICL=	${.OBJDIR}/../../ficl/libficl.a
.else
LIBFICL=	${.CURDIR}/../../ficl/libficl.a
.endif
.endif

# where to get libstand from
.if exists(${.OBJDIR}/../../../../lib/libstand/libstand.a)
LIBSTAND=       ${.OBJDIR}/../../../../lib/libstand/libstand.a
.else
LIBSTAND=       ${.CURDIR}/../../../../lib/libstand/libstand.a
.endif

.if exists(${.OBJDIR}/../libski/libski.a)
LIBSKI=		${.OBJDIR}/../libski/libski.a
.else
LIBSKI=		${.CURDIR}/../libski/libski.a
.endif

# Always add MI sources 
.PATH: ${.CURDIR}/../../common
.include <${.CURDIR}/../../common/Makefile.inc>

CFLAGS+= -I-
CFLAGS+= -I${.CURDIR}/../include
CFLAGS+= -I${.CURDIR}/../include/${MACHINE_ARCH}
CFLAGS+= -I${.CURDIR}/../../common -I${.CURDIR}
CFLAGS+= -I${.CURDIR}/../../.. -I.
CFLAGS+= -I${.CURDIR}/../libski
CFLAGS+= -DLOADER

LDFLAGS=	-nostdlib -T ${.CURDIR}/ldscript.ia64

CLEANFILES+=	vers.c vers.o ${PROG}.list
CLEANFILES+=	loader.help
CLEANFILES+=	machine

all: ${PROG}

vers.o: ${.CURDIR}/../../common/newvers.sh ${.CURDIR}/version
	sh ${.CURDIR}/../../common/newvers.sh ${.CURDIR}/version ${NEWVERSWHAT}
	${CC} -c vers.c

${PROG}.help:	help.common help.efi
	cat ${.ALLSRC} | awk -f ${.CURDIR}/../../common/merge_help.awk \
	    > ${.TARGET}

beforeinstall:
.if exists(${.OBJDIR}/${PROG}.help)
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${.OBJDIR}/${PROG}.help ${DESTDIR}/boot
.endif

machine:
	ln -sf ${.CURDIR}/../../../${MACHINE_ARCH}/include machine

${PROG}: ${OBJS} ${LIBFICL} ${LIBSKI} ${LIBSTAND} vers.o
	${LD} ${LDFLAGS} -o ${PROG} -M \
	    ${OBJS} vers.o \
	    ${LIBFICL} ${LIBSTAND} ${LIBSKI} ${LIBSTAND} \
	    > ${.OBJDIR}/${PROG}.list

.include <bsd.prog.mk>

beforedepend ${OBJS}: machine
