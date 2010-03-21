# $FreeBSD: src/sys/modules/cxgb/toecore/Makefile,v 1.1.2.1.6.1 2010/02/10 00:26:20 kensmith Exp $
TOECORE = ${.CURDIR}/../../../dev/cxgb/ulp/toecore
.PATH: ${TOECORE} 

KMOD=	toecore
SRCS=   toedev.c
SRCS+=	device_if.h bus_if.h pci_if.h opt_sched.h
.include <bsd.kmod.mk>