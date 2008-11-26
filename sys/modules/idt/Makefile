# $FreeBSD: src/sys/modules/idt/Makefile,v 1.1.30.1 2008/10/02 02:57:24 kensmith Exp $

.PATH:	${.CURDIR}/../../dev/idt
  
KMOD=	idt
SRCS=	idt_pci.c idt_harp.c idt.c
SRCS+=	pci_if.h device_if.h bus_if.h

.include <bsd.kmod.mk>
