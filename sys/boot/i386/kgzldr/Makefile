# $FreeBSD$

MAINTAINER=	rnordier

PROG=	kgzldr.o
SRCS=	start.s boot.c inflate.c lib.c crt.s sio.s
CFLAGS=	-fno-builtin
.if ${OBJFORMAT} == aout
CFLAGS+=-O2
.else
CFLAGS+=-Os
.endif
CFLAGS+=-DKZIP
LDFLAGS=-nostdlib -static -r
NOMAN=
BINDIR=	/usr/lib
BINMODE=444
STRIP=
.PATH:	${.CURDIR}/../../../kern

M4?=	m4
M4FLAGS=-DOBJFORMAT=${OBJFORMAT}

BOOT_COMCONSOLE_PORT?=	0x3f8
M4FLAGS+=-DSIOPRT=${BOOT_COMCONSOLE_PORT}

all: kgzldr.o

kgzldr.o: ${OBJS}
	${CC} ${LDFLAGS} -o ${.TARGET} ${OBJS}

.s.o:
	(cd ${.CURDIR}; ${M4} ${M4FLAGS} kgzldr.m4 ${.IMPSRC}) |\
		${AS} ${AFLAGS} -o ${.TARGET}

.include <bsd.prog.mk>
