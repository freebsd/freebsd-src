# $FreeBSD$

FILES=	kgzldr.o
SRCS=	start.s boot.c inflate.c lib.c crt.s sio.s
OBJS=	${SRCS:N*.h:R:S/$/.o/g}
CFLAGS=	-ffreestanding
CFLAGS+=-Os
CFLAGS+=-DKZIP
LDFLAGS=-nostdlib -static -r
BINDIR=	/usr/lib
.PATH:	${.CURDIR}/../../../kern

.PATH:	${.CURDIR}/../../i386/kgzldr
AFLAGS+=--defsym PC98=1

CLEANFILES=${FILES}

M4?=	m4

BOOT_COMCONSOLE_PORT?=	0x238
M4FLAGS+=-DSIOPRT=${BOOT_COMCONSOLE_PORT}

kgzldr.o: ${OBJS}
	${CC} ${LDFLAGS} -o ${.TARGET} ${OBJS}

.s.o:
	(cd ${.CURDIR}; ${M4} ${M4FLAGS} ../../i386/kgzldr/kgzldr.m4 ${.IMPSRC}) |\
		${AS} ${AFLAGS} -o ${.TARGET}

.include <bsd.prog.mk>
