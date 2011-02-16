#  $FreeBSD: src/sys/boot/arm/at91/boot0iic/Makefile,v 1.3.14.1 2010/12/21 17:10:29 kensmith Exp $

.PATH:	${.CURDIR}/../libat91

P=boot0iic
FILES=${P}
SRCS=arm_init.S main.c
NO_MAN=
LDFLAGS=-e 0 -T ${.CURDIR}/../linker.cfg
OBJS+=  ${SRCS:N*.h:R:S/$/.o/g}

.include <bsd.prog.mk>

CFLAGS+=-DBOOT_BOOT0
