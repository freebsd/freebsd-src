# $FreeBSD$

S	= ${.CURDIR}/../..
.PATH:	$S/dev/aha
KMOD	= aha
SRCS	= aha.c aha_isa.c ahareg.h opt_cam.h device_if.h bus_if.h aha.h \
	opt_scsi.h isa_if.h
CLEANFILES	+= aha.h device_if.h bus_if.h opt_cam.h opt_scsi.h isa_if.h
CFLAGS	+= ${DEBUG_FLAGS}

aha.h:
	echo "#define NAHA 1" > aha.h

opt_cam.h:
	touch opt_cam.h

opt_scsi.h:
	echo "#define SCSI_DELAY 15000" > opt_scsi.h

device_if.h: $S/kern/makedevops.pl $S/kern/device_if.m
	perl $S/kern/makedevops.pl -h $S/kern/device_if.m

bus_if.h: $S/kern/makedevops.pl $S/kern/bus_if.m
	perl $S/kern/makedevops.pl -h $S/kern/bus_if.m

isa_if.h: $S/kern/makedevops.pl $S/isa/isa_if.m
	perl $S/kern/makedevops.pl -h $S/isa/isa_if.m

.include <bsd.kmod.mk>
