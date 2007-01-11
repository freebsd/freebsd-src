#  $FreeBSD: src/sys/boot/arm/at91/bootspi/Makefile,v 1.2.2.1 2006/08/17 22:57:08 imp Exp $

P=bootspi
FILES=${P}
SRCS=arm_init.S main.c loader_prompt.c env_vars.c
NO_MAN=
LDFLAGS=-e 0 -T ${.CURDIR}/../linker.cfg
OBJS+=  ${SRCS:N*.h:R:S/$/.o/g}

.include <bsd.prog.mk>

.if ${MK_FPGA} != "no"
CFLAGS += -DTSC_FPGA
.endif
