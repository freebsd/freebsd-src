# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/ct

KMOD=	ct
SRCS=	bshw_machdep.c ct.c ct_isa.c
SRCS+=	device_if.h bus_if.h isa_if.h 
SRCS+=	opt_ct.h opt_cam.h opt_scsi.h opt_ddb.h

.include <bsd.kmod.mk>
