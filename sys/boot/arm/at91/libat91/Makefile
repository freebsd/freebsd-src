#  $FreeBSD: src/sys/boot/arm/at91/libat91/Makefile,v 1.10.6.1 2008/11/25 02:59:29 kensmith Exp $

.include "${.CURDIR}/../Makefile.inc"

LIB=		at91
INTERNALLIB=
SRCS=at91rm9200_lowlevel.c delay.c eeprom.c emac.c emac_init.c getc.c \
	putchar.c printf.c reset.c spi_flash.c xmodem.c \
	sd-card.c strcvt.c strlen.c strcmp.c memcpy.c strcpy.c \
	memset.c memcmp.c
SRCS+=ashldi3.c divsi3.S
NO_MAN=

.if ${MK_TAG_LIST} != "no"
CFLAGS += -I${.CURDIR}/.. -DSUPPORT_TAG_LIST
SRCS+=tag_list.c
.endif

.include <bsd.lib.mk>
