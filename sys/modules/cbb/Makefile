# $FreeBSD$

.PATH: ${.CURDIR}/../../pci

KMOD=	cbb
SRCS=	pccbb.c \
	device_if.h bus_if.h

EXPORT_SYMS=	YES	# XXX evaluate

.include <bsd.kmod.mk>
