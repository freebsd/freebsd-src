# $FreeBSD$

KMOD=	sfxge

SFXGE= ${.CURDIR}/../../dev/sfxge

SRCS=	device_if.h bus_if.h pci_if.h
SRCS+=	opt_inet.h opt_sched.h

.PATH: ${.CURDIR}/../../dev/sfxge
SRCS+=	sfxge.c sfxge_dma.c sfxge_ev.c
SRCS+=	sfxge_intr.c sfxge_mcdi.c
SRCS+=	sfxge_port.c sfxge_rx.c sfxge_tx.c

.PATH: ${.CURDIR}/../../dev/sfxge/common
SRCS+=	efx_ev.c efx_intr.c efx_mac.c efx_mcdi.c efx_nic.c 
SRCS+=	efx_nvram.c efx_phy.c efx_port.c efx_rx.c efx_sram.c efx_tx.c
SRCS+=	efx_vpd.c efx_wol.c

SRCS+=	siena_mac.c siena_nic.c siena_nvram.c siena_phy.c
SRCS+=	siena_sram.c siena_vpd.c 

DEBUG_FLAGS= -DDEBUG=1

.include <bsd.kmod.mk>
