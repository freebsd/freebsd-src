# $FreeBSD: src/sys/modules/rdma/addr/Makefile,v 1.1.4.1.2.1 2009/10/25 01:10:29 kensmith Exp $

RDMA=	${.CURDIR}/../../../contrib/rdma
.PATH: ${RDMA}

KMOD=	rdma_addr
SRCS= device_if.h bus_if.h pci_if.h pcib_if.h opt_sched.h
SRCS+= rdma_addr.c 

.include <bsd.kmod.mk>
