# $FreeBSD: src/sys/modules/fatm/Makefile,v 1.2.8.1.8.1 2008/10/02 02:57:24 kensmith Exp $
#
# Author: Harti Brandt <harti@freebsd.org>
#
.PATH: ${.CURDIR}/../../dev/fatm

KMOD=	if_fatm
SRCS=	if_fatm.c device_if.h bus_if.h pci_if.h opt_inet.h opt_natm.h 

# CFLAGS+= -DFATM_DEBUG=0 -DINVARIANT_SUPPORT -DINVARIANTS

.if !defined(KERNBUILDDIR)
opt_inet.h:
	echo "#define INET 1" > opt_inet.h

opt_natm.h:
	echo "#define NATM 1" > opt_natm.h
.endif

.include <bsd.kmod.mk>
