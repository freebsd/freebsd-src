#  $FreeBSD: src/sys/boot/arm/at91/boot0/Makefile,v 1.5 2006/11/16 00:53:27 imp Exp $

.PATH:	${.CURDIR}/../libat91

P=boot0
FILES=${P}
SRCS=arm_init.S main.c
NO_MAN=
LDFLAGS=-e 0 -T ${.CURDIR}/linker.cfg
OBJS+=  ${SRCS:N*.h:R:S/$/.o/g}

.include <bsd.prog.mk>

CFLAGS+=-DBOOT_BOOT0
