# $FreeBSD: src/sys/modules/cxgb/iw_cxgb/Makefile,v 1.1.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $

CXGB = ${.CURDIR}/../../../dev/cxgb
.PATH: ${IW_CXGB} ${CXGB}/common ${CXGB}/ulp/iw_cxgb

KMOD=   iw_cxgb
SRCS=   iw_cxgb.c iw_cxgb_cm.c iw_cxgb_hal.c 
SRCS+=  iw_cxgb_provider.c iw_cxgb_qp.c iw_cxgb_resource.c
SRCS+=  iw_cxgb_ev.c iw_cxgb_mem.c iw_cxgb_dbg.c iw_cxgb_cq.c
SRCS+=  bus_if.h device_if.h opt_sched.h pci_if.h pcib_if.h opt_ktr.h
CFLAGS+= -DCONFIG_CHELSIO_T3_CORE -g -DCONFIG_DEFINED  -I${CXGB} -DSMP
#CFLAGS+= -DDEBUG

.include <bsd.kmod.mk>
