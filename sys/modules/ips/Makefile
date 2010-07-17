# $FreeBSD: src/sys/modules/ips/Makefile,v 1.1.36.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH:		${.CURDIR}/../../dev/ips
KMOD		= ips
SRCS		= ips.c ips_pci.c ips.h ips_disk.c ips_disk.h ips_commands.c \
			ips_ioctl.h ips_ioctl.c device_if.h bus_if.h pci_if.h 

.include <bsd.kmod.mk>
