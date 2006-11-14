#	$FreeBSD$

.PATH:	${.CURDIR}/../../../dev/acpica

KMOD=	acpi_video
WARNS?=	2
CFLAGS+= -I@/contrib/dev/acpica
SRCS=	acpi_video.c
SRCS+=	opt_acpi.h acpi_if.h bus_if.h device_if.h
NO_MAN=

.include <bsd.kmod.mk>
