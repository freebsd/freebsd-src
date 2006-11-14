# $FreeBSD$

.PATH:	${.CURDIR}/../../dev/hifn
KMOD	= hifn
SRCS	= hifn7751.c opt_hifn.h
SRCS   += device_if.h bus_if.h pci_if.h
SRCS   += opt_bdg.h opt_bus.h crypto_if.h

.if !defined(KERNBUILDDIR)
opt_hifn.h:
	echo "#define HIFN_DEBUG 1" > opt_hifn.h
.endif

.include <bsd.kmod.mk>
