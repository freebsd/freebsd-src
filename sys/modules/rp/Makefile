# $FreeBSD: src/sys/modules/rp/Makefile,v 1.5.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/rp

KMOD=	rp
SRCS=	rp.c rp_pci.c device_if.h bus_if.h pci_if.h opt_compat.h

.if !defined(KERNBUILDDIR)
opt_compat.h:
	echo "#define COMPAT_43 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
