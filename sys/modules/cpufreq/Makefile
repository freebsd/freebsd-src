# $FreeBSD$

.PATH:	${.CURDIR}/../../dev/cpufreq		\
	${.CURDIR}/../../${MACHINE_ARCH}/cpufreq

KMOD=	cpufreq
WARNS?=	2
SRCS=	ichss.c
SRCS+=	bus_if.h cpufreq_if.h device_if.h pci_if.h

.if ${MACHINE} == "i386"
SRCS+=	est.c
.endif

.include <bsd.kmod.mk>
