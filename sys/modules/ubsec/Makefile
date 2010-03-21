# $FreeBSD: src/sys/modules/ubsec/Makefile,v 1.6.10.1 2010/02/10 00:26:20 kensmith Exp $

.PATH:	${.CURDIR}/../../dev/ubsec
KMOD	= ubsec
SRCS	= ubsec.c opt_ubsec.h
SRCS   += device_if.h bus_if.h pci_if.h
SRCS   += opt_bus.h cryptodev_if.h

.if !defined(KERNBUILDDIR)
opt_ubsec.h:
	echo "#define UBSEC_DEBUG 1" > opt_ubsec.h
.endif

.include <bsd.kmod.mk>
