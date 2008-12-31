#  $FreeBSD: src/sys/boot/arm/at91/boot0iic/Makefile,v 1.3.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH:	${.CURDIR}/../libat91

P=boot0iic
FILES=${P}
SRCS=arm_init.S main.c
NO_MAN=
LDFLAGS=-e 0 -T ${.CURDIR}/../linker.cfg
OBJS+=  ${SRCS:N*.h:R:S/$/.o/g}

.include <bsd.prog.mk>

CFLAGS+=-DBOOT_BOOT0
