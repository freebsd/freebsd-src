# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/trm

KMOD=	trm
SRCS=	trm.c trm.h opt_cam.h device_if.h bus_if.h \
	opt_scsi.h pci_if.h

.if !defined(KERNBUILDDIR)
opt_scsi.h:
	echo "#define SCSI_DELAY 15000" > opt_scsi.h
.endif

.include <bsd.kmod.mk>
