# $FreeBSD$

.include <bsd.own.mk>

KMOD=	sfxge

SFXGE= ${.CURDIR}/../../dev/sfxge

SRCS=	device_if.h bus_if.h pci_if.h
SRCS+=	opt_inet.h opt_inet6.h opt_sched.h

.PATH: ${.CURDIR}/../../dev/sfxge
SRCS+=	sfxge.c sfxge_dma.c sfxge_ev.c
SRCS+=	sfxge_intr.c sfxge_mcdi.c sfxge_nvram.c
SRCS+=	sfxge_port.c sfxge_rx.c sfxge_tx.c
SRCS+=	sfxge.h sfxge_rx.h sfxge_tx.h sfxge_version.h

.PATH: ${.CURDIR}/../../dev/sfxge/common
SRCS+=	efx_bootcfg.c efx_crc32.c efx_ev.c efx_intr.c efx_lic.c efx_mac.c
SRCS+=	efx_mcdi.c efx_mon.c efx_nic.c
SRCS+=	efx_nvram.c efx_phy.c efx_port.c efx_rx.c efx_sram.c efx_tx.c
SRCS+=	efx_vpd.c efx_wol.c efx_filter.c efx_hash.c
SRCS+=	efsys.h
SRCS+=	efx.h efx_check.h efx_impl.h efx_mcdi.h efx_regs.h efx_regs_ef10.h
SRCS+=	efx_regs_mcdi.h efx_regs_pci.h efx_types.h efx_phy_ids.h
SRCS+=	ef10_tlv_layout.h

SRCS+=	mcdi_mon.c mcdi_mon.h

SRCS+=	siena_mac.c siena_mcdi.c siena_nic.c siena_nvram.c siena_phy.c
SRCS+=	siena_sram.c siena_vpd.c
SRCS+=	siena_flash.h siena_impl.h

SRCS+=	ef10_ev.c ef10_filter.c ef10_intr.c ef10_mac.c ef10_mcdi.c ef10_nic.c
SRCS+=	ef10_nvram.c ef10_phy.c ef10_rx.c ef10_tx.c ef10_vpd.c
SRCS+=	ef10_impl.h

SRCS+=	hunt_nic.c hunt_phy.c
SRCS+=	hunt_impl.h

SRCS+=	medford_nic.c
SRCS+=	medford_impl.h

# Extra debug checks
#CFLAGS += -DDEBUG=1

.if !defined(KERNBUILDDIR)
.if ${MK_INET_SUPPORT} != "no"
opt_inet.h:
	@echo "#define INET 1" > ${.TARGET}
.endif

.if ${MK_INET6_SUPPORT} != "no"
opt_inet6.h:
	@echo "#define INET6 1" > ${.TARGET}
.endif
.endif

.include <bsd.kmod.mk>
