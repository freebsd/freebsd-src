# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/pccard

KMOD=	pccard
SRCS=	pccard.c pccard_cis.c pccard_cis_quirks.c \
	device_if.h bus_if.h card_if.h power_if.h

EXPORT_SYMS=	YES	# XXX evaluate

.include <bsd.kmod.mk>
