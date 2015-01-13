# $FreeBSD$

CXGB = ${.CURDIR}/../../../dev/cxgb
.PATH: ${CXGB}/ulp/tom

KMOD=	t3_tom
SRCS=   cxgb_tom.c cxgb_cpl_io.c cxgb_listen.c cxgb_l2t.c
SRCS+=	opt_compat.h opt_inet.h opt_inet6.h opt_ipsec.h
SRCS+=	opt_tcpdebug.h opt_ddb.h opt_sched.h opt_ktr.h
SRCS+=	device_if.h bus_if.h pci_if.h
CFLAGS+= -g -I${CXGB}

#CFLAGS+= -DDEBUG_PRINT -DDEBUG

.include <bsd.kmod.mk>
