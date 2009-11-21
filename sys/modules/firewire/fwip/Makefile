# $FreeBSD: src/sys/modules/firewire/fwip/Makefile,v 1.3.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $

# Makefile for the fwip(4) module (IP over IEEE1394)

.PATH: ${.CURDIR}/../../../dev/firewire ${.CURDIR}/../../../net

KMOD    = if_fwip
SRCS    = bus_if.h device_if.h \
          opt_inet.h opt_inet6.h \
          if_fwip.c if_fwipvar.h \
          firewire.h firewirereg.h \
          if_fwsubr.c

.if !defined(KERNBUILDDIR)
opt_inet.h:
	echo "#define INET      1" > ${.TARGET}
.endif

#CFLAGS += -DDEVICE_POLLING

.include <bsd.kmod.mk>
