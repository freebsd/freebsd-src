# $FreeBSD: src/sys/boot/pc98/kgzldr/Makefile,v 1.7 2002/09/30 20:37:58 peter Exp $

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

BOOT_COMCONSOLE_PORT?=	0x238
AFLAGS+=--defsym SIO_PRT=${BOOT_COMCONSOLE_PORT}

kgzldr.o: ${OBJS}
	${CC} ${LDFLAGS} -o ${.TARGET} ${OBJS}

.include <bsd.prog.mk>
