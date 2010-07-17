# $FreeBSD: src/sys/modules/x86bios/Makefile,v 1.3.2.2.2.1 2010/06/14 02:09:06 kensmith Exp $

KMOD=	x86bios
SRCS=	opt_x86bios.h
SRCS+=	bus_if.h device_if.h pci_if.h

.PATH:	${.CURDIR}/../../contrib/x86emu
.PATH:	${.CURDIR}/../../compat/x86bios
SRCS+=	x86bios.c x86emu.c

.include <bsd.kmod.mk>
