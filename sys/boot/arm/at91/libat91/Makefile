#  $FreeBSD: src/sys/boot/arm/at91/libat91/Makefile,v 1.4.2.1 2006/08/17 22:57:08 imp Exp $

.include "${.CURDIR}/../Makefile.inc"

LIB=		at91
INTERNALLIB=
SRCS=at91rm9200_lowlevel.c delay.c eeprom.c emac.c emac_init.c getc.c \
	p_string.c putchar.c printf.c reset.c spi_flash.c xmodem.c \
	sd-card.c mci_device.c
NO_MAN=

.if ${MK_TAG_LIST} != "no"
CFLAGS += -I${.CURDIR}/.. -DSUPPORT_TAG_LIST
SRCS+=tag_list.c
.endif

.include <bsd.lib.mk>
