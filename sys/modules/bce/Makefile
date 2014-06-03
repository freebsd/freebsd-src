# $FreeBSD$
.PATH:	${.CURDIR}/../../dev/bce
KMOD=	if_bce
SRCS=	opt_bce.h if_bce.c miibus_if.h miidevs.h device_if.h bus_if.h pci_if.h

#CFLAGS += -DBCE_DEBUG=0

clean:
	rm -f opt_bdg.h device_if.h bus_if.h pci_if.h export_syms
	rm -f *.o *.kld *.ko
	rm -f @ machine x86 miibus_if.h miidevs.h opt_bce.h

.include <bsd.kmod.mk>
