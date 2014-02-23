
# $FreeBSD$

.PATH:	${.CURDIR}/../../../../dev/aic7xxx
KMOD=		ahc_eisa

SRCS=	ahc_eisa.c
SRCS+=	device_if.h bus_if.h eisa_if.h
SRCS+=	opt_scsi.h opt_cam.h opt_aic7xxx.h

CFLAGS+=	-I${.CURDIR}/../../../../dev/aic7xxx -I..

.include <bsd.kmod.mk>
