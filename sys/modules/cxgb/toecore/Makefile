# $FreeBSD: src/sys/modules/cxgb/toecore/Makefile,v 1.3.2.1.8.1 2012/03/03 06:15:13 kensmith Exp $

CXGB = ${.CURDIR}/../../../dev/cxgb
.PATH: ${CXGB}/ulp/toecore

KMOD=	toecore
SRCS=   toedev.c
SRCS+=	device_if.h bus_if.h pci_if.h opt_sched.h opt_inet.h
CFLAGS+= -g -I${CXGB}

.include <bsd.kmod.mk>
