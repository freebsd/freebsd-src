# $FreeBSD: src/sys/modules/x86bios/Makefile,v 1.3.2.2.4.1 2010/12/21 17:09:25 kensmith Exp $

KMOD=	x86bios
SRCS=	opt_x86bios.h
SRCS+=	bus_if.h device_if.h pci_if.h

.PATH:	${.CURDIR}/../../contrib/x86emu
.PATH:	${.CURDIR}/../../compat/x86bios
SRCS+=	x86bios.c x86emu.c

.include <bsd.kmod.mk>
