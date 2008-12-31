# $Id: Makefile.def,v 1.18 2007/03/22 11:20:15 gmm Exp $
# $FreeBSD: src/sys/modules/hptrr/Makefile,v 1.1.2.1.4.1 2008/11/25 02:59:29 kensmith Exp $

HPTRR=  ${.CURDIR}/../../dev/hptrr
.PATH: ${HPTRR}
KMOD = hptrr
SRCS = opt_scsi.h opt_cam.h bus_if.h device_if.h pci_if.h os_bsd.h
SRCS+= hptrr_os_bsd.c hptrr_osm_bsd.c hptrr_config.c
OBJS = hptrr_lib.o

hptrr_lib.o:
	uudecode -p < ${HPTRR}/$(MACHINE_ARCH)-elf.hptrr_lib.o.uu > hptrr_lib.o

.include <bsd.kmod.mk>
