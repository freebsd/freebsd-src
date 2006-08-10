#  $FreeBSD$

.include "${.CURDIR}/../Makefile.inc"

LIB=		at91
INTERNALLIB=
SRCS=at91rm9200_lowlevel.c eeprom.c emac.c getc.c \
	p_string.c putchar.c printf.c spi_flash.c xmodem.c
NO_MAN=

.if ${MK_TAG_LIST} != "no"
CFLAGS += -I${.CURDIR}/.. -DSUPPORT_TAG_LIST
SRCS+=tag_list.c
.endif

.include <bsd.lib.mk>
