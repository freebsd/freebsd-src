# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/rp

KMOD=	rp
SRCS=	rp.c rp_pci.c device_if.h bus_if.h pci_if.h opt_compat.h

opt_compat.h:
	echo "#define COMPAT_43 1" > opt_compat.h

.include <bsd.kmod.mk>
