#	$Id: $

PROG=	kgzldr.o
SRCS=	start.s boot.c inflate.c lib.c crt.s sio.s
CFLAGS=	-fno-builtin -Os
CFLAGS+=-DKZIP
LDFLAGS=-nostdlib -static -r
NOMAN=
BINDIR=	/usr/lib
BINMODE=444
STRIP=
.PATH:	${.CURDIR}/../../../kern

M4?=	m4
MFLAGS=

BOOT_COMCONSOLE_PORT?=	0x3f8
MFLAGS+=-DSIOPRT=${BOOT_COMCONSOLE_PORT}

all: kgzldr.o

kgzldr.o: ${OBJS}
	${CC} ${LDFLAGS} -o ${.TARGET} ${OBJS}

.s.o:
	(cd ${.CURDIR}; ${M4} ${MFLAGS} ${.IMPSRC}) |\
		${AS} ${AFLAGS} -o ${.TARGET}

.include <bsd.prog.mk>
