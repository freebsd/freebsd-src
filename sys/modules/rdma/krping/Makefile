# $FreeBSD: src/sys/modules/rdma/krping/Makefile,v 1.1.4.1.2.1 2009/10/25 01:10:29 kensmith Exp $

RDMA= ${.CURDIR}/../../../contrib/rdma/krping
.PATH: ${RDMA}

KMOD= krping
SRCS= krping.c krping_dev.c getopt.c
SRCS+=  bus_if.h device_if.h opt_sched.h pci_if.h pcib_if.h

.include <bsd.kmod.mk>
