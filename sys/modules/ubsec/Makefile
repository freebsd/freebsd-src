# $FreeBSD$

.PATH:	${.CURDIR}/../../dev/ubsec
KMOD	= ubsec
SRCS	= ubsec.c opt_ubsec.h
SRCS   += device_if.h bus_if.h pci_if.h
SRCS   += opt_bdg.h opt_bus.h crypto_if.h

opt_ubsec.h:
	echo "#define UBSEC_DEBUG 1" > opt_ubsec.h

.include <bsd.kmod.mk>
