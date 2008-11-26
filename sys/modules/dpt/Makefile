# $FreeBSD: src/sys/modules/dpt/Makefile,v 1.1.24.1 2008/10/02 02:57:24 kensmith Exp $
#

.PATH:	${.CURDIR}/../../dev/dpt
KMOD=	dpt
SRCS=	dpt_scsi.c dpt.h \
	dpt_pci.c pci_if.h \
	opt_dpt.h \
	opt_cam.h opt_scsi.h \
	device_if.h bus_if.h
#SRCS+=	dpt_eisa.c eisa_if.h \
#SRCS+=	dpt_isa.c isa_if.h \

.include <bsd.kmod.mk>
