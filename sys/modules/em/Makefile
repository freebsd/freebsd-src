#$FreeBSD: src/sys/modules/em/Makefile,v 1.1.2.1 2001/12/15 01:41:00 pdeuskar Exp $
.PATH:	${.CURDIR}/../../dev/em
KMOD	= if_em
SRCS	= device_if.h bus_if.h pci_if.h opt_bdg.h
SRCS    += if_em.c if_em_fxhw.c if_em_phy.c

clean:
	rm -f opt_bdg.h device_if.h bus_if.h pci_if.h setdef*
	rm -f *.o *.kld *.ko
	rm -f @ machine

.include <bsd.kmod.mk>
