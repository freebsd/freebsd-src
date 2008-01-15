#  $FreeBSD: src/sys/boot/arm/at91/boot0spi/Makefile,v 1.2.2.1 2006/08/17 22:57:07 imp Exp $

.PATH:	${.CURDIR}/../boot0

P=boot0spi
FILES=${P}
SRCS=arm_init.s main.c
NO_MAN=
LDFLAGS=-e 0 -T ${.CURDIR}/../linker.cfg
OBJS+=  ${SRCS:N*.h:R:S/$/.o/g}

.include <bsd.prog.mk>

