# $FreeBSD: src/usr.sbin/eeprom/Makefile,v 1.3.30.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../ofwdump

PROG=	eeprom
MAN=	eeprom.8
MANSUBDIR= /sparc64
SRCS=	eeprom.c ofw_options.c ofw_util.c
WARNS?=	6
CFLAGS+= -I${.CURDIR}/../ofwdump

.include <bsd.prog.mk>
