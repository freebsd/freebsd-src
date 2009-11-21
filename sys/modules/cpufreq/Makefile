# $FreeBSD: src/sys/modules/cpufreq/Makefile,v 1.12.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $

.PATH:	${.CURDIR}/../../dev/cpufreq		\
	${.CURDIR}/../../${MACHINE_ARCH}/cpufreq

KMOD=	cpufreq
SRCS=	ichss.c
SRCS+=	bus_if.h cpufreq_if.h device_if.h pci_if.h

.if ${MACHINE} == "i386" || ${MACHINE} == "amd64"
.PATH:	${.CURDIR}/../../i386/cpufreq

SRCS+=	acpi_if.h opt_acpi.h
SRCS+=	est.c hwpstate.c p4tcc.c powernow.c
.endif

.if ${MACHINE} == "i386"
SRCS+=	smist.c
.endif

.if ${MACHINE} == "powerpc"
.PATH:	${.CURDIR}/../../powerpc/cpufreq
SRCS+= dfs.c
.endif

.include <bsd.kmod.mk>
