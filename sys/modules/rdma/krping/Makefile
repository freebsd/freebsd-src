# $FreeBSD: src/sys/modules/rdma/krping/Makefile,v 1.1.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $

RDMA= ${.CURDIR}/../../../contrib/rdma/krping
.PATH: ${RDMA}

KMOD= krping
SRCS= krping.c krping_dev.c getopt.c
SRCS+=  bus_if.h device_if.h opt_sched.h pci_if.h pcib_if.h

.include <bsd.kmod.mk>
