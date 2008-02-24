# $FreeBSD: src/sys/modules/nxge/Makefile,v 1.1.2.1 2007/11/02 00:52:34 rwatson Exp $

.PATH: ${.CURDIR}/../../dev/nxge
VPATH = ${.CURDIR}/../../dev/nxge/xgehal

CFLAGS_NXGE = 

# Debugging/Tracing:
# XGE_COMPONENT_HAL_CONFIG      0x1
# XGE_COMPONENT_HAL_FIFO        0x2
# XGE_COMPONENT_HAL_RING        0x4
# XGE_COMPONENT_HAL_CHANNEL     0x8
# XGE_COMPONENT_HAL_DEVICE      0x10
# XGE_COMPONENT_HAL_MM          0x20
# XGE_COMPONENT_HAL_QUEUE       0x40
# XGE_COMPONENT_HAL_STATS       0x100
# XGE_COMPONENT_OSDEP           0x10000000
# XGE_COMPONENT_LL              0x20000000
# XGE_COMPONENT_ALL             0xffffffff

CFLAGS_NXGE  += -DXGE_DEBUG_MODULE_MASK=XGE_COMPONENT_LL
CFLAGS_NXGE  += -DXGE_DEBUG_ERR_MASK=XGE_COMPONENT_LL
#CFLAGS_NXGE  += -DXGE_DEBUG_TRACE_MASK=XGE_COMPONENT_LL

# Check Memory
#CFLAGS_NXGE += -DXGE_OS_MEMORY_CHECK

# 2-Buffer Mode
#CFLAGS_NXGE += -DXGE_FEATURE_BUFFER_MODE_2

CFLAGS += $(CFLAGS_NXGE)

KMOD=   if_nxge
SRCS=   if_nxge.c
SRCS+=  xgehal-device.c xgehal-mm.c xge-queue.c
SRCS+=  xgehal-driver.c xgehal-ring.c xgehal-channel.c
SRCS+=  xgehal-fifo.c xgehal-stats.c xgehal-config.c
SRCS+=  xgehal-mgmt.c
SRCS+=  device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
