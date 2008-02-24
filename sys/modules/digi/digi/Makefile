# $FreeBSD: src/sys/modules/digi/digi/Makefile,v 1.5 2006/09/27 19:57:00 ru Exp $

.PATH:	${.CURDIR}/../../../dev/digi
KMOD=	digi
SRCS=	digi.c digi_pci.c digi_isa.c
SRCS+=	digi.h digi_pci.h digireg.h digi_mod.h
SRCS+=	bus_if.h pci_if.h device_if.h
SRCS+=	opt_compat.h

.if !defined(KERNBUILDDIR)
opt_compat.h:
	echo "#define COMPAT_43 1" > opt_compat.h
	echo "#define COMPAT_FREEBSD6 1" >> opt_compat.h
.endif

.include <bsd.kmod.mk>
