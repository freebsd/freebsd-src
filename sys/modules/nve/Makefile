# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/nve

KMOD=	if_nve
SRCS=	if_nve.c if_nvereg.h miidevs.h \
	device_if.h bus_if.h pci_if.h miibus_if.h
OBJS+=	nvenetlib.o

CLEANFILES+=	nvenetlib.o
nvenetlib.o: ${.CURDIR}/../../contrib/dev/nve/${MACHINE}/${.TARGET}.bz2.uu
	uudecode < ${.CURDIR}/../../contrib/dev/nve/${MACHINE}/${.TARGET}.bz2.uu
	bzip2 -d ${.TARGET}.bz2

.include <bsd.kmod.mk>
