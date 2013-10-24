#  $FreeBSD$

PROG=		boot2.elf
BINDIR?=	/boot
INSTALLFLAGS=	-b

FILES=		${PROG}
SRCS=		start.S			\
		boot2.c			\
		altera_jtag_uart.c	\
		cfi.c

NO_MAN=

AFLAGS=		-G0

CFLAGS=		-ffreestanding			\
		-I${.CURDIR}			\
		-I${.CURDIR}/../../../common	\
		-I${.CURDIR}/../../../..	\
		-D_KERNEL			\
		-Wall				\
		-G0 -Xassembler -G0		\
		-fno-pic -mno-abicalls		\
		-g

LDFLAGS=	-nostdlib			\
		-static				\
		-N				\
		-T ${.CURDIR}/linker.cfg	\
		-G0

.PATH:		${.CURDIR}/../common
CFLAGS+=	-I${.CURDIR}/../common

boot2.elf: linker.cfg

boot2: boot2.elf
	objcopy -S -O binary boot2.elf boot2

boot2.dump: boot2.elf
	objdump -dS boot2.elf > boot2.dump

all: boot2 boot2.dump

.include <bsd.prog.mk>
