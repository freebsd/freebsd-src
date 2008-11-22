# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/ncv

KMOD=	ncv
SRCS=	ncr53c500_pccard.c ncr53c500.c 
SRCS+=	opt_bdg.h device_if.h bus_if.h pci_if.h isa_if.h card_if.h pccarddevs.h
SRCS+=	opt_cam.h opt_scsi.h opt_ddb.h

.include <bsd.kmod.mk>
