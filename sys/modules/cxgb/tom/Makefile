# $FreeBSD$
TOM = ${.CURDIR}/../../../dev/cxgb/ulp/tom
.PATH: ${TOM} 

KMOD=	tom
SRCS=   cxgb_tom.c cxgb_cpl_io.c cxgb_listen.c cxgb_tom_sysctl.c cxgb_cpl_socket.c
SRCS+=	device_if.h bus_if.h pci_if.h 
.include <bsd.kmod.mk>