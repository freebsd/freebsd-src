# $FreeBSD$

RDMA= ${.CURDIR}/../../../contrib/rdma/krping
.PATH: ${RDMA}

KMOD= krping
SRCS= krping.c krping_dev.c getopt.c
SRCS+=  bus_if.h device_if.h opt_sched.h pci_if.h pcib_if.h
SRCS+=  vnode_if.h
CFLAGS+= -I${.CURDIR}/../../../ofed/include 

.include <bsd.kmod.mk>
