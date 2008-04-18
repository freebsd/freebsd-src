# $FreeBSD$

CXGB = ${.CURDIR}/../../../dev/cxgb
.PATH: ${CXGB} ${CXGB}/common ${CXGB}/sys

KMOD=	if_cxgb
SRCS=	cxgb_mc5.c cxgb_vsc8211.c cxgb_ael1002.c cxgb_mv88e1xxx.c 
SRCS+=	cxgb_xgmac.c cxgb_vsc7323.c cxgb_t3_hw.c cxgb_main.c 
SRCS+=  cxgb_sge.c cxgb_lro.c cxgb_offload.c 
SRCS+=	device_if.h bus_if.h pci_if.h opt_zero.h 
SRCS+=	uipc_mvec.c cxgb_support.c cxgb_multiq.c

CFLAGS+= -DCONFIG_CHELSIO_T3_CORE -g -DCONFIG_DEFINED -DDEFAULT_JUMBO -I${CXGB}
#CFLAGS+= -DDEBUG -DDEBUG_PRINT
#CFLAGS+= -DINVARIANT_SUPPORT -DINVARIANTS -DWITNESS


.include <bsd.kmod.mk>