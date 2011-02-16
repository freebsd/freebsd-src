# $FreeBSD: src/sys/modules/cxgb/toecore/Makefile,v 1.3.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

CXGB = ${.CURDIR}/../../../dev/cxgb
.PATH: ${CXGB}/ulp/toecore

KMOD=	toecore
SRCS=   toedev.c
SRCS+=	device_if.h bus_if.h pci_if.h opt_sched.h opt_inet.h
CFLAGS+= -g -I${CXGB}

.include <bsd.kmod.mk>
