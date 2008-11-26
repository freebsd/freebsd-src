# $FreeBSD: src/usr.sbin/eeprom/Makefile,v 1.3.24.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../ofwdump

PROG=	eeprom
MAN=	eeprom.8
MANSUBDIR= /sparc64
SRCS=	eeprom.c ofw_options.c ofw_util.c
WARNS?=	6
CFLAGS+= -I${.CURDIR}/../ofwdump

.include <bsd.prog.mk>
