# $FreeBSD$

CXGB = ${.CURDIR}/../../dev/cxgb
.PATH: ${CXGB} ${CXGB}/common ${CXGB}/sys

KMOD=	if_cxgb
SRCS=	cxgb_mc5.c cxgb_vsc8211.c cxgb_ael1002.c cxgb_mv88e1xxx.c 
SRCS+=	cxgb_xgmac.c cxgb_vsc7323.c cxgb_t3_hw.c cxgb_main.c 
SRCS+=  cxgb_sge.c cxgb_lro.c cxgb_offload.c cxgb_l2t.c
SRCS+=	device_if.h bus_if.h pci_if.h opt_zero.h 
SRCS+=	uipc_mvec.c

CFLAGS+= -DCONFIG_CHELSIO_T3_CORE -g -DCONFIG_DEFINED -DDEFAULT_JUMBO -I${CXGB}
#CFLAGS+= -DDEBUG -DDEBUG_PRINT
#CFLAGS+= -DINVARIANT_SUPPORT -DINVARIANTS -DWITNESS


.if ${MACHINE_ARCH} != "ia64"
# ld is broken on ia64
t3fw-4.7.0.bin: ${CXGB}/t3fw-4.7.0.bin.gz.uu
	uudecode -p < ${CXGB}/t3fw-4.7.0.bin.gz.uu \
	| gzip -dc > ${.TARGET}

FIRMWS= t3fw-4.7.0.bin:t3fw470
CLEANFILES+= t3fw-4.7.0.bin

t3b_protocol_sram-1.1.0.bin: ${CXGB}/t3b_protocol_sram-1.1.0.bin.gz.uu
	uudecode -p < ${CXGB}/t3b_protocol_sram-1.1.0.bin.gz.uu \
	| gzip -dc > ${.TARGET}

FIRMWS+= t3b_protocol_sram-1.1.0.bin:t3bps110
CLEANFILES+= t3b_protocol_sram-1.1.0.bin

t3b_tp_eeprom-1.1.0.bin: ${CXGB}/t3b_tp_eeprom-1.1.0.bin.gz.uu
	uudecode -p < ${CXGB}/t3b_tp_eeprom-1.1.0.bin.gz.uu \
	| gzip -dc > ${.TARGET}

FIRMWS+= t3b_tp_eeprom-1.1.0.bin:t3btpe110
CLEANFILES+= t3b_tp_eeprom-1.1.0.bin


.endif


.include <bsd.kmod.mk>
