# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/ahb

KMOD=	ahb
SRCS=	ahb.c opt_cam.h device_if.h bus_if.h eisa_if.h opt_scsi.h

.if !defined(KERNBUILDDIR)
opt_scsi.h:
	echo "#define SCSI_DELAY 15000" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
