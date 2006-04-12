#  $FreeBSD$

PROG=boot0
SRCS=arm_init.s at91rm9200_lowlevel.c lib.c main.c xmodem.c
NO_MAN=
LDFLAGS=-e 0 -T linker.cfg
CFLAGS=-O2 -mcpu=arm9 -ffreestanding -I${.CURDIR}/../inc
CFLAGS+=-DBOOT0_KB9202
OBJS+=  ${SRCS:N*.h:R:S/$/.o/g}
CLEANFILES=${OBJS} ${PROG} ${PROG}.out

all:	${PROG}

${PROG}:	${PROG}.out ${OBJS}
	objcopy -S -O binary ${PROG}.out ${PROG}

${PROG}.out: ${OBJS}
	ld ${LDFLAGS} -o ${PROG}.out ${OBJS}

clean:
	rm -f ${CLEANFILES}
