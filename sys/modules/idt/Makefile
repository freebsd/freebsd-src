# $FreeBSD: src/sys/modules/idt/Makefile,v 1.1 2002/09/30 05:12:39 mdodd Exp $

.PATH:	${.CURDIR}/../../dev/idt
  
KMOD=	idt
SRCS=	idt_pci.c idt_harp.c idt.c
SRCS+=	pci_if.h device_if.h bus_if.h

.include <bsd.kmod.mk>
