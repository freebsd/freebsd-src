#	$Id: Makefile,v 1.1.1.1 1998/10/05 10:08:37 rnordier Exp $

PROG=		boot0
NOMAN=
STRIP=
BINDIR?=	/boot

M4?=	m4

ORG=	0x600

boot0: boot0.o
	${LD} -N -e start -Ttext ${ORG} -o boot0.out boot0.o
	/usr/libexec/elf/objcopy -S -O binary boot0.out ${.TARGET}

boot0.o: boot0.m4 boot0.s
	(cd ${.CURDIR}; ${M4} boot0.m4 boot0.s) | \
	${AS} ${AFLAGS} -o ${.TARGET}

CLEANFILES+= boot0.out boot0.o

.include <bsd.prog.mk>
