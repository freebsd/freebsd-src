# $FreeBSD$

CXGB = ${.CURDIR}/../../dev/cxgb
.PATH: ${CXGB} ${CXGB}/common

KMOD=	if_cxgb
SRCS=	cxgb_mc5.c cxgb_vsc8211.c cxgb_ael1002.c cxgb_mv88e1xxx.c 
SRCS+=	cxgb_xgmac.c cxgb_t3_hw.c cxgb_main.c cxgb_sge.c cxgb_lro.c 
SRCS+=	device_if.h bus_if.h pci_if.h

CFLAGS+= -DCONFIG_CHELSIO_T3_CORE -g -DDEFAULT_JUMBO -DCONFIG_DEFINED

.if ${MACHINE_ARCH} != "ia64"
# ld is broken on ia64
t3fw-3.2.bin: ${CXGB}/t3fw-3.2.bin.gz.uu
	uudecode -p < ${CXGB}/t3fw-3.2.bin.gz.uu \
	| gzip -dc > ${.TARGET}

FIRMWS= t3fw-3.2.bin:t3fw32
CLEANFILES+= t3fw-3.2.bin
.endif


.include <bsd.kmod.mk>
