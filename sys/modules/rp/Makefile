# $FreeBSD: src/sys/modules/rp/Makefile,v 1.3.16.1 2006/02/10 00:38:06 yar Exp $

.PATH: ${.CURDIR}/../../dev/rp

KMOD=	rp
SRCS=	rp.c rp_pci.c device_if.h bus_if.h pci_if.h opt_compat.h

.if !defined(KERNBUILDDIR)
opt_compat.h:
	echo "#define COMPAT_43 1" > opt_compat.h
.endif

.include <bsd.kmod.mk>
