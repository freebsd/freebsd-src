# $FreeBSD: src/sys/modules/rdma/addr/Makefile,v 1.1.2.1.8.1 2010/12/21 17:10:29 kensmith Exp $

RDMA=	${.CURDIR}/../../../contrib/rdma
.PATH: ${RDMA}

KMOD=	rdma_addr
SRCS= device_if.h bus_if.h pci_if.h pcib_if.h opt_sched.h
SRCS+= rdma_addr.c 

.include <bsd.kmod.mk>
