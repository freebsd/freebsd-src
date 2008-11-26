#  $FreeBSD: src/sys/boot/arm/at91/bootspi/Makefile,v 1.2.2.1.6.1 2008/10/02 02:57:24 kensmith Exp $

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
