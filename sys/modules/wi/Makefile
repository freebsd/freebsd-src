# $FreeBSD: src/sys/modules/wi/Makefile,v 1.3.2.2 2001/08/28 06:21:15 imp Exp $

.PATH:	${.CURDIR}/../../i386/isa
KMOD	= if_wi
SRCS	= if_wi.c device_if.h bus_if.h pci_if.h pci.h

pci.h:
	echo "#define NPCI 1" > $@

.include <bsd.kmod.mk>
