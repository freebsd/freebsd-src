# $FreeBSD$
TOECORE = ${.CURDIR}/../../../dev/cxgb/ulp/toecore
.PATH: ${TOECORE} 

KMOD=	toecore
SRCS=   toedev.c
SRCS+=	device_if.h bus_if.h pci_if.h opt_sched.h
.include <bsd.kmod.mk>