# $FreeBSD$

.PATH: ${.CURDIR}/../common

PROG= 		skiload
NOMAN=
NEWVERSWHAT=	"ia64 SKI boot" ${MACHINE_ARCH}

SRCS+=		main.c conf.c

# Enable BootForth
BOOT_FORTH=	yes
CFLAGS+=	-g
CFLAGS+=	-I${.CURDIR}/../../ficl -I${.CURDIR}/../../ficl/alpha
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
#XXX need a better way to do this
LIBSTAND=       ${.CURDIR}/../../../../lib/libstand/libstand.a
.if !exists(${LIBSTAND})
LIBSTAND=       ${.OBJDIR}/../../../../lib/libstand/libstand.a
.if !exists(${LIBSTAND})
LIBSTAND=       -lstand
.endif
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
CFLAGS+= -ffreestanding

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

# Other fragments still to be brought in from ../Makfile.booters?
start.o: ${.CURDIR}/../libefi/arch/${MACHINE_ARCH}/start.S
	${CC} -c ${CFLAGS} ${.IMPSRC}

machine:
	ln -sf ${.CURDIR}/../../../${MACHINE_ARCH}/include machine

${PROG}: ${OBJS} ${LIBFICL} ${LIBSKI} vers.o
	${LD} ${LDFLAGS} -o ${PROG} -M \
	    ${OBJS} vers.o \
	    ${LIBFICL} ${LIBSTAND} ${LIBSKI} ${LIBSTAND} \
	    > ${.OBJDIR}/${PROG}.list

.include <bsd.prog.mk>

beforedepend ${OBJS}: machine
