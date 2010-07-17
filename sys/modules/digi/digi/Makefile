# $FreeBSD: src/sys/modules/digi/digi/Makefile,v 1.6.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH:	${.CURDIR}/../../../dev/digi
KMOD=	digi
SRCS=	digi.c digi_pci.c digi_isa.c
SRCS+=	digi.h digi_pci.h digireg.h digi_mod.h
SRCS+=	bus_if.h pci_if.h device_if.h
SRCS+=	opt_compat.h

.if !defined(KERNBUILDDIR)
opt_compat.h:
	echo "#define COMPAT_43 1" > ${.TARGET}
	echo "#define COMPAT_FREEBSD6 1" >> ${.TARGET}
.endif

.include <bsd.kmod.mk>
