# $FreeBSD: src/sys/modules/rdma/iwcm/Makefile,v 1.1.4.1.6.1 2010/12/21 17:09:25 kensmith Exp $

RDMA=	${.CURDIR}/../../../contrib/rdma
.PATH: ${RDMA}

KMOD=  rdma_iwcm
SRCS=  device_if.h bus_if.h pci_if.h pcib_if.h opt_sched.h
SRCS+= rdma_iwcm.c 

.include <bsd.kmod.mk>
