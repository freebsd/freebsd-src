# $FreeBSD$

.PATH:	${.CURDIR}/../../dev/aic
KMOD	= aic
SRCS	= aic.c aic_pccard.c aic_isa.c 
SRCS	+= opt_bdg.h device_if.h bus_if.h pci_if.h isa_if.h
SRCS	+= opt_cam.h opt_scsi.h

.include <bsd.kmod.mk>
