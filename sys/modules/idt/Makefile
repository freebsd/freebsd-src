# $FreeBSD$

.PATH:	${.CURDIR}/../../dev/idt
  
KMOD=	idt
SRCS=	idt_pci.c idt_harp.c idt.c
SRCS+=	pci_if.h device_if.h bus_if.h

.include <bsd.kmod.mk>
