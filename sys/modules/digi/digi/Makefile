# $FreeBSD: src/sys/modules/digi/digi/Makefile,v 1.3 2002/02/15 15:45:34 bde Exp $

.PATH:	${.CURDIR}/../../../dev/digi
KMOD=	digi
SRCS=	digi.c digi_pci.c digi_isa.c
SRCS+=	digi.h digi_pci.h digireg.h digi_mod.h
SRCS+=	bus_if.h pci_if.h device_if.h
SRCS+=	opt_compat.h

opt_compat.h:
	echo "#define COMPAT_43 1" > opt_compat.h

.include <bsd.kmod.mk>
