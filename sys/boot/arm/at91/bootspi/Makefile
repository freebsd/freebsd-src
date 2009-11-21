#  $FreeBSD: src/sys/boot/arm/at91/bootspi/Makefile,v 1.6.10.1.2.1 2009/10/25 01:10:29 kensmith Exp $

.PATH:	${.CURDIR}/../libat91

P=bootspi
FILES=${P}
SRCS=arm_init.S main.c loader_prompt.c env_vars.c ee.c
NO_MAN=
LDFLAGS=-e 0 -T ${.CURDIR}/../linker.cfg
OBJS+=  ${SRCS:N*.h:R:S/$/.o/g}

.include <bsd.prog.mk>

.if ${MK_FPGA} == "yes"
CFLAGS += -DTSC_FPGA
.endif
CFLAGS += -DBOOT_COMMANDS
