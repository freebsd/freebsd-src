# $FreeBSD: src/sys/modules/ips/Makefile,v 1.1.10.1 2005/01/13 00:47:33 scottl Exp $

.PATH:	${.CURDIR}/../../dev/ips
KMOD	= ips
SRCS	= ips.c ips_pci.c ips_disk.c ips_commands.c ips_ioctl.c
SRCS	+= device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
