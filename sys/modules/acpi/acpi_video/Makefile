#	$FreeBSD: src/sys/modules/acpi/acpi_video/Makefile,v 1.3 2004/08/03 21:27:43 njl Exp $

.PATH:	${.CURDIR}/../../../dev/acpica

KMOD=	acpi_video
WARNS?=	2
CFLAGS+= -I@/contrib/dev/acpica
SRCS=	acpi_video.c
SRCS+=	opt_acpi.h acpi_if.h bus_if.h device_if.h
NOMAN=

.include <bsd.kmod.mk>
