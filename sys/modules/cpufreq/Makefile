# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/cpufreq

KMOD=	cpufreq
WARNS?=	2
SRCS=	ichss.c
SRCS+=	bus_if.h cpufreq_if.h device_if.h pci_if.h

.include <bsd.kmod.mk>
