# $NetBSD: Makefile,v 1.15 1998/03/28 00:21:35 thorpej Exp $

.include <../Makefile.inc>

PROG = boot1

.PATH:		${.CURDIR}/../../common
.PATH:		${.CURDIR}/../libalpha

SRCS=		start.S boot1.c sys.c
CFLAGS+=	-mno-fp-regs
CFLAGS+=	-I${.CURDIR}/../../common -I${.CURDIR}
LIBSTANDDIR=	/home/dfr/FreeBSD/alpha/src/lib/libstand
LIBSTAND=	-lstand
CFLAGS+=	-I${LIBSTANDDIR}
LIBALPHA=	${.OBJDIR}/../libalpha/libalpha.a
CFLAGS+=	-I${.CURDIR}/..
CFLAGS+=	-DSECONDARY_LOAD_ADDRESS=0x${SECONDARY_LOAD_ADDRESS} -DMINIMAL
NOMAN=1
STRIP=

BOOT_RELOC = ${PRIMARY_LOAD_ADDRESS}

CLEANFILES+= ${PROG}.sym ${PROG}.nosym ${PROG}.list

all:	${PROG}

afterinstall:
	ln -sf boot1 /usr/mdec/sdboot

${PROG}.nosym: ${PROG}.sym
	cp ${PROG}.sym ${PROG}.nosym
	strip ${PROG}.nosym

${PROG}: ${PROG}.nosym
	objcopy -O binary ${PROG}.nosym ${PROG}

.include <bsd.prog.mk>

start.o:	${.CURDIR}/../libalpha/start.S
	${CC} -c -DPRIMARY_BOOTBLOCK $<

${PROG}.sym: ${OBJS} ${LIBKERN}
	${LD} -M -Ttext ${BOOT_RELOC} -N -e start -o ${PROG}.sym ${OBJS} \
		${LIBSTAND} ${LIBALPHA} ${LIBSTAND} > ${.OBJDIR}/${PROG}.list
	size ${PROG}.sym

