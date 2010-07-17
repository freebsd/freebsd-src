# $FreeBSD: src/sys/modules/hifn/Makefile,v 1.7.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH:	${.CURDIR}/../../dev/hifn
KMOD	= hifn
SRCS	= hifn7751.c opt_hifn.h
SRCS   += device_if.h bus_if.h pci_if.h
SRCS   += opt_bus.h cryptodev_if.h

.if !defined(KERNBUILDDIR)
opt_hifn.h:
	echo "#define HIFN_DEBUG 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
