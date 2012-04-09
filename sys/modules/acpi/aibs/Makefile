# $FreeBSD: src/sys/modules/acpi/aibs/Makefile,v 1.1.2.2.4.1 2012/03/03 06:15:13 kensmith Exp $

.PATH:	${.CURDIR}/../../../dev/acpi_support

KMOD=	aibs
SRCS=	atk0110.c
SRCS+=	opt_acpi.h acpi_if.h bus_if.h device_if.h
SRCS+=	opt_ddb.h

.include <bsd.kmod.mk>
