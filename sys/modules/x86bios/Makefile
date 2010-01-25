# $FreeBSD$

KMOD=	x86bios
SRCS=	opt_x86bios.h
SRCS+=	bus_if.h device_if.h pci_if.h

.PATH:	${.CURDIR}/../../contrib/x86emu
.PATH:	${.CURDIR}/../../compat/x86bios
SRCS+=	x86bios.c x86emu.c

.include <bsd.kmod.mk>
