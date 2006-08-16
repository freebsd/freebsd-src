#  $FreeBSD$

.PATH:	${.CURDIR}/../boot0

P=boot0spi
FILES=${P}
SRCS=arm_init.s main.c
NO_MAN=
LDFLAGS=-e 0 -T ${.CURDIR}/../linker.cfg
OBJS+=  ${SRCS:N*.h:R:S/$/.o/g}

.include <bsd.prog.mk>

