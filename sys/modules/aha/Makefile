# $FreeBSD$

.PATH:	${.CURDIR}/../../dev/aha
KMOD	= aha
SRCS	= aha.c aha_isa.c ahareg.h opt_cam.h device_if.h bus_if.h aha.h \
	opt_scsi.h isa_if.h
CLEANFILES	= aha.h

aha.h:
	echo "#define NAHA 1" > aha.h

opt_scsi.h:
	echo "#define SCSI_DELAY 15000" > opt_scsi.h

.include <bsd.kmod.mk>
