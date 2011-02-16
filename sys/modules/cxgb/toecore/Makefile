# $FreeBSD: src/sys/modules/cxgb/toecore/Makefile,v 1.1.2.1.8.1 2010/12/21 17:10:29 kensmith Exp $
TOECORE = ${.CURDIR}/../../../dev/cxgb/ulp/toecore
.PATH: ${TOECORE} 

KMOD=	toecore
SRCS=   toedev.c
SRCS+=	device_if.h bus_if.h pci_if.h opt_sched.h
.include <bsd.kmod.mk>