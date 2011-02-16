# $FreeBSD: src/sys/modules/rdma/krping/Makefile,v 1.1.4.1.6.1 2010/12/21 17:09:25 kensmith Exp $

RDMA= ${.CURDIR}/../../../contrib/rdma/krping
.PATH: ${RDMA}

KMOD= krping
SRCS= krping.c krping_dev.c getopt.c
SRCS+=  bus_if.h device_if.h opt_sched.h pci_if.h pcib_if.h

.include <bsd.kmod.mk>
