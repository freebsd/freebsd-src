#  $FreeBSD: src/sys/boot/arm/at91/bootiic/Makefile,v 1.5.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH:	${.CURDIR}/../libat91

P=bootiic
FILES=${P}
SRCS=arm_init.S main.c loader_prompt.c env_vars.c
NO_MAN=
LDFLAGS=-e 0 -T ${.CURDIR}/../linker.cfg
OBJS+=  ${SRCS:N*.h:R:S/$/.o/g}

.include <bsd.prog.mk>

CFLAGS += -DBOOT_IIC -DBOOT_COMMANDS
