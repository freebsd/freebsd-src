# $FreeBSD$

FILES=	kgzldr.o
FILESOWN= ${LIBOWN}
FILESGRP= ${LIBGRP}
FILESMODE= ${LIBMODE}
FILESDIR= ${LIBDIR}
CLEANFILES= kgzldr.o

SRCS=	start.s boot.c inflate.c lib.c crt.s sio.s
OBJS=	${SRCS:N*.h:R:S/$/.o/g}
CFLAGS=	-Os
CFLAGS+=-DKZIP
LDFLAGS=-nostdlib -static -Wl,-r
.PATH:	${.CURDIR}/../../../kern

.PATH:	${.CURDIR}/../../i386/kgzldr
AFLAGS+=--defsym PC98=1

BOOT_COMCONSOLE_PORT?=	0x238
AFLAGS+=--defsym SIO_PRT=${BOOT_COMCONSOLE_PORT}

kgzldr.o: ${OBJS}
	${CC} ${LDFLAGS} -o ${.TARGET} ${OBJS}

.include <bsd.prog.mk>
