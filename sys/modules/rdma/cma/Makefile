# $FreeBSD: src/sys/modules/rdma/cma/Makefile,v 1.1.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $

RDMA=	${.CURDIR}/../../../contrib/rdma
.PATH: ${RDMA}

KMOD=  rdma_cma
SRCS=  device_if.h bus_if.h pci_if.h pcib_if.h opt_sched.h
SRCS+= rdma_cma.c 

.include <bsd.kmod.mk>
