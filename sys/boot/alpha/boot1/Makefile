# $NetBSD: Makefile,v 1.15 1998/03/28 00:21:35 thorpej Exp $
# $FreeBSD$

.include <../Makefile.inc>

PROG = boot1

.PATH:		${.CURDIR}/../libalpha

SRCS=		start.S boot1.c
CFLAGS+=	-ffreestanding -mno-fp-regs
CFLAGS+=	-DSECONDARY_LOAD_ADDRESS=${SECONDARY_LOAD_ADDRESS} -DMINIMAL
CFLAGS+=	-DBOOT1
CFLAGS+=	-I${.CURDIR}/../../common -I${.CURDIR}
CFLAGS+=	-I${LIBSTANDDIR}
CFLAGS+=	-I${.CURDIR}/..
CFLAGS+=	-DUFS1_ONLY
NOMAN=1
STRIP=
BINDIR?=	/boot

BOOT_RELOC = ${PRIMARY_LOAD_ADDRESS}

CLEANFILES+= ${PROG}.sym ${PROG}.nosym ${PROG}.list

all:	${PROG}

${PROG}.nosym: ${PROG}.sym
	cp ${PROG}.sym ${PROG}.nosym
	strip ${PROG}.nosym

${PROG}: ${PROG}.nosym
	objcopy -O binary ${PROG}.nosym ${PROG}

.include <bsd.prog.mk>

start.o:	${.CURDIR}/../libalpha/start.S
	${CC} -c ${CFLAGS} $<

${PROG}.sym: ${OBJS} ${LIBKERN}
	${LD} -M -Ttext ${BOOT_RELOC} -N -e start -o ${PROG}.sym ${OBJS} \
		-L${DESTDIR}${LIBDIR} ${LIBSTAND} ${LIBALPHA} ${LIBSTAND} \
		> ${.OBJDIR}/${PROG}.list
	size ${PROG}.sym
