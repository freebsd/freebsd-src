# $FreeBSD: src/sys/boot/i386/kgzldr/Makefile,v 1.12 2002/09/30 20:37:57 peter Exp $

FILES=	kgzldr.o
SRCS=	start.s boot.c inflate.c lib.c crt.s sio.s
OBJS=	${SRCS:N*.h:R:S/$/.o/g}
CFLAGS=	-ffreestanding
CFLAGS+=-Os
CFLAGS+=-DKZIP
LDFLAGS=-nostdlib -static -r
BINDIR=	/usr/lib
.PATH:	${.CURDIR}/../../../kern

CLEANFILES=${FILES}

BOOT_COMCONSOLE_PORT?=	0x3f8
AFLAGS+=--defsym SIO_PRT=${BOOT_COMCONSOLE_PORT}

kgzldr.o: ${OBJS}
	${CC} ${LDFLAGS} -o ${.TARGET} ${OBJS}

.include <bsd.prog.mk>
