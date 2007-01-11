#  $FreeBSD: src/sys/boot/arm/at91/boot0/Makefile,v 1.3.2.1 2006/08/17 22:57:07 imp Exp $

P=boot0
FILES=${P}
SRCS=arm_init.s main.c
NO_MAN=
LDFLAGS=-e 0 -T linker.cfg
OBJS+=  ${SRCS:N*.h:R:S/$/.o/g}

.include <bsd.prog.mk>

