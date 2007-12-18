# $FreeBSD$
TOM = ${.CURDIR}/../../../dev/cxgb/ulp/tom
.PATH: ${TOM} 

KMOD=	tom
SRCS=   cxgb_tom.c cxgb_cpl_io.c cxgb_listen.c cxgb_tom_sysctl.c cxgb_cpl_socket.c
#SRCS+=  cxgb_tcp_subr.c cxgb_tcp_usrreq.c
SRCS+=	opt_compat.h opt_inet.h opt_inet6.h opt_ipsec.h opt_mac.h opt_tcpdebug.h opt_ddb.h
SRCS+=	device_if.h bus_if.h pci_if.h

#CFLAGS+= -DDEBUG_PRINT -DDEBUG
.include <bsd.kmod.mk>
