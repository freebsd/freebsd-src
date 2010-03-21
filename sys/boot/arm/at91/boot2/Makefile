#  $FreeBSD: src/sys/boot/arm/at91/boot2/Makefile,v 1.3.12.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/../libat91 ${.CURDIR}/../bootspi

P=boot2
FILES=${P}
SRCS=arm_init.S boot2.c ${BOOT_FLAVOR:L}_board.c
NO_MAN=
LDFLAGS=-e 0 -T ${.CURDIR}/../linker.cfg
OBJS+=  ${SRCS:N*.h:R:S/$/.o/g}

.include <bsd.prog.mk>

.if ${BOOT_FLAVOR} == "kb920x"
CFLAGS+=-DBOOT_IIC
.endif
CFLAGS+= \
	-I${.CURDIR}/../bootspi \
	-I${.CURDIR}/../../../common \
	-I${.CURDIR}/../../../.. \
	-D_KERNEL \
	-DUFS1_ONLY
