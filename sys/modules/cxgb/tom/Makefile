# $FreeBSD: src/sys/modules/cxgb/tom/Makefile,v 1.5.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $

TOM = ${.CURDIR}/../../../dev/cxgb/ulp/tom
.PATH: ${TOM} 

KMOD=	tom
SRCS=   cxgb_tom.c cxgb_cpl_io.c cxgb_listen.c cxgb_tom_sysctl.c cxgb_cpl_socket.c
SRCS+=  cxgb_ddp.c cxgb_vm.c cxgb_l2t.c cxgb_tcp_offload.c
SRCS+=	opt_compat.h opt_inet.h opt_inet6.h opt_ipsec.h opt_mac.h 
SRCS+=	opt_tcpdebug.h opt_ddb.h opt_sched.h opt_global.h opt_ktr.h
SRCS+=	device_if.h bus_if.h pci_if.h

#CFLAGS+= -DDEBUG_PRINT -DDEBUG
.include <bsd.kmod.mk>
