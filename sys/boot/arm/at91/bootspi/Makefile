#  $FreeBSD$

.include <src.opts.mk>

.PATH:	${.CURDIR}/../libat91

P=bootspi
FILES=${P}
SRCS=arm_init.S main.c loader_prompt.c env_vars.c ee.c
MAN=
LDFLAGS=-e 0 -T ${.CURDIR}/../linker.cfg
OBJS+=  ${SRCS:N*.h:R:S/$/.o/g}

.include <bsd.prog.mk>

.if ${MK_FPGA} == "yes"
CFLAGS += -DTSC_FPGA
.endif
CFLAGS += -DBOOT_COMMANDS
