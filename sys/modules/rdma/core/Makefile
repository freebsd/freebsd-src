# $FreeBSD: src/sys/modules/rdma/core/Makefile,v 1.1.2.1.6.1 2010/02/10 00:26:20 kensmith Exp $

RDMA=	${.CURDIR}/../../../contrib/rdma
.PATH: ${RDMA}

KMOD=	rdma_core
SRCS= device_if.h bus_if.h pci_if.h pcib_if.h
SRCS+= rdma_device.c
SRCS+= rdma_cache.c
SRCS+= rdma_verbs.c

.include <bsd.kmod.mk>
