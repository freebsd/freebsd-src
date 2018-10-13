# $FreeBSD$

SYSDIR?=${SRCTOP}/sys
.include "${SYSDIR}/conf/kern.opts.mk"

.PATH:	${SYSDIR}/dev/dpt
KMOD=	dpt
SRCS=	dpt_scsi.c dpt.h \
	dpt_pci.c pci_if.h \
	opt_dpt.h \
	opt_cam.h opt_scsi.h \
	device_if.h bus_if.h

.include <bsd.kmod.mk>
