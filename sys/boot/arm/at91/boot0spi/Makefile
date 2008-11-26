#  $FreeBSD: src/sys/boot/arm/at91/boot0spi/Makefile,v 1.2.2.1.6.1 2008/10/02 02:57:24 kensmith Exp $

.PATH:	${.CURDIR}/../boot0

P=boot0spi
FILES=${P}
SRCS=arm_init.s main.c
NO_MAN=
LDFLAGS=-e 0 -T ${.CURDIR}/../linker.cfg
OBJS+=  ${SRCS:N*.h:R:S/$/.o/g}

.include <bsd.prog.mk>

