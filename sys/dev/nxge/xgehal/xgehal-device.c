/*-
 * Copyright (c) 2002-2007 Neterion, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <dev/nxge/include/xgehal-device.h>
#include <dev/nxge/include/xgehal-channel.h>
#include <dev/nxge/include/xgehal-fifo.h>
#include <dev/nxge/include/xgehal-ring.h>
#include <dev/nxge/include/xgehal-driver.h>
#include <dev/nxge/include/xgehal-mgmt.h>

#define SWITCH_SIGN 0xA5A5A5A5A5A5A5A5ULL
#define END_SIGN    0x0

#ifdef XGE_HAL_HERC_EMULATION
#undef XGE_HAL_PROCESS_LINK_INT_IN_ISR
#endif

/*
 * Jenkins hash key length(in bytes)
 */
#define XGE_HAL_JHASH_MSG_LEN 50

/*
 * mix(a,b,c) used in Jenkins hash algorithm
 */
#define mix(a,b,c) { \
	a -= b; a -= c; a ^= (c>>13); \
	b -= c; b -= a; b ^= (a<<8);  \
	c -= a; c -= b; c ^= (b>>13); \
	a -= b; a -= c; a ^= (c>>12); \
	b -= c; b -= a; b ^= (a<<16); \
	c -= a; c -= b; c ^= (b>>5);  \
	a -= b; a -= c; a ^= (c>>3);  \
	b -= c; b -= a; b ^= (a<<10); \
	c -= a; c -= b; c ^= (b>>15); \
}


/*
 * __hal_device_event_queued
 * @data: pointer to xge_hal_device_t structure
 *
 * Will be called when new event succesfully queued.
 */
void
__hal_device_event_queued(void *data, int event_type)
{
	xge_assert(((xge_hal_device_t*)data)->magic == XGE_HAL_MAGIC);
	if (g_xge_hal_driver->uld_callbacks.event_queued) {
	    g_xge_hal_driver->uld_callbacks.event_queued(data, event_type);
	}
}

/*
 * __hal_pio_mem_write32_upper
 *
 * Endiann-aware implementation of xge_os_pio_mem_write32().
 * Since Xframe has 64bit registers, we differintiate uppper and lower
 * parts.
 */
void
__hal_pio_mem_write32_upper(pci_dev_h pdev, pci_reg_h regh, u32 val, void *addr)
{
#if defined(XGE_OS_HOST_BIG_ENDIAN) && !defined(XGE_OS_PIO_LITTLE_ENDIAN)
	xge_os_pio_mem_write32(pdev, regh, val, addr);
#else
	xge_os_pio_mem_write32(pdev, regh, val, (void *)((char *)addr + 4));
#endif
}

/*
 * __hal_pio_mem_write32_upper
 *
 * Endiann-aware implementation of xge_os_pio_mem_write32().
 * Since Xframe has 64bit registers, we differintiate uppper and lower
 * parts.
 */
void
__hal_pio_mem_write32_lower(pci_dev_h pdev, pci_reg_h regh, u32 val,
	                        void *addr)
{
#if defined(XGE_OS_HOST_BIG_ENDIAN) && !defined(XGE_OS_PIO_LITTLE_ENDIAN)
	xge_os_pio_mem_write32(pdev, regh, val,
	                           (void *) ((char *)addr + 4));
#else
	xge_os_pio_mem_write32(pdev, regh, val, addr);
#endif
}

/*
 * __hal_device_register_poll
 * @hldev: pointer to xge_hal_device_t structure
 * @reg: register to poll for
 * @op: 0 - bit reset, 1 - bit set
 * @mask: mask for logical "and" condition based on %op
 * @max_millis: maximum time to try to poll in milliseconds
 *
 * Will poll certain register for specified amount of time.
 * Will poll until masked bit is not cleared.
 */
xge_hal_status_e
__hal_device_register_poll(xge_hal_device_t *hldev, u64 *reg,
	           int op, u64 mask, int max_millis)
{
	u64 val64;
	int i = 0;
	xge_hal_status_e ret = XGE_HAL_FAIL;

	xge_os_udelay(10);

	do {
	    val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0, reg);
	    if (op == 0 && !(val64 & mask))
	        return XGE_HAL_OK;
	    else if (op == 1 && (val64 & mask) == mask)
	        return XGE_HAL_OK;
	    xge_os_udelay(100);
	} while (++i <= 9);

	do {
	    val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0, reg);
	    if (op == 0 && !(val64 & mask))
	        return XGE_HAL_OK;
	    else if (op == 1 && (val64 & mask) == mask)
	        return XGE_HAL_OK;
	    xge_os_udelay(1000);
	} while (++i < max_millis);

	return ret;
}

/*
 * __hal_device_wait_quiescent
 * @hldev: the device
 * @hw_status: hw_status in case of error
 *
 * Will wait until device is quiescent for some blocks.
 */
static xge_hal_status_e
__hal_device_wait_quiescent(xge_hal_device_t *hldev, u64 *hw_status)
{
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;

	/* poll and wait first */
#ifdef XGE_HAL_HERC_EMULATION
	(void) __hal_device_register_poll(hldev, &bar0->adapter_status, 1,
	        (XGE_HAL_ADAPTER_STATUS_TDMA_READY |
	         XGE_HAL_ADAPTER_STATUS_RDMA_READY |
	         XGE_HAL_ADAPTER_STATUS_PFC_READY |
	         XGE_HAL_ADAPTER_STATUS_TMAC_BUF_EMPTY |
	         XGE_HAL_ADAPTER_STATUS_PIC_QUIESCENT |
	         XGE_HAL_ADAPTER_STATUS_MC_DRAM_READY |
	         XGE_HAL_ADAPTER_STATUS_MC_QUEUES_READY |
	         XGE_HAL_ADAPTER_STATUS_M_PLL_LOCK),
	         XGE_HAL_DEVICE_QUIESCENT_WAIT_MAX_MILLIS);
#else
	(void) __hal_device_register_poll(hldev, &bar0->adapter_status, 1,
	        (XGE_HAL_ADAPTER_STATUS_TDMA_READY |
	         XGE_HAL_ADAPTER_STATUS_RDMA_READY |
	         XGE_HAL_ADAPTER_STATUS_PFC_READY |
	         XGE_HAL_ADAPTER_STATUS_TMAC_BUF_EMPTY |
	         XGE_HAL_ADAPTER_STATUS_PIC_QUIESCENT |
	         XGE_HAL_ADAPTER_STATUS_MC_DRAM_READY |
	         XGE_HAL_ADAPTER_STATUS_MC_QUEUES_READY |
	         XGE_HAL_ADAPTER_STATUS_M_PLL_LOCK |
	         XGE_HAL_ADAPTER_STATUS_P_PLL_LOCK),
	         XGE_HAL_DEVICE_QUIESCENT_WAIT_MAX_MILLIS);
#endif

	return xge_hal_device_status(hldev, hw_status);
}

/**
 * xge_hal_device_is_slot_freeze
 * @devh: the device
 *
 * Returns non-zero if the slot is freezed.
 * The determination is made based on the adapter_status
 * register which will never give all FFs, unless PCI read
 * cannot go through.
 */
int
xge_hal_device_is_slot_freeze(xge_hal_device_h devh)
{
	xge_hal_device_t *hldev = (xge_hal_device_t *)devh;
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;
	u16 device_id;
	u64 adapter_status =
	    xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                  &bar0->adapter_status);
	xge_os_pci_read16(hldev->pdev,hldev->cfgh,
	        xge_offsetof(xge_hal_pci_config_le_t, device_id),
	        &device_id);
#ifdef TX_DEBUG
	if (adapter_status == XGE_HAL_ALL_FOXES)
	{
	    u64 dummy;
	    dummy = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                    &bar0->pcc_enable);
	    printf(">>> Slot is frozen!\n");
	    brkpoint(0);
	}
#endif
	return((adapter_status == XGE_HAL_ALL_FOXES) || (device_id == 0xffff));
}


/*
 * __hal_device_led_actifity_fix
 * @hldev: pointer to xge_hal_device_t structure
 *
 * SXE-002: Configure link and activity LED to turn it off
 */
static void
__hal_device_led_actifity_fix(xge_hal_device_t *hldev)
{
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;
	u16 subid;
	u64 val64;

	xge_os_pci_read16(hldev->pdev, hldev->cfgh,
	    xge_offsetof(xge_hal_pci_config_le_t, subsystem_id), &subid);

	/*
	 *  In the case of Herc, there is a new register named beacon control
	 *  is added which was not present in Xena.
	 *  Beacon control register in Herc is at the same offset as
	 *  gpio control register in Xena.  It means they are one and same in
	 *  the case of Xena. Also, gpio control register offset in Herc and
	 *  Xena is different.
	 *  The current register map represents Herc(It means we have
	 *  both beacon  and gpio control registers in register map).
	 *  WRT transition from Xena to Herc, all the code in Xena which was
	 *  using  gpio control register for LED handling would  have to
	 *  use beacon control register in Herc and the rest of the code
	 *  which uses gpio control in Xena  would use the same register
	 *  in Herc.
	 *  WRT LED handling(following code), In the case of Herc, beacon
	 *  control register has to be used. This is applicable for Xena also,
	 *  since it represents the gpio control register in Xena.
	 */
	if ((subid & 0xFF) >= 0x07) {
	    val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                                &bar0->beacon_control);
	    val64 |= 0x0000800000000000ULL;
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	                 val64, &bar0->beacon_control);
	    val64 = 0x0411040400000000ULL;
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	                (void *) ((u8 *)bar0 + 0x2700));
	}
}

/* Constants for Fixing the MacAddress problem seen mostly on
 * Alpha machines.
 */
static u64 xena_fix_mac[] = {
	0x0060000000000000ULL, 0x0060600000000000ULL,
	0x0040600000000000ULL, 0x0000600000000000ULL,
	0x0020600000000000ULL, 0x0060600000000000ULL,
	0x0020600000000000ULL, 0x0060600000000000ULL,
	0x0020600000000000ULL, 0x0060600000000000ULL,
	0x0020600000000000ULL, 0x0060600000000000ULL,
	0x0020600000000000ULL, 0x0060600000000000ULL,
	0x0020600000000000ULL, 0x0060600000000000ULL,
	0x0020600000000000ULL, 0x0060600000000000ULL,
	0x0020600000000000ULL, 0x0060600000000000ULL,
	0x0020600000000000ULL, 0x0060600000000000ULL,
	0x0020600000000000ULL, 0x0060600000000000ULL,
	0x0020600000000000ULL, 0x0000600000000000ULL,
	0x0040600000000000ULL, 0x0060600000000000ULL,
	END_SIGN
};

/*
 * __hal_device_fix_mac
 * @hldev: HAL device handle.
 *
 * Fix for all "FFs" MAC address problems observed on Alpha platforms.
 */
static void
__hal_device_xena_fix_mac(xge_hal_device_t *hldev)
{
	int i = 0;
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;

	/*
	 *  In the case of Herc, there is a new register named beacon control
	 *  is added which was not present in Xena.
	 *  Beacon control register in Herc is at the same offset as
	 *  gpio control register in Xena.  It means they are one and same in
	 *  the case of Xena. Also, gpio control register offset in Herc and
	 *  Xena is different.
	 *  The current register map represents Herc(It means we have
	 *  both beacon  and gpio control registers in register map).
	 *  WRT transition from Xena to Herc, all the code in Xena which was
	 *  using  gpio control register for LED handling would  have to
	 *  use beacon control register in Herc and the rest of the code
	 *  which uses gpio control in Xena  would use the same register
	 *  in Herc.
	 *  In the following code(xena_fix_mac), beacon control register has
	 *  to be used in the case of Xena, since it represents gpio control
	 *  register. In the case of Herc, there is no change required.
	 */
	while (xena_fix_mac[i] != END_SIGN) {
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	            xena_fix_mac[i++], &bar0->beacon_control);
	    xge_os_mdelay(1);
	}
}

/*
 * xge_hal_device_bcast_enable
 * @hldev: HAL device handle.
 *
 * Enable receiving broadcasts.
 * The host must first write RMAC_CFG_KEY "key"
 * register, and then - MAC_CFG register.
 */
void
xge_hal_device_bcast_enable(xge_hal_device_h devh)
{
	xge_hal_device_t *hldev = (xge_hal_device_t *)devh;
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;
	u64 val64;

	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	&bar0->mac_cfg);
	    val64 |= XGE_HAL_MAC_RMAC_BCAST_ENABLE;

	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	    XGE_HAL_RMAC_CFG_KEY(0x4C0D), &bar0->rmac_cfg_key);

	__hal_pio_mem_write32_upper(hldev->pdev, hldev->regh0,
	    (u32)(val64 >> 32), &bar0->mac_cfg);

	xge_debug_device(XGE_TRACE, "mac_cfg 0x"XGE_OS_LLXFMT": broadcast %s",
	    (unsigned long long)val64,
	    hldev->config.mac.rmac_bcast_en ? "enabled" : "disabled");
}

/*
 * xge_hal_device_bcast_disable
 * @hldev: HAL device handle.
 *
 * Disable receiving broadcasts.
 * The host must first write RMAC_CFG_KEY "key"
 * register, and then - MAC_CFG register.
 */
void
xge_hal_device_bcast_disable(xge_hal_device_h devh)
{
	xge_hal_device_t *hldev = (xge_hal_device_t *)devh;
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;
	u64 val64;

	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	&bar0->mac_cfg);

	val64 &= ~(XGE_HAL_MAC_RMAC_BCAST_ENABLE);
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	         XGE_HAL_RMAC_CFG_KEY(0x4C0D), &bar0->rmac_cfg_key);

	    __hal_pio_mem_write32_upper(hldev->pdev, hldev->regh0,
	        (u32)(val64 >> 32), &bar0->mac_cfg);

	xge_debug_device(XGE_TRACE, "mac_cfg 0x"XGE_OS_LLXFMT": broadcast %s",
	    (unsigned long long)val64,
	    hldev->config.mac.rmac_bcast_en ? "enabled" : "disabled");
}

/*
 * __hal_device_shared_splits_configure
 * @hldev: HAL device handle.
 *
 * TxDMA will stop Read request if the number of read split had exceeded
 * the limit set by shared_splits
 */
static void
__hal_device_shared_splits_configure(xge_hal_device_t *hldev)
{
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;
	u64 val64;

	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                            &bar0->pic_control);
	val64 |=
	XGE_HAL_PIC_CNTL_SHARED_SPLITS(hldev->config.shared_splits);
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	             &bar0->pic_control);
	xge_debug_device(XGE_TRACE, "%s", "shared splits configured");
}

/*
 * __hal_device_rmac_padding_configure
 * @hldev: HAL device handle.
 *
 * Configure RMAC frame padding. Depends on configuration, it
 * can be send to host or removed by MAC.
 */
static void
__hal_device_rmac_padding_configure(xge_hal_device_t *hldev)
{
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;
	u64 val64;

	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	        XGE_HAL_RMAC_CFG_KEY(0x4C0D), &bar0->rmac_cfg_key);
	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	&bar0->mac_cfg);
	val64 &= ( ~XGE_HAL_MAC_RMAC_ALL_ADDR_ENABLE );
	val64 &= ( ~XGE_HAL_MAC_CFG_RMAC_PROM_ENABLE );
	val64 |= XGE_HAL_MAC_CFG_TMAC_APPEND_PAD;

	/*
	 * If the RTH enable bit is not set, strip the FCS
	 */
	if (!hldev->config.rth_en ||
	    !(xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	           &bar0->rts_rth_cfg) & XGE_HAL_RTS_RTH_EN)) {
	    val64 |= XGE_HAL_MAC_CFG_RMAC_STRIP_FCS;
	}

	val64 &= ( ~XGE_HAL_MAC_CFG_RMAC_STRIP_PAD );
	val64 |= XGE_HAL_MAC_RMAC_DISCARD_PFRM;

	__hal_pio_mem_write32_upper(hldev->pdev, hldev->regh0,
	        (u32)(val64 >> 32), (char*)&bar0->mac_cfg);
	xge_os_mdelay(1);

	xge_debug_device(XGE_TRACE,
	      "mac_cfg 0x"XGE_OS_LLXFMT": frame padding configured",
	      (unsigned long long)val64);
}

/*
 * __hal_device_pause_frames_configure
 * @hldev: HAL device handle.
 *
 * Set Pause threshold.
 *
 * Pause frame is generated if the amount of data outstanding
 * on any queue exceeded the ratio of
 * (mac_control.mc_pause_threshold_q0q3 or q4q7)/256
 */
static void
__hal_device_pause_frames_configure(xge_hal_device_t *hldev)
{
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;
	int i;
	u64 val64;

	switch (hldev->config.mac.media) {
	    case XGE_HAL_MEDIA_SR:
	    case XGE_HAL_MEDIA_SW:
	        val64=0xfffbfffbfffbfffbULL;
	        break;
	    case XGE_HAL_MEDIA_LR:
	    case XGE_HAL_MEDIA_LW:
	        val64=0xffbbffbbffbbffbbULL;
	        break;
	    case XGE_HAL_MEDIA_ER:
	    case XGE_HAL_MEDIA_EW:
	    default:
	        val64=0xffbbffbbffbbffbbULL;
	        break;
	}

	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	        val64, &bar0->mc_pause_thresh_q0q3);
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	        val64, &bar0->mc_pause_thresh_q4q7);

	/* Set the time value  to be inserted in the pause frame generated
	 * by Xframe */
	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                            &bar0->rmac_pause_cfg);
	if (hldev->config.mac.rmac_pause_gen_en)
	    val64 |= XGE_HAL_RMAC_PAUSE_GEN_EN;
	else
	    val64 &= ~(XGE_HAL_RMAC_PAUSE_GEN_EN);
	if (hldev->config.mac.rmac_pause_rcv_en)
	    val64 |= XGE_HAL_RMAC_PAUSE_RCV_EN;
	else
	    val64 &= ~(XGE_HAL_RMAC_PAUSE_RCV_EN);
	val64 &= ~(XGE_HAL_RMAC_PAUSE_HG_PTIME(0xffff));
	val64 |= XGE_HAL_RMAC_PAUSE_HG_PTIME(hldev->config.mac.rmac_pause_time);
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	             &bar0->rmac_pause_cfg);

	val64 = 0;
	for (i = 0; i<4; i++) {
	    val64 |=
	         (((u64)0xFF00|hldev->config.mac.mc_pause_threshold_q0q3)
	                        <<(i*2*8));
	}
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	             &bar0->mc_pause_thresh_q0q3);

	val64 = 0;
	for (i = 0; i<4; i++) {
	    val64 |=
	         (((u64)0xFF00|hldev->config.mac.mc_pause_threshold_q4q7)
	                        <<(i*2*8));
	}
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	             &bar0->mc_pause_thresh_q4q7);
	xge_debug_device(XGE_TRACE, "%s", "pause frames configured");
}

/*
 * Herc's clock rate doubled, unless the slot is 33MHz.
 */
unsigned int __hal_fix_time_ival_herc(xge_hal_device_t *hldev,
	                  unsigned int time_ival)
{
	if (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_XENA)
	    return time_ival;

	xge_assert(xge_hal_device_check_id(hldev) == XGE_HAL_CARD_HERC);

	if (hldev->bus_frequency != XGE_HAL_PCI_BUS_FREQUENCY_UNKNOWN &&
	    hldev->bus_frequency != XGE_HAL_PCI_BUS_FREQUENCY_33MHZ)
	    time_ival *= 2;

	return time_ival;
}


/*
 * __hal_device_bus_master_disable
 * @hldev: HAL device handle.
 *
 * Disable bus mastership.
 */
static void
__hal_device_bus_master_disable (xge_hal_device_t *hldev)
{
	u16 cmd;
	u16 bus_master = 4;

	xge_os_pci_read16(hldev->pdev, hldev->cfgh,
	        xge_offsetof(xge_hal_pci_config_le_t, command), &cmd);
	cmd &= ~bus_master;
	xge_os_pci_write16(hldev->pdev, hldev->cfgh,
	         xge_offsetof(xge_hal_pci_config_le_t, command), cmd);
}

/*
 * __hal_device_bus_master_enable
 * @hldev: HAL device handle.
 *
 * Disable bus mastership.
 */
static void
__hal_device_bus_master_enable (xge_hal_device_t *hldev)
{
	u16 cmd;
	u16 bus_master = 4;

	xge_os_pci_read16(hldev->pdev, hldev->cfgh,
	        xge_offsetof(xge_hal_pci_config_le_t, command), &cmd);

	/* already enabled? do nothing */
	if (cmd & bus_master)
	    return;

	cmd |= bus_master;
	xge_os_pci_write16(hldev->pdev, hldev->cfgh,
	         xge_offsetof(xge_hal_pci_config_le_t, command), cmd);
}
/*
 * __hal_device_intr_mgmt
 * @hldev: HAL device handle.
 * @mask: mask indicating which Intr block must be modified.
 * @flag: if true - enable, otherwise - disable interrupts.
 *
 * Disable or enable device interrupts. Mask is used to specify
 * which hardware blocks should produce interrupts. For details
 * please refer to Xframe User Guide.
 */
static void
__hal_device_intr_mgmt(xge_hal_device_t *hldev, u64 mask, int flag)
{
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;
	u64 val64 = 0, temp64 = 0;
	u64 gim, gim_saved;

	gim_saved = gim = xge_os_pio_mem_read64(hldev->pdev,
	                          hldev->regh0, &bar0->general_int_mask);

	/* Top level interrupt classification */
	/* PIC Interrupts */
	if ((mask & (XGE_HAL_TX_PIC_INTR/* | XGE_HAL_RX_PIC_INTR*/))) {
	    /* Enable PIC Intrs in the general intr mask register */
	    val64 = XGE_HAL_TXPIC_INT_M/* | XGE_HAL_PIC_RX_INT_M*/;
	    if (flag) {
	        gim &= ~((u64) val64);
	        temp64 = xge_os_pio_mem_read64(hldev->pdev,
	                hldev->regh0, &bar0->pic_int_mask);

	        temp64 &= ~XGE_HAL_PIC_INT_TX;
#ifdef  XGE_HAL_PROCESS_LINK_INT_IN_ISR
	        if (xge_hal_device_check_id(hldev) ==
	                        XGE_HAL_CARD_HERC) {
	            temp64 &= ~XGE_HAL_PIC_INT_MISC;
	        }
#endif
	        xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	                     temp64, &bar0->pic_int_mask);
#ifdef  XGE_HAL_PROCESS_LINK_INT_IN_ISR
	        if (xge_hal_device_check_id(hldev) ==
	                        XGE_HAL_CARD_HERC) {
	            /*
	             * Unmask only Link Up interrupt
	             */
	            temp64 = xge_os_pio_mem_read64(hldev->pdev,
	                hldev->regh0, &bar0->misc_int_mask);
	            temp64 &= ~XGE_HAL_MISC_INT_REG_LINK_UP_INT;
	            xge_os_pio_mem_write64(hldev->pdev,
	                      hldev->regh0, temp64,
	                      &bar0->misc_int_mask);
	            xge_debug_device(XGE_TRACE,
	                "unmask link up flag "XGE_OS_LLXFMT,
	                (unsigned long long)temp64);
	        }
#endif
	    } else { /* flag == 0 */

#ifdef  XGE_HAL_PROCESS_LINK_INT_IN_ISR
	        if (xge_hal_device_check_id(hldev) ==
	                        XGE_HAL_CARD_HERC) {
	            /*
	             * Mask both Link Up and Down interrupts
	             */
	            temp64 = xge_os_pio_mem_read64(hldev->pdev,
	                hldev->regh0, &bar0->misc_int_mask);
	            temp64 |= XGE_HAL_MISC_INT_REG_LINK_UP_INT;
	            temp64 |= XGE_HAL_MISC_INT_REG_LINK_DOWN_INT;
	            xge_os_pio_mem_write64(hldev->pdev,
	                      hldev->regh0, temp64,
	                      &bar0->misc_int_mask);
	            xge_debug_device(XGE_TRACE,
	                "mask link up/down flag "XGE_OS_LLXFMT,
	                (unsigned long long)temp64);
	        }
#endif
	        /* Disable PIC Intrs in the general intr mask
	         * register */
	        xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	                     XGE_HAL_ALL_INTRS_DIS,
	                             &bar0->pic_int_mask);
	        gim |= val64;
	    }
	}

	/*  DMA Interrupts */
	/*  Enabling/Disabling Tx DMA interrupts */
	if (mask & XGE_HAL_TX_DMA_INTR) {
	    /*  Enable TxDMA Intrs in the general intr mask register */
	    val64 = XGE_HAL_TXDMA_INT_M;
	    if (flag) {
	        gim &= ~((u64) val64);
	        /* Enable all TxDMA interrupts */
	        xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	                     0x0, &bar0->txdma_int_mask);
	        xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	                     0x0, &bar0->pfc_err_mask);
	        xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	                     0x0, &bar0->tda_err_mask);
	        xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	                     0x0, &bar0->pcc_err_mask);
	        xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	                     0x0, &bar0->tti_err_mask);
	        xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	                     0x0, &bar0->lso_err_mask);
	        xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	                     0x0, &bar0->tpa_err_mask);
	        xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	                     0x0, &bar0->sm_err_mask);

	    } else { /* flag == 0 */

	        /*  Disable TxDMA Intrs in the general intr mask
	         *  register */
	        xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	                     XGE_HAL_ALL_INTRS_DIS,
	                             &bar0->txdma_int_mask);
	        xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	                     XGE_HAL_ALL_INTRS_DIS,
	                             &bar0->pfc_err_mask);

	        gim |= val64;
	    }
	}

	/*  Enabling/Disabling Rx DMA interrupts */
	if (mask & XGE_HAL_RX_DMA_INTR) {
	    /*  Enable RxDMA Intrs in the general intr mask register */
	    val64 = XGE_HAL_RXDMA_INT_M;
	    if (flag) {

	        gim &= ~((u64) val64);
	        /* All RxDMA block interrupts are disabled for now
	         * TODO */
	        xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	                     XGE_HAL_ALL_INTRS_DIS,
	                             &bar0->rxdma_int_mask);

	    } else { /* flag == 0 */

	        /*  Disable RxDMA Intrs in the general intr mask
	         *  register */
	        xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	                     XGE_HAL_ALL_INTRS_DIS,
	                             &bar0->rxdma_int_mask);

	        gim |= val64;
	    }
	}

	/*  MAC Interrupts */
	/*  Enabling/Disabling MAC interrupts */
	if (mask & (XGE_HAL_TX_MAC_INTR | XGE_HAL_RX_MAC_INTR)) {
	    val64 = XGE_HAL_TXMAC_INT_M | XGE_HAL_RXMAC_INT_M;
	    if (flag) {

	        gim &= ~((u64) val64);

	        /* All MAC block error inter. are disabled for now. */
	        xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	             XGE_HAL_ALL_INTRS_DIS, &bar0->mac_int_mask);
	        xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	             XGE_HAL_ALL_INTRS_DIS, &bar0->mac_rmac_err_mask);

	    } else { /* flag == 0 */

	        /* Disable MAC Intrs in the general intr mask
	         * register */
	        xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	             XGE_HAL_ALL_INTRS_DIS, &bar0->mac_int_mask);
	        xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	             XGE_HAL_ALL_INTRS_DIS, &bar0->mac_rmac_err_mask);

	        gim |= val64;
	    }
	}

	/*  XGXS Interrupts */
	if (mask & (XGE_HAL_TX_XGXS_INTR | XGE_HAL_RX_XGXS_INTR)) {
	    val64 = XGE_HAL_TXXGXS_INT_M | XGE_HAL_RXXGXS_INT_M;
	    if (flag) {

	        gim &= ~((u64) val64);
	        /* All XGXS block error interrupts are disabled for now
	         * TODO */
	        xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	             XGE_HAL_ALL_INTRS_DIS, &bar0->xgxs_int_mask);

	    } else { /* flag == 0 */

	        /* Disable MC Intrs in the general intr mask register */
	        xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	            XGE_HAL_ALL_INTRS_DIS, &bar0->xgxs_int_mask);

	        gim |= val64;
	    }
	}

	/*  Memory Controller(MC) interrupts */
	if (mask & XGE_HAL_MC_INTR) {
	    val64 = XGE_HAL_MC_INT_M;
	    if (flag) {

	        gim &= ~((u64) val64);

	        /* Enable all MC blocks error interrupts */
	        xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	                 0x0ULL, &bar0->mc_int_mask);

	    } else { /* flag == 0 */

	        /* Disable MC Intrs in the general intr mask
	         * register */
	        xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	                 XGE_HAL_ALL_INTRS_DIS, &bar0->mc_int_mask);

	        gim |= val64;
	    }
	}


	/*  Tx traffic interrupts */
	if (mask & XGE_HAL_TX_TRAFFIC_INTR) {
	    val64 = XGE_HAL_TXTRAFFIC_INT_M;
	    if (flag) {

	        gim &= ~((u64) val64);

	        /* Enable all the Tx side interrupts */
	        /* '0' Enables all 64 TX interrupt levels. */
	        xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, 0x0,
	                            &bar0->tx_traffic_mask);

	    } else { /* flag == 0 */

	        /* Disable Tx Traffic Intrs in the general intr mask
	         * register. */
	        xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	                             XGE_HAL_ALL_INTRS_DIS,
	                             &bar0->tx_traffic_mask);
	        gim |= val64;
	    }
	}

	/*  Rx traffic interrupts */
	if (mask & XGE_HAL_RX_TRAFFIC_INTR) {
	    val64 = XGE_HAL_RXTRAFFIC_INT_M;
	    if (flag) {
	        gim &= ~((u64) val64);
	        /* '0' Enables all 8 RX interrupt levels. */
	        xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, 0x0,
	                            &bar0->rx_traffic_mask);

	    } else { /* flag == 0 */

	        /* Disable Rx Traffic Intrs in the general intr mask
	         * register.
	         */
	        xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	                             XGE_HAL_ALL_INTRS_DIS,
	                             &bar0->rx_traffic_mask);

	        gim |= val64;
	    }
	}

	/* Sched Timer interrupt */
	if (mask & XGE_HAL_SCHED_INTR) {
	    if (flag) {
	        temp64 = xge_os_pio_mem_read64(hldev->pdev,
	                hldev->regh0, &bar0->txpic_int_mask);
	        temp64 &= ~XGE_HAL_TXPIC_INT_SCHED_INTR;
	        xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	                temp64, &bar0->txpic_int_mask);

	        xge_hal_device_sched_timer(hldev,
	                hldev->config.sched_timer_us,
	                hldev->config.sched_timer_one_shot);
	    } else {
	        temp64 = xge_os_pio_mem_read64(hldev->pdev,
	                hldev->regh0, &bar0->txpic_int_mask);
	        temp64 |= XGE_HAL_TXPIC_INT_SCHED_INTR;

	        xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	                temp64, &bar0->txpic_int_mask);

	        xge_hal_device_sched_timer(hldev,
	                XGE_HAL_SCHED_TIMER_DISABLED,
	                XGE_HAL_SCHED_TIMER_ON_SHOT_ENABLE);
	    }
	}

	if (gim != gim_saved) {
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, gim,
	        &bar0->general_int_mask);
	    xge_debug_device(XGE_TRACE, "general_int_mask updated "
	         XGE_OS_LLXFMT" => "XGE_OS_LLXFMT,
	        (unsigned long long)gim_saved, (unsigned long long)gim);
	}
}

/*
 * __hal_device_bimodal_configure
 * @hldev: HAL device handle.
 *
 * Bimodal parameters initialization.
 */
static void
__hal_device_bimodal_configure(xge_hal_device_t *hldev)
{
	int i;

	for (i=0; i<XGE_HAL_MAX_RING_NUM; i++) {
	    xge_hal_tti_config_t *tti;
	    xge_hal_rti_config_t *rti;

	    if (!hldev->config.ring.queue[i].configured)
	        continue;
	    rti = &hldev->config.ring.queue[i].rti;
	    tti = &hldev->bimodal_tti[i];

	    tti->enabled = 1;
	    tti->urange_a = hldev->bimodal_urange_a_en * 10;
	    tti->urange_b = 20;
	    tti->urange_c = 30;
	    tti->ufc_a = hldev->bimodal_urange_a_en * 8;
	    tti->ufc_b = 16;
	    tti->ufc_c = 32;
	    tti->ufc_d = 64;
	    tti->timer_val_us = hldev->bimodal_timer_val_us;
	    tti->timer_ac_en = 1;
	    tti->timer_ci_en = 0;

	    rti->urange_a = 10;
	    rti->urange_b = 20;
	    rti->urange_c = 30;
	    rti->ufc_a = 1; /* <= for netpipe type of tests */
	    rti->ufc_b = 4;
	    rti->ufc_c = 4;
	    rti->ufc_d = 4; /* <= 99% of a bandwidth traffic counts here */
	    rti->timer_ac_en = 1;
	    rti->timer_val_us = 5; /* for optimal bus efficiency usage */
	}
}

/*
 * __hal_device_tti_apply
 * @hldev: HAL device handle.
 *
 * apply TTI configuration.
 */
static xge_hal_status_e
__hal_device_tti_apply(xge_hal_device_t *hldev, xge_hal_tti_config_t *tti,
	           int num, int runtime)
{
	u64 val64, data1 = 0, data2 = 0;
	xge_hal_pci_bar0_t *bar0;

	if (runtime)
	    bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->isrbar0;
	else
	    bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;

	if (tti->timer_val_us) {
	    unsigned int tx_interval;

	    if (hldev->config.pci_freq_mherz) {
	        tx_interval = hldev->config.pci_freq_mherz *
	                tti->timer_val_us / 64;
	        tx_interval =
	            __hal_fix_time_ival_herc(hldev,
	                         tx_interval);
	    } else {
	        tx_interval = tti->timer_val_us;
	    }
	    data1 |= XGE_HAL_TTI_DATA1_MEM_TX_TIMER_VAL(tx_interval);
	    if (tti->timer_ac_en) {
	        data1 |= XGE_HAL_TTI_DATA1_MEM_TX_TIMER_AC_EN;
	    }
	    if (tti->timer_ci_en) {
	        data1 |= XGE_HAL_TTI_DATA1_MEM_TX_TIMER_CI_EN;
	    }

	    if (!runtime) {
	        xge_debug_device(XGE_TRACE, "TTI[%d] timer enabled to %d, ci %s",
	              num, tx_interval, tti->timer_ci_en ?
	              "enabled": "disabled");
	    }
	}

	if (tti->urange_a ||
	    tti->urange_b ||
	    tti->urange_c ||
	    tti->ufc_a ||
	    tti->ufc_b ||
	    tti->ufc_c ||
	    tti->ufc_d ) {
	    data1 |= XGE_HAL_TTI_DATA1_MEM_TX_URNG_A(tti->urange_a) |
	         XGE_HAL_TTI_DATA1_MEM_TX_URNG_B(tti->urange_b) |
	         XGE_HAL_TTI_DATA1_MEM_TX_URNG_C(tti->urange_c);

	    data2 |= XGE_HAL_TTI_DATA2_MEM_TX_UFC_A(tti->ufc_a) |
	         XGE_HAL_TTI_DATA2_MEM_TX_UFC_B(tti->ufc_b) |
	         XGE_HAL_TTI_DATA2_MEM_TX_UFC_C(tti->ufc_c) |
	         XGE_HAL_TTI_DATA2_MEM_TX_UFC_D(tti->ufc_d);
	}

	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, data1,
	             &bar0->tti_data1_mem);
	(void)xge_os_pio_mem_read64(hldev->pdev,
	      hldev->regh0, &bar0->tti_data1_mem);
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, data2,
	             &bar0->tti_data2_mem);
	(void)xge_os_pio_mem_read64(hldev->pdev,
	      hldev->regh0, &bar0->tti_data2_mem);
	xge_os_wmb();

	val64 = XGE_HAL_TTI_CMD_MEM_WE | XGE_HAL_TTI_CMD_MEM_STROBE_NEW_CMD |
	      XGE_HAL_TTI_CMD_MEM_OFFSET(num);
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	    &bar0->tti_command_mem);

	if (!runtime && __hal_device_register_poll(hldev, &bar0->tti_command_mem,
	       0, XGE_HAL_TTI_CMD_MEM_STROBE_NEW_CMD,
	       XGE_HAL_DEVICE_CMDMEM_WAIT_MAX_MILLIS) != XGE_HAL_OK) {
	    /* upper layer may require to repeat */
	    return XGE_HAL_INF_MEM_STROBE_CMD_EXECUTING;
	}

	if (!runtime) {
	    xge_debug_device(XGE_TRACE, "TTI[%d] configured: tti_data1_mem 0x"
	       XGE_OS_LLXFMT, num,
	       (unsigned long long)xge_os_pio_mem_read64(hldev->pdev,
	       hldev->regh0, &bar0->tti_data1_mem));
	}

	return XGE_HAL_OK;
}

/*
 * __hal_device_tti_configure
 * @hldev: HAL device handle.
 *
 * TTI Initialization.
 * Initialize Transmit Traffic Interrupt Scheme.
 */
static xge_hal_status_e
__hal_device_tti_configure(xge_hal_device_t *hldev, int runtime)
{
	int i;

	for (i=0; i<XGE_HAL_MAX_FIFO_NUM; i++) {
	    int j;

	    if (!hldev->config.fifo.queue[i].configured)
	        continue;

	    for (j=0; j<XGE_HAL_MAX_FIFO_TTI_NUM; j++) {
	        xge_hal_status_e status;

	        if (!hldev->config.fifo.queue[i].tti[j].enabled)
	            continue;

	        /* at least some TTI enabled. Record it. */
	        hldev->tti_enabled = 1;

	        status = __hal_device_tti_apply(hldev,
	            &hldev->config.fifo.queue[i].tti[j],
	            i * XGE_HAL_MAX_FIFO_TTI_NUM + j, runtime);
	        if (status != XGE_HAL_OK)
	            return status;
	    }
	}

	/* processing bimodal TTIs */
	for (i=0; i<XGE_HAL_MAX_RING_NUM; i++) {
	    xge_hal_status_e status;

	    if (!hldev->bimodal_tti[i].enabled)
	        continue;

	    /* at least some bimodal TTI enabled. Record it. */
	    hldev->tti_enabled = 1;

	    status = __hal_device_tti_apply(hldev, &hldev->bimodal_tti[i],
	            XGE_HAL_MAX_FIFO_TTI_RING_0 + i, runtime);
	    if (status != XGE_HAL_OK)
	        return status;

	}

	return XGE_HAL_OK;
}

/*
 * __hal_device_rti_configure
 * @hldev: HAL device handle.
 *
 * RTI Initialization.
 * Initialize Receive Traffic Interrupt Scheme.
 */
xge_hal_status_e
__hal_device_rti_configure(xge_hal_device_t *hldev, int runtime)
{
	xge_hal_pci_bar0_t *bar0;
	u64 val64, data1 = 0, data2 = 0;
	int i;

	if (runtime) {
	    /*
	     * we don't want to re-configure RTI in case when
	     * bimodal interrupts are in use. Instead reconfigure TTI
	     * with new RTI values.
	     */
	    if (hldev->config.bimodal_interrupts) {
	        __hal_device_bimodal_configure(hldev);
	        return __hal_device_tti_configure(hldev, 1);
	    }
	    bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->isrbar0;
	} else
	    bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;

	for (i=0; i<XGE_HAL_MAX_RING_NUM; i++) {
	    xge_hal_rti_config_t *rti = &hldev->config.ring.queue[i].rti;

	    if (!hldev->config.ring.queue[i].configured)
	        continue;

	    if (rti->timer_val_us) {
	        unsigned int rx_interval;

	        if (hldev->config.pci_freq_mherz) {
	            rx_interval = hldev->config.pci_freq_mherz *
	                    rti->timer_val_us / 8;
	            rx_interval =
	                __hal_fix_time_ival_herc(hldev,
	                             rx_interval);
	        } else {
	            rx_interval = rti->timer_val_us;
	        }
	        data1 |=XGE_HAL_RTI_DATA1_MEM_RX_TIMER_VAL(rx_interval);
	        if (rti->timer_ac_en) {
	            data1 |= XGE_HAL_RTI_DATA1_MEM_RX_TIMER_AC_EN;
	        }
	        data1 |= XGE_HAL_RTI_DATA1_MEM_RX_TIMER_CI_EN;
	    }

	    if (rti->urange_a ||
	        rti->urange_b ||
	        rti->urange_c ||
	        rti->ufc_a ||
	        rti->ufc_b ||
	        rti->ufc_c ||
	        rti->ufc_d) {
	        data1 |=XGE_HAL_RTI_DATA1_MEM_RX_URNG_A(rti->urange_a) |
	            XGE_HAL_RTI_DATA1_MEM_RX_URNG_B(rti->urange_b) |
	            XGE_HAL_RTI_DATA1_MEM_RX_URNG_C(rti->urange_c);

	        data2 |= XGE_HAL_RTI_DATA2_MEM_RX_UFC_A(rti->ufc_a) |
	             XGE_HAL_RTI_DATA2_MEM_RX_UFC_B(rti->ufc_b) |
	             XGE_HAL_RTI_DATA2_MEM_RX_UFC_C(rti->ufc_c) |
	             XGE_HAL_RTI_DATA2_MEM_RX_UFC_D(rti->ufc_d);
	    }

	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, data1,
	                 &bar0->rti_data1_mem);
	    (void)xge_os_pio_mem_read64(hldev->pdev,
	          hldev->regh0, &bar0->rti_data1_mem);
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, data2,
	                     &bar0->rti_data2_mem);
	    (void)xge_os_pio_mem_read64(hldev->pdev,
	          hldev->regh0, &bar0->rti_data2_mem);
	    xge_os_wmb();

	    val64 = XGE_HAL_RTI_CMD_MEM_WE |
	    XGE_HAL_RTI_CMD_MEM_STROBE_NEW_CMD;
	    val64 |= XGE_HAL_RTI_CMD_MEM_OFFSET(i);
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	                        &bar0->rti_command_mem);

	    if (!runtime && __hal_device_register_poll(hldev,
	        &bar0->rti_command_mem, 0,
	        XGE_HAL_RTI_CMD_MEM_STROBE_NEW_CMD,
	        XGE_HAL_DEVICE_CMDMEM_WAIT_MAX_MILLIS) != XGE_HAL_OK) {
	        /* upper layer may require to repeat */
	        return XGE_HAL_INF_MEM_STROBE_CMD_EXECUTING;
	    }

	    if (!runtime) {
	        xge_debug_device(XGE_TRACE,
	          "RTI[%d] configured: rti_data1_mem 0x"XGE_OS_LLXFMT,
	          i,
	          (unsigned long long)xge_os_pio_mem_read64(hldev->pdev,
	                  hldev->regh0, &bar0->rti_data1_mem));
	    }
	}

	return XGE_HAL_OK;
}


/* Constants to be programmed into the Xena's registers to configure
 * the XAUI. */
static u64 default_xena_mdio_cfg[] = {
	/* Reset PMA PLL */
	0xC001010000000000ULL, 0xC0010100000000E0ULL,
	0xC0010100008000E4ULL,
	/* Remove Reset from PMA PLL */
	0xC001010000000000ULL, 0xC0010100000000E0ULL,
	0xC0010100000000E4ULL,
	END_SIGN
};

static u64 default_herc_mdio_cfg[] = {
	END_SIGN
};

static u64 default_xena_dtx_cfg[] = {
	0x8000051500000000ULL, 0x80000515000000E0ULL,
	0x80000515D93500E4ULL, 0x8001051500000000ULL,
	0x80010515000000E0ULL, 0x80010515001E00E4ULL,
	0x8002051500000000ULL, 0x80020515000000E0ULL,
	0x80020515F21000E4ULL,
	/* Set PADLOOPBACKN */
	0x8002051500000000ULL, 0x80020515000000E0ULL,
	0x80020515B20000E4ULL, 0x8003051500000000ULL,
	0x80030515000000E0ULL, 0x80030515B20000E4ULL,
	0x8004051500000000ULL, 0x80040515000000E0ULL,
	0x80040515B20000E4ULL, 0x8005051500000000ULL,
	0x80050515000000E0ULL, 0x80050515B20000E4ULL,
	SWITCH_SIGN,
	/* Remove PADLOOPBACKN */
	0x8002051500000000ULL, 0x80020515000000E0ULL,
	0x80020515F20000E4ULL, 0x8003051500000000ULL,
	0x80030515000000E0ULL, 0x80030515F20000E4ULL,
	0x8004051500000000ULL, 0x80040515000000E0ULL,
	0x80040515F20000E4ULL, 0x8005051500000000ULL,
	0x80050515000000E0ULL, 0x80050515F20000E4ULL,
	END_SIGN
};

/*
static u64 default_herc_dtx_cfg[] = {
	0x80000515BA750000ULL, 0x80000515BA7500E0ULL,
	0x80000515BA750004ULL, 0x80000515BA7500E4ULL,
	0x80010515003F0000ULL, 0x80010515003F00E0ULL,
	0x80010515003F0004ULL, 0x80010515003F00E4ULL,
	0x80020515F2100000ULL, 0x80020515F21000E0ULL,
	0x80020515F2100004ULL, 0x80020515F21000E4ULL,
	END_SIGN
};
*/

static u64 default_herc_dtx_cfg[] = {
	0x8000051536750000ULL, 0x80000515367500E0ULL,
	0x8000051536750004ULL, 0x80000515367500E4ULL,

	0x80010515003F0000ULL, 0x80010515003F00E0ULL,
	0x80010515003F0004ULL, 0x80010515003F00E4ULL,

	0x801205150D440000ULL, 0x801205150D4400E0ULL,
	0x801205150D440004ULL, 0x801205150D4400E4ULL,

	0x80020515F2100000ULL, 0x80020515F21000E0ULL,
	0x80020515F2100004ULL, 0x80020515F21000E4ULL,
	END_SIGN
};


void
__hal_serial_mem_write64(xge_hal_device_t *hldev, u64 value, u64 *reg)
{
	__hal_pio_mem_write32_upper(hldev->pdev, hldev->regh0,
	        (u32)(value>>32), reg);
	xge_os_wmb();
	__hal_pio_mem_write32_lower(hldev->pdev, hldev->regh0,
	        (u32)value, reg);
	xge_os_wmb();
	xge_os_mdelay(1);
}

u64
__hal_serial_mem_read64(xge_hal_device_t *hldev, u64 *reg)
{
	u64 val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	        reg);
	xge_os_mdelay(1);
	return val64;
}

/*
 * __hal_device_xaui_configure
 * @hldev: HAL device handle.
 *
 * Configure XAUI Interface of Xena.
 *
 * To Configure the Xena's XAUI, one has to write a series
 * of 64 bit values into two registers in a particular
 * sequence. Hence a macro 'SWITCH_SIGN' has been defined
 * which will be defined in the array of configuration values
 * (default_dtx_cfg & default_mdio_cfg) at appropriate places
 * to switch writing from one regsiter to another. We continue
 * writing these values until we encounter the 'END_SIGN' macro.
 * For example, After making a series of 21 writes into
 * dtx_control register the 'SWITCH_SIGN' appears and hence we
 * start writing into mdio_control until we encounter END_SIGN.
 */
static void
__hal_device_xaui_configure(xge_hal_device_t *hldev)
{
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;
	int mdio_cnt = 0, dtx_cnt = 0;
	u64 *default_dtx_cfg = NULL, *default_mdio_cfg = NULL;

	if (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_XENA) {
	    default_dtx_cfg = default_xena_dtx_cfg;
	    default_mdio_cfg = default_xena_mdio_cfg;
	} else if (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_HERC) {
	    default_dtx_cfg = default_herc_dtx_cfg;
	    default_mdio_cfg = default_herc_mdio_cfg;
	} else {
	    xge_assert(default_dtx_cfg);
	return;
  }

	do {
	    dtx_cfg:
	    while (default_dtx_cfg[dtx_cnt] != END_SIGN) {
	        if (default_dtx_cfg[dtx_cnt] == SWITCH_SIGN) {
	            dtx_cnt++;
	            goto mdio_cfg;
	        }
	        __hal_serial_mem_write64(hldev, default_dtx_cfg[dtx_cnt],
	                       &bar0->dtx_control);
	        dtx_cnt++;
	    }
	    mdio_cfg:
	    while (default_mdio_cfg[mdio_cnt] != END_SIGN) {
	        if (default_mdio_cfg[mdio_cnt] == SWITCH_SIGN) {
	            mdio_cnt++;
	            goto dtx_cfg;
	        }
	        __hal_serial_mem_write64(hldev, default_mdio_cfg[mdio_cnt],
	            &bar0->mdio_control);
	        mdio_cnt++;
	    }
	} while ( !((default_dtx_cfg[dtx_cnt] == END_SIGN) &&
	        (default_mdio_cfg[mdio_cnt] == END_SIGN)) );

	xge_debug_device(XGE_TRACE, "%s", "XAUI interface configured");
}

/*
 * __hal_device_mac_link_util_set
 * @hldev: HAL device handle.
 *
 * Set sampling rate to calculate link utilization.
 */
static void
__hal_device_mac_link_util_set(xge_hal_device_t *hldev)
{
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;
	u64 val64;

	val64 = XGE_HAL_MAC_TX_LINK_UTIL_VAL(
	        hldev->config.mac.tmac_util_period) |
	    XGE_HAL_MAC_RX_LINK_UTIL_VAL(
	        hldev->config.mac.rmac_util_period);
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	                     &bar0->mac_link_util);
	xge_debug_device(XGE_TRACE, "%s",
	          "bandwidth link utilization configured");
}

/*
 * __hal_device_set_swapper
 * @hldev: HAL device handle.
 *
 * Set the Xframe's byte "swapper" in accordance with
 * endianness of the host.
 */
xge_hal_status_e
__hal_device_set_swapper(xge_hal_device_t *hldev)
{
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;
	u64 val64;

	/*
	 * from 32bit errarta:
	 *
	 * The SWAPPER_CONTROL register determines how the adapter accesses
	 * host memory as well as how it responds to read and write requests
	 * from the host system. Writes to this register should be performed
	 * carefully, since the byte swappers could reverse the order of bytes.
	 * When configuring this register keep in mind that writes to the PIF
	 * read and write swappers could reverse the order of the upper and
	 * lower 32-bit words. This means that the driver may have to write
	 * to the upper 32 bits of the SWAPPER_CONTROL twice in order to
	 * configure the entire register. */

	/*
	 * The device by default set to a big endian format, so a big endian
	 * driver need not set anything.
	 */

#if defined(XGE_HAL_CUSTOM_HW_SWAPPER)

	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	        0xffffffffffffffffULL, &bar0->swapper_ctrl);

	val64 = XGE_HAL_CUSTOM_HW_SWAPPER;

	xge_os_wmb();
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	             &bar0->swapper_ctrl);

	xge_debug_device(XGE_TRACE, "using custom HW swapper 0x"XGE_OS_LLXFMT,
	        (unsigned long long)val64);

#elif !defined(XGE_OS_HOST_BIG_ENDIAN)

	/*
	 * Initially we enable all bits to make it accessible by the driver,
	 * then we selectively enable only those bits that we want to set.
	 * i.e. force swapper to swap for the first time since second write
	 * will overwrite with the final settings.
	 *
	 * Use only for little endian platforms.
	 */
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	        0xffffffffffffffffULL, &bar0->swapper_ctrl);
	xge_os_wmb();
	val64 = (XGE_HAL_SWAPPER_CTRL_PIF_R_FE |
	     XGE_HAL_SWAPPER_CTRL_PIF_R_SE |
	     XGE_HAL_SWAPPER_CTRL_PIF_W_FE |
	     XGE_HAL_SWAPPER_CTRL_PIF_W_SE |
	     XGE_HAL_SWAPPER_CTRL_RTH_FE |
	     XGE_HAL_SWAPPER_CTRL_RTH_SE |
	     XGE_HAL_SWAPPER_CTRL_TXP_FE |
	     XGE_HAL_SWAPPER_CTRL_TXP_SE |
	     XGE_HAL_SWAPPER_CTRL_TXD_R_FE |
	     XGE_HAL_SWAPPER_CTRL_TXD_R_SE |
	     XGE_HAL_SWAPPER_CTRL_TXD_W_FE |
	     XGE_HAL_SWAPPER_CTRL_TXD_W_SE |
	     XGE_HAL_SWAPPER_CTRL_TXF_R_FE |
	     XGE_HAL_SWAPPER_CTRL_RXD_R_FE |
	     XGE_HAL_SWAPPER_CTRL_RXD_R_SE |
	     XGE_HAL_SWAPPER_CTRL_RXD_W_FE |
	     XGE_HAL_SWAPPER_CTRL_RXD_W_SE |
	     XGE_HAL_SWAPPER_CTRL_RXF_W_FE |
	     XGE_HAL_SWAPPER_CTRL_XMSI_FE |
	     XGE_HAL_SWAPPER_CTRL_STATS_FE | XGE_HAL_SWAPPER_CTRL_STATS_SE);

	/*
	if (hldev->config.intr_mode == XGE_HAL_INTR_MODE_MSIX) {
	     val64 |= XGE_HAL_SWAPPER_CTRL_XMSI_SE;
	} */
	__hal_pio_mem_write32_lower(hldev->pdev, hldev->regh0, (u32)val64,
	                     &bar0->swapper_ctrl);
	xge_os_wmb();
	__hal_pio_mem_write32_upper(hldev->pdev, hldev->regh0, (u32)(val64>>32),
	                     &bar0->swapper_ctrl);
	xge_os_wmb();
	__hal_pio_mem_write32_upper(hldev->pdev, hldev->regh0, (u32)(val64>>32),
	                     &bar0->swapper_ctrl);
	xge_debug_device(XGE_TRACE, "%s", "using little endian set");
#endif

	/*  Verifying if endian settings are accurate by reading a feedback
	 *  register.  */
	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                            &bar0->pif_rd_swapper_fb);
	if (val64 != XGE_HAL_IF_RD_SWAPPER_FB) {
	    xge_debug_device(XGE_ERR, "pif_rd_swapper_fb read "XGE_OS_LLXFMT,
	          (unsigned long long) val64);
	    return XGE_HAL_ERR_SWAPPER_CTRL;
	}

	xge_debug_device(XGE_TRACE, "%s", "be/le swapper enabled");

	return XGE_HAL_OK;
}

/*
 * __hal_device_rts_mac_configure - Configure RTS steering based on
 * destination mac address.
 * @hldev: HAL device handle.
 *
 */
xge_hal_status_e
__hal_device_rts_mac_configure(xge_hal_device_t *hldev)
{
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;
	u64 val64;

	if (!hldev->config.rts_mac_en) {
	    return XGE_HAL_OK;
	}

	/*
	* Set the receive traffic steering mode from default(classic)
	* to enhanced.
	*/
	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                &bar0->rts_ctrl);
	val64 |=  XGE_HAL_RTS_CTRL_ENHANCED_MODE;
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	            val64, &bar0->rts_ctrl);
	return XGE_HAL_OK;
}

/*
 * __hal_device_rts_port_configure - Configure RTS steering based on
 * destination or source port number.
 * @hldev: HAL device handle.
 *
 */
xge_hal_status_e
__hal_device_rts_port_configure(xge_hal_device_t *hldev)
{
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;
	u64 val64;
	int rnum;

	if (!hldev->config.rts_port_en) {
	    return XGE_HAL_OK;
	}

	/*
	 * Set the receive traffic steering mode from default(classic)
	 * to enhanced.
	 */
	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                &bar0->rts_ctrl);
	val64 |=  XGE_HAL_RTS_CTRL_ENHANCED_MODE;
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	            val64, &bar0->rts_ctrl);

	/*
	 * Initiate port steering according to per-ring configuration
	 */
	for (rnum = 0; rnum < XGE_HAL_MAX_RING_NUM; rnum++) {
	    int pnum;
	    xge_hal_ring_queue_t *queue = &hldev->config.ring.queue[rnum];

	    if (!queue->configured || queue->rts_port_en)
	        continue;

	    for (pnum = 0; pnum < XGE_HAL_MAX_STEERABLE_PORTS; pnum++) {
	        xge_hal_rts_port_t *port = &queue->rts_ports[pnum];

	        /*
	         * Skip and clear empty ports
	         */
	        if (!port->num) {
	            /*
	             * Clear CAM memory
	             */
	            xge_os_pio_mem_write64(hldev->pdev,
	                   hldev->regh0, 0ULL,
	                   &bar0->rts_pn_cam_data);

	            val64 = BIT(7) | BIT(15);
	        } else {
	            /*
	             * Assign new Port values according
	             * to configuration
	             */
	            val64 = vBIT(port->num,8,16) |
	                vBIT(rnum,37,3) | BIT(63);
	            if (port->src)
	                val64 = BIT(47);
	            if (!port->udp)
	                val64 = BIT(7);
	            xge_os_pio_mem_write64(hldev->pdev,
	                       hldev->regh0, val64,
	                       &bar0->rts_pn_cam_data);

	            val64 = BIT(7) | BIT(15) | vBIT(pnum,24,8);
	        }

	        xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	                       val64, &bar0->rts_pn_cam_ctrl);

	        /* poll until done */
	        if (__hal_device_register_poll(hldev,
	               &bar0->rts_pn_cam_ctrl, 0,
	               XGE_HAL_RTS_PN_CAM_CTRL_STROBE_BEING_EXECUTED,
	               XGE_HAL_DEVICE_CMDMEM_WAIT_MAX_MILLIS) !=
	                            XGE_HAL_OK) {
	            /* upper layer may require to repeat */
	            return XGE_HAL_INF_MEM_STROBE_CMD_EXECUTING;
	        }
	    }
	}
	return XGE_HAL_OK;
}

/*
 * __hal_device_rts_qos_configure - Configure RTS steering based on
 * qos.
 * @hldev: HAL device handle.
 *
 */
xge_hal_status_e
__hal_device_rts_qos_configure(xge_hal_device_t *hldev)
{
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;
	u64 val64;
	int j, rx_ring_num;

	if (!hldev->config.rts_qos_en) {
	    return XGE_HAL_OK;
	}

	/* First clear the RTS_DS_MEM_DATA */
	val64 = 0;
	for (j = 0; j < 64; j++ )
	{
	    /* First clear the value */
	    val64 = XGE_HAL_RTS_DS_MEM_DATA(0);

	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	                   &bar0->rts_ds_mem_data);

	    val64 = XGE_HAL_RTS_DS_MEM_CTRL_WE |
	        XGE_HAL_RTS_DS_MEM_CTRL_STROBE_NEW_CMD |
	        XGE_HAL_RTS_DS_MEM_CTRL_OFFSET ( j );

	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	                   &bar0->rts_ds_mem_ctrl);


	    /* poll until done */
	    if (__hal_device_register_poll(hldev,
	           &bar0->rts_ds_mem_ctrl, 0,
	           XGE_HAL_RTS_DS_MEM_CTRL_STROBE_CMD_BEING_EXECUTED,
	           XGE_HAL_DEVICE_CMDMEM_WAIT_MAX_MILLIS) != XGE_HAL_OK) {
	        /* upper layer may require to repeat */
	        return XGE_HAL_INF_MEM_STROBE_CMD_EXECUTING;
	    }

	}

	rx_ring_num = 0;
	for (j = 0; j < XGE_HAL_MAX_RING_NUM; j++) {
	    if (hldev->config.ring.queue[j].configured)
	        rx_ring_num++;
	}

	switch (rx_ring_num) {
	case 1:
	    val64 = 0x0;
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64, &bar0->rx_w_round_robin_0);
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64, &bar0->rx_w_round_robin_1);
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64, &bar0->rx_w_round_robin_2);
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64, &bar0->rx_w_round_robin_3);
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64, &bar0->rx_w_round_robin_4);
	    break;
	case 2:
	    val64 = 0x0001000100010001ULL;
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64, &bar0->rx_w_round_robin_0);
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64, &bar0->rx_w_round_robin_1);
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64, &bar0->rx_w_round_robin_2);
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64, &bar0->rx_w_round_robin_3);
	    val64 = 0x0001000100000000ULL;
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64, &bar0->rx_w_round_robin_4);
	    break;
	case 3:
	    val64 = 0x0001020001020001ULL;
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64, &bar0->rx_w_round_robin_0);
	    val64 = 0x0200010200010200ULL;
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64, &bar0->rx_w_round_robin_1);
	    val64 = 0x0102000102000102ULL;
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64, &bar0->rx_w_round_robin_2);
	    val64 = 0x0001020001020001ULL;
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64, &bar0->rx_w_round_robin_3);
	    val64 = 0x0200010200000000ULL;
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64, &bar0->rx_w_round_robin_4);
	    break;
	case 4:
	    val64 = 0x0001020300010203ULL;
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64, &bar0->rx_w_round_robin_0);
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64, &bar0->rx_w_round_robin_1);
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64, &bar0->rx_w_round_robin_2);
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64, &bar0->rx_w_round_robin_3);
	    val64 = 0x0001020300000000ULL;
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64, &bar0->rx_w_round_robin_4);
	    break;
	case 5:
	    val64 = 0x0001020304000102ULL;
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64, &bar0->rx_w_round_robin_0);
	    val64 = 0x0304000102030400ULL;
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64, &bar0->rx_w_round_robin_1);
	    val64 = 0x0102030400010203ULL;
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64, &bar0->rx_w_round_robin_2);
	    val64 = 0x0400010203040001ULL;
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64, &bar0->rx_w_round_robin_3);
	    val64 = 0x0203040000000000ULL;
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64, &bar0->rx_w_round_robin_4);
	    break;
	case 6:
	    val64 = 0x0001020304050001ULL;
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64, &bar0->rx_w_round_robin_0);
	    val64 = 0x0203040500010203ULL;
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64, &bar0->rx_w_round_robin_1);
	    val64 = 0x0405000102030405ULL;
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64, &bar0->rx_w_round_robin_2);
	    val64 = 0x0001020304050001ULL;
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64, &bar0->rx_w_round_robin_3);
	    val64 = 0x0203040500000000ULL;
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64, &bar0->rx_w_round_robin_4);
	    break;
	case 7:
	    val64 = 0x0001020304050600ULL;
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64, &bar0->rx_w_round_robin_0);
	    val64 = 0x0102030405060001ULL;
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64, &bar0->rx_w_round_robin_1);
	    val64 = 0x0203040506000102ULL;
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64, &bar0->rx_w_round_robin_2);
	    val64 = 0x0304050600010203ULL;
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64, &bar0->rx_w_round_robin_3);
	    val64 = 0x0405060000000000ULL;
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64, &bar0->rx_w_round_robin_4);
	    break;
	case 8:
	    val64 = 0x0001020304050607ULL;
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64, &bar0->rx_w_round_robin_0);
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64, &bar0->rx_w_round_robin_1);
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64, &bar0->rx_w_round_robin_2);
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64, &bar0->rx_w_round_robin_3);
	    val64 = 0x0001020300000000ULL;
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64, &bar0->rx_w_round_robin_4);
	    break;
	}

	return XGE_HAL_OK;
}

/*
 * xge__hal_device_rts_mac_enable
 *
 * @devh: HAL device handle.
 * @index: index number where the MAC addr will be stored
 * @macaddr: MAC address
 *
 * - Enable RTS steering for the given MAC address. This function has to be
 * called with lock acquired.
 *
 * NOTE:
 * 1. ULD has to call this function with the index value which
 *    statisfies the following condition:
 *  ring_num = (index % 8)
 * 2.ULD also needs to make sure that the index is not
 *   occupied by any MAC address. If that index has any MAC address
 *   it will be overwritten and HAL will not check for it.
 *
 */
xge_hal_status_e
xge_hal_device_rts_mac_enable(xge_hal_device_h devh, int index, macaddr_t macaddr)
{
	int max_addr = XGE_HAL_MAX_MAC_ADDRESSES;
	xge_hal_status_e status;

	xge_hal_device_t *hldev = (xge_hal_device_t *)devh;

	if (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_HERC)
	    max_addr = XGE_HAL_MAX_MAC_ADDRESSES_HERC;

	if ( index >= max_addr )
	    return XGE_HAL_ERR_OUT_OF_MAC_ADDRESSES;

	/*
	 * Set the MAC address at the given location marked by index.
	 */
	status = xge_hal_device_macaddr_set(hldev, index, macaddr);
	if (status != XGE_HAL_OK) {
	    xge_debug_device(XGE_ERR, "%s",
	        "Not able to set the mac addr");
	    return status;
	}

	return xge_hal_device_rts_section_enable(hldev, index);
}

/*
 * xge__hal_device_rts_mac_disable
 * @hldev: HAL device handle.
 * @index: index number where to disable the MAC addr
 *
 * Disable RTS Steering based on the MAC address.
 * This function should be called with lock acquired.
 *
 */
xge_hal_status_e
xge_hal_device_rts_mac_disable(xge_hal_device_h devh, int index)
{
	xge_hal_status_e status;
	u8 macaddr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	int max_addr = XGE_HAL_MAX_MAC_ADDRESSES;

	xge_hal_device_t *hldev = (xge_hal_device_t *)devh;

	xge_debug_ll(XGE_TRACE, "the index value is %d ", index);

	if (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_HERC)
	    max_addr = XGE_HAL_MAX_MAC_ADDRESSES_HERC;

	if ( index >= max_addr )
	    return XGE_HAL_ERR_OUT_OF_MAC_ADDRESSES;

	/*
	 * Disable MAC address @ given index location
	 */
	status = xge_hal_device_macaddr_set(hldev, index, macaddr);
	if (status != XGE_HAL_OK) {
	    xge_debug_device(XGE_ERR, "%s",
	        "Not able to set the mac addr");
	    return status;
	}

	return XGE_HAL_OK;
}


/*
 * __hal_device_rth_configure - Configure RTH for the device
 * @hldev: HAL device handle.
 *
 * Using IT (Indirection Table).
 */
xge_hal_status_e
__hal_device_rth_it_configure(xge_hal_device_t *hldev)
{
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;
	u64 val64;
	int rings[XGE_HAL_MAX_RING_NUM]={0};
	int rnum;
	int rmax;
	int buckets_num;
	int bucket;

	if (!hldev->config.rth_en) {
	    return XGE_HAL_OK;
	}

	/*
	 * Set the receive traffic steering mode from default(classic)
	 * to enhanced.
	 */
	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                  &bar0->rts_ctrl);
	val64 |=  XGE_HAL_RTS_CTRL_ENHANCED_MODE;
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	               val64, &bar0->rts_ctrl);

	buckets_num = (1 << hldev->config.rth_bucket_size);

	rmax=0;
	for (rnum = 0; rnum < XGE_HAL_MAX_RING_NUM; rnum++) {
	    if (hldev->config.ring.queue[rnum].configured &&
	            hldev->config.ring.queue[rnum].rth_en)
	            rings[rmax++] = rnum;
	}

	rnum = 0;
	/* for starters: fill in all the buckets with rings "equally" */
	for (bucket = 0; bucket < buckets_num; bucket++) {

	    if (rnum == rmax)
	       rnum = 0;

	    /* write data */
	    val64 = XGE_HAL_RTS_RTH_MAP_MEM_DATA_ENTRY_EN |
	            XGE_HAL_RTS_RTH_MAP_MEM_DATA(rings[rnum]);
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	                 &bar0->rts_rth_map_mem_data);

	    /* execute */
	    val64 = XGE_HAL_RTS_RTH_MAP_MEM_CTRL_WE |
	        XGE_HAL_RTS_RTH_MAP_MEM_CTRL_STROBE |
	        XGE_HAL_RTS_RTH_MAP_MEM_CTRL_OFFSET(bucket);
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	                 &bar0->rts_rth_map_mem_ctrl);

	    /* poll until done */
	    if (__hal_device_register_poll(hldev,
	        &bar0->rts_rth_map_mem_ctrl, 0,
	        XGE_HAL_RTS_RTH_MAP_MEM_CTRL_STROBE,
	        XGE_HAL_DEVICE_CMDMEM_WAIT_MAX_MILLIS) != XGE_HAL_OK) {
	        return XGE_HAL_INF_MEM_STROBE_CMD_EXECUTING;
	    }

	    rnum++;
	}

	val64 = XGE_HAL_RTS_RTH_EN;
	val64 |= XGE_HAL_RTS_RTH_BUCKET_SIZE(hldev->config.rth_bucket_size);
	val64 |= XGE_HAL_RTS_RTH_TCP_IPV4_EN | XGE_HAL_RTS_RTH_UDP_IPV4_EN | XGE_HAL_RTS_RTH_IPV4_EN |
	         XGE_HAL_RTS_RTH_TCP_IPV6_EN |XGE_HAL_RTS_RTH_UDP_IPV6_EN | XGE_HAL_RTS_RTH_IPV6_EN |
	         XGE_HAL_RTS_RTH_TCP_IPV6_EX_EN | XGE_HAL_RTS_RTH_UDP_IPV6_EX_EN | XGE_HAL_RTS_RTH_IPV6_EX_EN;

	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	             &bar0->rts_rth_cfg);

	xge_debug_device(XGE_TRACE, "RTH configured, bucket_size %d",
	          hldev->config.rth_bucket_size);

	return XGE_HAL_OK;
}


/*
 * __hal_spdm_entry_add - Add a new entry to the SPDM table.
 *
 * Add a new entry to the SPDM table
 *
 * This function add a new entry to the SPDM table.
 *
 * Note:
 *   This function should be called with spdm_lock.
 *
 * See also: xge_hal_spdm_entry_add , xge_hal_spdm_entry_remove.
 */
static xge_hal_status_e
__hal_spdm_entry_add(xge_hal_device_t *hldev, xge_hal_ipaddr_t *src_ip,
	    xge_hal_ipaddr_t *dst_ip, u16 l4_sp, u16 l4_dp, u8 is_tcp,
	    u8 is_ipv4, u8 tgt_queue, u32 jhash_value, u16 spdm_entry)
{
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;
	u64 val64;
	u64 spdm_line_arr[8];
	u8 line_no;

	/*
	 * Clear the SPDM READY bit
	 */
	val64 = XGE_HAL_RX_PIC_INT_REG_SPDM_READY;
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	               &bar0->rxpic_int_reg);

	xge_debug_device(XGE_TRACE,
	        "L4 SP %x:DP %x: hash %x tgt_queue %d ",
	        l4_sp, l4_dp, jhash_value, tgt_queue);

	xge_os_memzero(&spdm_line_arr, sizeof(spdm_line_arr));

	/*
	 * Construct the SPDM entry.
	 */
	spdm_line_arr[0] = vBIT(l4_sp,0,16) |
	           vBIT(l4_dp,16,32) |
	           vBIT(tgt_queue,53,3) |
	           vBIT(is_tcp,59,1) |
	           vBIT(is_ipv4,63,1);


	if (is_ipv4) {
	    spdm_line_arr[1] = vBIT(src_ip->ipv4.addr,0,32) |
	               vBIT(dst_ip->ipv4.addr,32,32);

	} else {
	    xge_os_memcpy(&spdm_line_arr[1], &src_ip->ipv6.addr[0], 8);
	    xge_os_memcpy(&spdm_line_arr[2], &src_ip->ipv6.addr[1], 8);
	    xge_os_memcpy(&spdm_line_arr[3], &dst_ip->ipv6.addr[0], 8);
	    xge_os_memcpy(&spdm_line_arr[4], &dst_ip->ipv6.addr[1], 8);
	}

	spdm_line_arr[7] = vBIT(jhash_value,0,32) |
	            BIT(63);  /* entry enable bit */

	/*
	 * Add the entry to the SPDM table
	 */
	for(line_no = 0; line_no < 8; line_no++) {
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	            spdm_line_arr[line_no],
	            (void *)((char *)hldev->spdm_mem_base +
	                    (spdm_entry * 64) +
	                    (line_no * 8)));
	}

	/*
	 * Wait for the operation to be completed.
	 */
	if (__hal_device_register_poll(hldev, &bar0->rxpic_int_reg, 1,
	        XGE_HAL_RX_PIC_INT_REG_SPDM_READY,
	        XGE_HAL_DEVICE_CMDMEM_WAIT_MAX_MILLIS) != XGE_HAL_OK) {
	    return XGE_HAL_INF_MEM_STROBE_CMD_EXECUTING;
	}

	/*
	 * Add this information to a local SPDM table. The purpose of
	 * maintaining a local SPDM table is to avoid a search in the
	 * adapter SPDM table for spdm entry lookup which is very costly
	 * in terms of time.
	 */
	hldev->spdm_table[spdm_entry]->in_use = 1;
	xge_os_memcpy(&hldev->spdm_table[spdm_entry]->src_ip, src_ip,
	        sizeof(xge_hal_ipaddr_t));
	xge_os_memcpy(&hldev->spdm_table[spdm_entry]->dst_ip, dst_ip,
	        sizeof(xge_hal_ipaddr_t));
	hldev->spdm_table[spdm_entry]->l4_sp = l4_sp;
	hldev->spdm_table[spdm_entry]->l4_dp = l4_dp;
	hldev->spdm_table[spdm_entry]->is_tcp = is_tcp;
	hldev->spdm_table[spdm_entry]->is_ipv4 = is_ipv4;
	hldev->spdm_table[spdm_entry]->tgt_queue = tgt_queue;
	hldev->spdm_table[spdm_entry]->jhash_value = jhash_value;
	hldev->spdm_table[spdm_entry]->spdm_entry = spdm_entry;

	return XGE_HAL_OK;
}

/*
 * __hal_device_rth_spdm_configure - Configure RTH for the device
 * @hldev: HAL device handle.
 *
 * Using SPDM (Socket-Pair Direct Match).
 */
xge_hal_status_e
__hal_device_rth_spdm_configure(xge_hal_device_t *hldev)
{
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)hldev->bar0;
	u64 val64;
	u8 spdm_bar_num;
	u32 spdm_bar_offset;
	int spdm_table_size;
	int i;

	if (!hldev->config.rth_spdm_en) {
	    return XGE_HAL_OK;
	}

	/*
	 * Retrieve the base address of SPDM Table.
	 */
	val64 = xge_os_pio_mem_read64(hldev->pdev,
	        hldev->regh0, &bar0->spdm_bir_offset);

	spdm_bar_num    = XGE_HAL_SPDM_PCI_BAR_NUM(val64);
	spdm_bar_offset = XGE_HAL_SPDM_PCI_BAR_OFFSET(val64);


	/*
	 * spdm_bar_num specifies the PCI bar num register used to
	 * address the memory space. spdm_bar_offset specifies the offset
	 * of the SPDM memory with in the bar num memory space.
	 */
	switch (spdm_bar_num) {
	    case 0:
	    {
	        hldev->spdm_mem_base = (char *)bar0 +
	                    (spdm_bar_offset * 8);
	        break;
	    }
	    case 1:
	    {
	        char *bar1 = (char *)hldev->bar1;
	        hldev->spdm_mem_base = bar1 + (spdm_bar_offset * 8);
	        break;
	    }
	    default:
	        xge_assert(((spdm_bar_num != 0) && (spdm_bar_num != 1)));
	}

	/*
	 * Retrieve the size of SPDM table(number of entries).
	 */
	val64 = xge_os_pio_mem_read64(hldev->pdev,
	        hldev->regh0, &bar0->spdm_structure);
	hldev->spdm_max_entries = XGE_HAL_SPDM_MAX_ENTRIES(val64);


	spdm_table_size = hldev->spdm_max_entries *
	                sizeof(xge_hal_spdm_entry_t);
	if (hldev->spdm_table == NULL) {
	    void *mem;

	    /*
	     * Allocate memory to hold the copy of SPDM table.
	     */
	    if ((hldev->spdm_table = (xge_hal_spdm_entry_t **)
	                xge_os_malloc(
	                 hldev->pdev,
	                 (sizeof(xge_hal_spdm_entry_t *) *
	                 hldev->spdm_max_entries))) == NULL) {
	        return XGE_HAL_ERR_OUT_OF_MEMORY;
	    }

	    if ((mem = xge_os_malloc(hldev->pdev, spdm_table_size)) == NULL)
	    {
	        xge_os_free(hldev->pdev, hldev->spdm_table,
	              (sizeof(xge_hal_spdm_entry_t *) *
	                 hldev->spdm_max_entries));
	        return XGE_HAL_ERR_OUT_OF_MEMORY;
	    }

	    xge_os_memzero(mem, spdm_table_size);
	    for (i = 0; i < hldev->spdm_max_entries; i++) {
	        hldev->spdm_table[i] = (xge_hal_spdm_entry_t *)
	                ((char *)mem +
	                 i * sizeof(xge_hal_spdm_entry_t));
	    }
	    xge_os_spin_lock_init(&hldev->spdm_lock, hldev->pdev);
	} else {
	    /*
	     * We are here because the host driver tries to
	     * do a soft reset on the device.
	     * Since the device soft reset clears the SPDM table, copy
	     * the entries from the local SPDM table to the actual one.
	     */
	    xge_os_spin_lock(&hldev->spdm_lock);
	    for (i = 0; i < hldev->spdm_max_entries; i++) {
	        xge_hal_spdm_entry_t *spdm_entry = hldev->spdm_table[i];

	        if (spdm_entry->in_use) {
	            if (__hal_spdm_entry_add(hldev,
	                         &spdm_entry->src_ip,
	                         &spdm_entry->dst_ip,
	                         spdm_entry->l4_sp,
	                         spdm_entry->l4_dp,
	                         spdm_entry->is_tcp,
	                         spdm_entry->is_ipv4,
	                         spdm_entry->tgt_queue,
	                         spdm_entry->jhash_value,
	                         spdm_entry->spdm_entry)
	                    != XGE_HAL_OK) {
	                /* Log an warning */
	                xge_debug_device(XGE_ERR,
	                    "SPDM table update from local"
	                    " memory failed");
	            }
	        }
	    }
	    xge_os_spin_unlock(&hldev->spdm_lock);
	}

	/*
	 * Set the receive traffic steering mode from default(classic)
	 * to enhanced.
	 */
	val64 = xge_os_pio_mem_read64(hldev->pdev,
	                hldev->regh0, &bar0->rts_ctrl);
	val64 |=  XGE_HAL_RTS_CTRL_ENHANCED_MODE;
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	             val64, &bar0->rts_ctrl);

	/*
	 * We may not need to configure rts_rth_jhash_cfg register as the
	 * default values are good enough to calculate the hash.
	 */

	/*
	 * As of now, set all the rth mask registers to zero. TODO.
	 */
	for(i = 0; i < 5; i++) {
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	                 0, &bar0->rts_rth_hash_mask[i]);
	}

	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	             0, &bar0->rts_rth_hash_mask_5);

	if (hldev->config.rth_spdm_use_l4) {
	    val64 = XGE_HAL_RTH_STATUS_SPDM_USE_L4;
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	                 val64, &bar0->rts_rth_status);
	}

	val64 = XGE_HAL_RTS_RTH_EN;
	val64 |= XGE_HAL_RTS_RTH_IPV4_EN | XGE_HAL_RTS_RTH_TCP_IPV4_EN;
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	             &bar0->rts_rth_cfg);


	return XGE_HAL_OK;
}

/*
 * __hal_device_pci_init
 * @hldev: HAL device handle.
 *
 * Initialize certain PCI/PCI-X configuration registers
 * with recommended values. Save config space for future hw resets.
 */
static void
__hal_device_pci_init(xge_hal_device_t *hldev)
{
	int i, pcisize = 0;
	u16 cmd = 0;
	u8  val;

	/* Store PCI device ID and revision for future references where in we
	 * decide Xena revision using PCI sub system ID */
	xge_os_pci_read16(hldev->pdev,hldev->cfgh,
	        xge_offsetof(xge_hal_pci_config_le_t, device_id),
	        &hldev->device_id);
	xge_os_pci_read8(hldev->pdev,hldev->cfgh,
	        xge_offsetof(xge_hal_pci_config_le_t, revision),
	        &hldev->revision);

	if (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_HERC)
	    pcisize = XGE_HAL_PCISIZE_HERC;
	else if (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_XENA)
	    pcisize = XGE_HAL_PCISIZE_XENA;

	/* save original PCI config space to restore it on device_terminate() */
	for (i = 0; i < pcisize; i++) {
	    xge_os_pci_read32(hldev->pdev, hldev->cfgh, i*4,
	                    (u32*)&hldev->pci_config_space_bios + i);
	}

	/* Set the PErr Repconse bit and SERR in PCI command register. */
	xge_os_pci_read16(hldev->pdev, hldev->cfgh,
	        xge_offsetof(xge_hal_pci_config_le_t, command), &cmd);
	cmd |= 0x140;
	xge_os_pci_write16(hldev->pdev, hldev->cfgh,
	         xge_offsetof(xge_hal_pci_config_le_t, command), cmd);

	/* Set user spcecified value for the PCI Latency Timer */
	if (hldev->config.latency_timer &&
	    hldev->config.latency_timer != XGE_HAL_USE_BIOS_DEFAULT_LATENCY) {
	    xge_os_pci_write8(hldev->pdev, hldev->cfgh,
	                 xge_offsetof(xge_hal_pci_config_le_t,
	                 latency_timer),
	         (u8)hldev->config.latency_timer);
	}
	/* Read back latency timer to reflect it into user level */
	xge_os_pci_read8(hldev->pdev, hldev->cfgh,
	    xge_offsetof(xge_hal_pci_config_le_t, latency_timer), &val);
	hldev->config.latency_timer = val;

	/* Enable Data Parity Error Recovery in PCI-X command register. */
	xge_os_pci_read16(hldev->pdev, hldev->cfgh,
	    xge_offsetof(xge_hal_pci_config_le_t, pcix_command), &cmd);
	cmd |= 1;
	xge_os_pci_write16(hldev->pdev, hldev->cfgh,
	     xge_offsetof(xge_hal_pci_config_le_t, pcix_command), cmd);

	/* Set MMRB count in PCI-X command register. */
	if (hldev->config.mmrb_count != XGE_HAL_DEFAULT_BIOS_MMRB_COUNT) {
	    cmd &= 0xFFF3;
	    cmd |= hldev->config.mmrb_count << 2;
	    xge_os_pci_write16(hldev->pdev, hldev->cfgh,
	           xge_offsetof(xge_hal_pci_config_le_t, pcix_command),
	           cmd);
	}
	/* Read back MMRB count to reflect it into user level */
	xge_os_pci_read16(hldev->pdev, hldev->cfgh,
	            xge_offsetof(xge_hal_pci_config_le_t, pcix_command),
	            &cmd);
	cmd &= 0x000C;
	hldev->config.mmrb_count = cmd>>2;

	/*  Setting Maximum outstanding splits based on system type. */
	if (hldev->config.max_splits_trans != XGE_HAL_USE_BIOS_DEFAULT_SPLITS)  {
	    xge_os_pci_read16(hldev->pdev, hldev->cfgh,
	        xge_offsetof(xge_hal_pci_config_le_t, pcix_command),
	        &cmd);
	    cmd &= 0xFF8F;
	    cmd |= hldev->config.max_splits_trans << 4;
	    xge_os_pci_write16(hldev->pdev, hldev->cfgh,
	        xge_offsetof(xge_hal_pci_config_le_t, pcix_command),
	        cmd);
	}

	/* Read back max split trans to reflect it into user level */
	xge_os_pci_read16(hldev->pdev, hldev->cfgh,
	    xge_offsetof(xge_hal_pci_config_le_t, pcix_command), &cmd);
	cmd &= 0x0070;
	hldev->config.max_splits_trans = cmd>>4;

	/* Forcibly disabling relaxed ordering capability of the card. */
	xge_os_pci_read16(hldev->pdev, hldev->cfgh,
	    xge_offsetof(xge_hal_pci_config_le_t, pcix_command), &cmd);
	cmd &= 0xFFFD;
	xge_os_pci_write16(hldev->pdev, hldev->cfgh,
	     xge_offsetof(xge_hal_pci_config_le_t, pcix_command), cmd);

	/* save PCI config space for future resets */
	for (i = 0; i < pcisize; i++) {
	    xge_os_pci_read32(hldev->pdev, hldev->cfgh, i*4,
	                    (u32*)&hldev->pci_config_space + i);
	}
}

/*
 * __hal_device_pci_info_get - Get PCI bus informations such as width, frequency
 *                               and mode.
 * @devh: HAL device handle.
 * @pci_mode:       pointer to a variable of enumerated type
 *          xge_hal_pci_mode_e{}.
 * @bus_frequency:  pointer to a variable of enumerated type
 *          xge_hal_pci_bus_frequency_e{}.
 * @bus_width:      pointer to a variable of enumerated type
 *          xge_hal_pci_bus_width_e{}.
 *
 * Get pci mode, frequency, and PCI bus width.
 *
 * Returns: one of the xge_hal_status_e{} enumerated types.
 * XGE_HAL_OK           - for success.
 * XGE_HAL_ERR_INVALID_PCI_INFO - for invalid PCI information from the card.
 * XGE_HAL_ERR_BAD_DEVICE_ID    - for invalid card.
 *
 * See Also: xge_hal_pci_mode_e, xge_hal_pci_mode_e, xge_hal_pci_width_e.
 */
static xge_hal_status_e
__hal_device_pci_info_get(xge_hal_device_h devh, xge_hal_pci_mode_e *pci_mode,
	    xge_hal_pci_bus_frequency_e *bus_frequency,
	    xge_hal_pci_bus_width_e *bus_width)
{
	xge_hal_device_t *hldev = (xge_hal_device_t *)devh;
	xge_hal_status_e rc_status = XGE_HAL_OK;
	xge_hal_card_e card_id     = xge_hal_device_check_id (devh);

#ifdef XGE_HAL_HERC_EMULATION
	hldev->config.pci_freq_mherz =
	    XGE_HAL_PCI_BUS_FREQUENCY_66MHZ;
	*bus_frequency  =
	    XGE_HAL_PCI_BUS_FREQUENCY_66MHZ;
	*pci_mode = XGE_HAL_PCI_66MHZ_MODE;
#else
	if (card_id == XGE_HAL_CARD_HERC) {
	    xge_hal_pci_bar0_t *bar0 =
	    (xge_hal_pci_bar0_t *)(void *)hldev->bar0;
	    u64 pci_info = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                &bar0->pci_info);
	    if (XGE_HAL_PCI_32_BIT & pci_info)
	        *bus_width = XGE_HAL_PCI_BUS_WIDTH_32BIT;
	    else
	        *bus_width = XGE_HAL_PCI_BUS_WIDTH_64BIT;
	    switch((pci_info & XGE_HAL_PCI_INFO)>>60)
	    {
	        case XGE_HAL_PCI_33MHZ_MODE:
	             *bus_frequency =
	                 XGE_HAL_PCI_BUS_FREQUENCY_33MHZ;
	             *pci_mode = XGE_HAL_PCI_33MHZ_MODE;
	             break;
	        case XGE_HAL_PCI_66MHZ_MODE:
	             *bus_frequency =
	                 XGE_HAL_PCI_BUS_FREQUENCY_66MHZ;
	             *pci_mode = XGE_HAL_PCI_66MHZ_MODE;
	             break;
	        case XGE_HAL_PCIX_M1_66MHZ_MODE:
	             *bus_frequency =
	                 XGE_HAL_PCI_BUS_FREQUENCY_66MHZ;
	             *pci_mode = XGE_HAL_PCIX_M1_66MHZ_MODE;
	             break;
	        case XGE_HAL_PCIX_M1_100MHZ_MODE:
	             *bus_frequency =
	                 XGE_HAL_PCI_BUS_FREQUENCY_100MHZ;
	             *pci_mode = XGE_HAL_PCIX_M1_100MHZ_MODE;
	             break;
	        case XGE_HAL_PCIX_M1_133MHZ_MODE:
	             *bus_frequency =
	                 XGE_HAL_PCI_BUS_FREQUENCY_133MHZ;
	             *pci_mode = XGE_HAL_PCIX_M1_133MHZ_MODE;
	             break;
	        case XGE_HAL_PCIX_M2_66MHZ_MODE:
	             *bus_frequency =
	                 XGE_HAL_PCI_BUS_FREQUENCY_133MHZ;
	             *pci_mode = XGE_HAL_PCIX_M2_66MHZ_MODE;
	             break;
	        case XGE_HAL_PCIX_M2_100MHZ_MODE:
	             *bus_frequency =
	                 XGE_HAL_PCI_BUS_FREQUENCY_200MHZ;
	             *pci_mode = XGE_HAL_PCIX_M2_100MHZ_MODE;
	             break;
	        case XGE_HAL_PCIX_M2_133MHZ_MODE:
	             *bus_frequency =
	                 XGE_HAL_PCI_BUS_FREQUENCY_266MHZ;
	             *pci_mode = XGE_HAL_PCIX_M2_133MHZ_MODE;
	              break;
	        case XGE_HAL_PCIX_M1_RESERVED:
	        case XGE_HAL_PCIX_M1_66MHZ_NS:
	        case XGE_HAL_PCIX_M1_100MHZ_NS:
	        case XGE_HAL_PCIX_M1_133MHZ_NS:
	        case XGE_HAL_PCIX_M2_RESERVED:
	        case XGE_HAL_PCIX_533_RESERVED:
	        default:
	             rc_status = XGE_HAL_ERR_INVALID_PCI_INFO;
	             xge_debug_device(XGE_ERR,
	                  "invalid pci info "XGE_OS_LLXFMT,
	                 (unsigned long long)pci_info);
	             break;
	    }
	    if (rc_status != XGE_HAL_ERR_INVALID_PCI_INFO)
	        xge_debug_device(XGE_TRACE, "PCI info: mode %d width "
	            "%d frequency %d", *pci_mode, *bus_width,
	            *bus_frequency);
	    if (hldev->config.pci_freq_mherz ==
	            XGE_HAL_DEFAULT_USE_HARDCODE) {
	        hldev->config.pci_freq_mherz = *bus_frequency;
	    }
	}
	/* for XENA, we report PCI mode, only. PCI bus frequency, and bus width
	 * are set to unknown */
	else if (card_id == XGE_HAL_CARD_XENA) {
	    u32 pcix_status;
	    u8 dev_num, bus_num;
	    /* initialize defaults for XENA */
	    *bus_frequency  = XGE_HAL_PCI_BUS_FREQUENCY_UNKNOWN;
	    *bus_width  = XGE_HAL_PCI_BUS_WIDTH_UNKNOWN;
	    xge_os_pci_read32(hldev->pdev, hldev->cfgh,
	        xge_offsetof(xge_hal_pci_config_le_t, pcix_status),
	        &pcix_status);
	    dev_num = (u8)((pcix_status & 0xF8) >> 3);
	    bus_num = (u8)((pcix_status & 0xFF00) >> 8);
	    if (dev_num == 0 && bus_num == 0)
	        *pci_mode = XGE_HAL_PCI_BASIC_MODE;
	    else
	        *pci_mode = XGE_HAL_PCIX_BASIC_MODE;
	    xge_debug_device(XGE_TRACE, "PCI info: mode %d", *pci_mode);
	    if (hldev->config.pci_freq_mherz ==
	            XGE_HAL_DEFAULT_USE_HARDCODE) {
	        /*
	         * There is no way to detect BUS frequency on Xena,
	         * so, in case of automatic configuration we hopelessly
	         * assume 133MHZ.
	         */
	        hldev->config.pci_freq_mherz =
	            XGE_HAL_PCI_BUS_FREQUENCY_133MHZ;
	    }
	} else if (card_id == XGE_HAL_CARD_TITAN) {
	    *bus_width = XGE_HAL_PCI_BUS_WIDTH_64BIT;
	    *bus_frequency  = XGE_HAL_PCI_BUS_FREQUENCY_250MHZ;
	    if (hldev->config.pci_freq_mherz ==
	            XGE_HAL_DEFAULT_USE_HARDCODE) {
	        hldev->config.pci_freq_mherz = *bus_frequency;
	    }
	} else{
	    rc_status =  XGE_HAL_ERR_BAD_DEVICE_ID;
	    xge_debug_device(XGE_ERR, "invalid device id %d", card_id);
	}
#endif

	return rc_status;
}

/*
 * __hal_device_handle_link_up_ind
 * @hldev: HAL device handle.
 *
 * Link up indication handler. The function is invoked by HAL when
 * Xframe indicates that the link is up for programmable amount of time.
 */
static int
__hal_device_handle_link_up_ind(xge_hal_device_t *hldev)
{
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;
	u64 val64;

	/*
	 * If the previous link state is not down, return.
	 */
	if (hldev->link_state == XGE_HAL_LINK_UP) {
#ifdef XGE_HAL_PROCESS_LINK_INT_IN_ISR
	    if (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_HERC){
	        val64 = xge_os_pio_mem_read64(
	            hldev->pdev, hldev->regh0,
	            &bar0->misc_int_mask);
	        val64 |= XGE_HAL_MISC_INT_REG_LINK_UP_INT;
	        val64 &= ~XGE_HAL_MISC_INT_REG_LINK_DOWN_INT;
	        xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	            val64, &bar0->misc_int_mask);
	    }
#endif
	    xge_debug_device(XGE_TRACE,
	        "link up indication while link is up, ignoring..");
	    return 0;
	}

	/* Now re-enable it as due to noise, hardware turned it off */
	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                 &bar0->adapter_control);
	val64 |= XGE_HAL_ADAPTER_CNTL_EN;
	val64 = val64 & (~XGE_HAL_ADAPTER_ECC_EN); /* ECC enable */
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	             &bar0->adapter_control);

	/* Turn on the Laser */
	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                &bar0->adapter_control);
	val64 = val64|(XGE_HAL_ADAPTER_EOI_TX_ON |
	        XGE_HAL_ADAPTER_LED_ON);
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	             &bar0->adapter_control);

#ifdef XGE_HAL_PROCESS_LINK_INT_IN_ISR
	if (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_HERC) {
	        val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                          &bar0->adapter_status);
	        if (val64 & (XGE_HAL_ADAPTER_STATUS_RMAC_REMOTE_FAULT |
	                 XGE_HAL_ADAPTER_STATUS_RMAC_LOCAL_FAULT)) {
	            xge_debug_device(XGE_TRACE, "%s",
	                      "fail to transition link to up...");
	        return 0;
	        }
	        else {
	            /*
	             * Mask the Link Up interrupt and unmask the Link Down
	             * interrupt.
	             */
	            val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                              &bar0->misc_int_mask);
	            val64 |= XGE_HAL_MISC_INT_REG_LINK_UP_INT;
	            val64 &= ~XGE_HAL_MISC_INT_REG_LINK_DOWN_INT;
	            xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	                           &bar0->misc_int_mask);
	            xge_debug_device(XGE_TRACE, "calling link up..");
	            hldev->link_state = XGE_HAL_LINK_UP;

	            /* notify ULD */
	            if (g_xge_hal_driver->uld_callbacks.link_up) {
	                g_xge_hal_driver->uld_callbacks.link_up(
	                        hldev->upper_layer_info);
	            }
	        return 1;
	        }
	    }
#endif
	xge_os_mdelay(1);
	if (__hal_device_register_poll(hldev, &bar0->adapter_status, 0,
	        (XGE_HAL_ADAPTER_STATUS_RMAC_REMOTE_FAULT |
	        XGE_HAL_ADAPTER_STATUS_RMAC_LOCAL_FAULT),
	        XGE_HAL_DEVICE_FAULT_WAIT_MAX_MILLIS) == XGE_HAL_OK) {

	    /* notify ULD */
	    (void) xge_queue_produce_context(hldev->queueh,
	                     XGE_HAL_EVENT_LINK_IS_UP,
	                     hldev);
	    /* link is up after been enabled */
	    return 1;
	} else {
	    xge_debug_device(XGE_TRACE, "%s",
	              "fail to transition link to up...");
	    return 0;
	}
}

/*
 * __hal_device_handle_link_down_ind
 * @hldev: HAL device handle.
 *
 * Link down indication handler. The function is invoked by HAL when
 * Xframe indicates that the link is down.
 */
static int
__hal_device_handle_link_down_ind(xge_hal_device_t *hldev)
{
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;
	u64 val64;

	/*
	 * If the previous link state is not up, return.
	 */
	if (hldev->link_state == XGE_HAL_LINK_DOWN) {
#ifdef  XGE_HAL_PROCESS_LINK_INT_IN_ISR
	    if (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_HERC){
	        val64 = xge_os_pio_mem_read64(
	            hldev->pdev, hldev->regh0,
	            &bar0->misc_int_mask);
	        val64 |= XGE_HAL_MISC_INT_REG_LINK_DOWN_INT;
	        val64 &= ~XGE_HAL_MISC_INT_REG_LINK_UP_INT;
	        xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	            val64, &bar0->misc_int_mask);
	    }
#endif
	    xge_debug_device(XGE_TRACE,
	        "link down indication while link is down, ignoring..");
	    return 0;
	}
	xge_os_mdelay(1);

	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                  &bar0->adapter_control);

	/* try to debounce the link only if the adapter is enabled. */
	if (val64 & XGE_HAL_ADAPTER_CNTL_EN) {
	    if (__hal_device_register_poll(hldev, &bar0->adapter_status, 0,
	        (XGE_HAL_ADAPTER_STATUS_RMAC_REMOTE_FAULT |
	        XGE_HAL_ADAPTER_STATUS_RMAC_LOCAL_FAULT),
	        XGE_HAL_DEVICE_FAULT_WAIT_MAX_MILLIS) == XGE_HAL_OK) {
	        xge_debug_device(XGE_TRACE,
	            "link is actually up (possible noisy link?), ignoring.");
	        return(0);
	    }
	}

	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                &bar0->adapter_control);
	/* turn off LED */
	val64 = val64 & (~XGE_HAL_ADAPTER_LED_ON);
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	               &bar0->adapter_control);

#ifdef  XGE_HAL_PROCESS_LINK_INT_IN_ISR
	if (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_HERC) {
	    /*
	     * Mask the Link Down interrupt and unmask the Link up
	     * interrupt
	     */
	    val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                      &bar0->misc_int_mask);
	    val64 |= XGE_HAL_MISC_INT_REG_LINK_DOWN_INT;
	    val64 &= ~XGE_HAL_MISC_INT_REG_LINK_UP_INT;
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	                   &bar0->misc_int_mask);

	    /* link is down */
	    xge_debug_device(XGE_TRACE, "calling link down..");
	    hldev->link_state = XGE_HAL_LINK_DOWN;

	    /* notify ULD */
	    if (g_xge_hal_driver->uld_callbacks.link_down) {
	            g_xge_hal_driver->uld_callbacks.link_down(
	                hldev->upper_layer_info);
	    }
	    return 1;
	}
#endif
	/* notify ULD */
	(void) xge_queue_produce_context(hldev->queueh,
	                 XGE_HAL_EVENT_LINK_IS_DOWN,
	                 hldev);
	/* link is down */
	return 1;
}
/*
 * __hal_device_handle_link_state_change
 * @hldev: HAL device handle.
 *
 * Link state change handler. The function is invoked by HAL when
 * Xframe indicates link state change condition. The code here makes sure to
 * 1) ignore redundant state change indications;
 * 2) execute link-up sequence, and handle the failure to bring the link up;
 * 3) generate XGE_HAL_LINK_UP/DOWN event for the subsequent handling by
 *    upper-layer driver (ULD).
 */
static int
__hal_device_handle_link_state_change(xge_hal_device_t *hldev)
{
	u64 hw_status;
	int hw_link_state;
	int retcode;
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;
	u64 val64;
	int i = 0;

	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                &bar0->adapter_control);

	/* If the adapter is not enabled but the hal thinks we are in the up
	 * state then transition to the down state.
	 */
	if ( !(val64 & XGE_HAL_ADAPTER_CNTL_EN) &&
	     (hldev->link_state == XGE_HAL_LINK_UP) ) {
	    return(__hal_device_handle_link_down_ind(hldev));
	}

	do {
	    xge_os_mdelay(1);
	    (void) xge_hal_device_status(hldev, &hw_status);
	    hw_link_state = (hw_status &
	        (XGE_HAL_ADAPTER_STATUS_RMAC_REMOTE_FAULT |
	            XGE_HAL_ADAPTER_STATUS_RMAC_LOCAL_FAULT)) ?
	            XGE_HAL_LINK_DOWN : XGE_HAL_LINK_UP;

	    /* check if the current link state is still considered
	     * to be changed. This way we will make sure that this is
	     * not a noise which needs to be filtered out */
	    if (hldev->link_state == hw_link_state)
	        break;
	} while (i++ < hldev->config.link_valid_cnt);

	/* If the current link state is same as previous, just return */
	if (hldev->link_state == hw_link_state)
	    retcode = 0;
	/* detected state change */
	else if (hw_link_state == XGE_HAL_LINK_UP)
	    retcode = __hal_device_handle_link_up_ind(hldev);
	else
	    retcode = __hal_device_handle_link_down_ind(hldev);
	return retcode;
}

/*
 *
 */
static void
__hal_device_handle_serr(xge_hal_device_t *hldev, char *reg, u64 value)
{
	hldev->stats.sw_dev_err_stats.serr_cnt++;
	if (hldev->config.dump_on_serr) {
#ifdef XGE_HAL_USE_MGMT_AUX
	    (void) xge_hal_aux_device_dump(hldev);
#endif
	}

	(void) xge_queue_produce(hldev->queueh, XGE_HAL_EVENT_SERR, hldev,
	           1, sizeof(u64), (void *)&value);

	xge_debug_device(XGE_ERR, "%s: read "XGE_OS_LLXFMT, reg,
	              (unsigned long long) value);
}

/*
 *
 */
static void
__hal_device_handle_eccerr(xge_hal_device_t *hldev, char *reg, u64 value)
{
	if (hldev->config.dump_on_eccerr) {
#ifdef XGE_HAL_USE_MGMT_AUX
	    (void) xge_hal_aux_device_dump(hldev);
#endif
	}

	/* Herc smart enough to recover on its own! */
	if (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_XENA) {
	    (void) xge_queue_produce(hldev->queueh,
	        XGE_HAL_EVENT_ECCERR, hldev,
	        1, sizeof(u64), (void *)&value);
	}

	    xge_debug_device(XGE_ERR, "%s: read "XGE_OS_LLXFMT, reg,
	                              (unsigned long long) value);
}

/*
 *
 */
static void
__hal_device_handle_parityerr(xge_hal_device_t *hldev, char *reg, u64 value)
{
	if (hldev->config.dump_on_parityerr) {
#ifdef XGE_HAL_USE_MGMT_AUX
	    (void) xge_hal_aux_device_dump(hldev);
#endif
	}
	(void) xge_queue_produce_context(hldev->queueh,
	        XGE_HAL_EVENT_PARITYERR, hldev);

	    xge_debug_device(XGE_ERR, "%s: read "XGE_OS_LLXFMT, reg,
	                              (unsigned long long) value);
}

/*
 *
 */
static void
__hal_device_handle_targetabort(xge_hal_device_t *hldev)
{
	(void) xge_queue_produce_context(hldev->queueh,
	        XGE_HAL_EVENT_TARGETABORT, hldev);
}


/*
 * __hal_device_hw_initialize
 * @hldev: HAL device handle.
 *
 * Initialize Xframe hardware.
 */
static xge_hal_status_e
__hal_device_hw_initialize(xge_hal_device_t *hldev)
{
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;
	xge_hal_status_e status;
	u64 val64;

	/* Set proper endian settings and verify the same by reading the PIF
	 * Feed-back register. */
	status = __hal_device_set_swapper(hldev);
	if (status != XGE_HAL_OK) {
	    return status;
	}

	/* update the pci mode, frequency, and width */
	if (__hal_device_pci_info_get(hldev, &hldev->pci_mode,
	    &hldev->bus_frequency, &hldev->bus_width) != XGE_HAL_OK){
	    hldev->pci_mode = XGE_HAL_PCI_INVALID_MODE;
	    hldev->bus_frequency = XGE_HAL_PCI_BUS_FREQUENCY_UNKNOWN;
	    hldev->bus_width = XGE_HAL_PCI_BUS_WIDTH_UNKNOWN;
	    /*
	     * FIXME: this cannot happen.
	     * But if it happens we cannot continue just like that
	     */
	    xge_debug_device(XGE_ERR, "unable to get pci info");
	}

	if ((hldev->pci_mode == XGE_HAL_PCI_33MHZ_MODE) ||
	    (hldev->pci_mode == XGE_HAL_PCI_66MHZ_MODE) ||
	    (hldev->pci_mode == XGE_HAL_PCI_BASIC_MODE)) {
	    /* PCI optimization: set TxReqTimeOut
	     * register (0x800+0x120) to 0x1ff or
	     * something close to this.
	     * Note: not to be used for PCI-X! */

	    val64 = XGE_HAL_TXREQTO_VAL(0x1FF);
	    val64 |= XGE_HAL_TXREQTO_EN;
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	                 &bar0->txreqtimeout);

	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, 0ULL,
	                 &bar0->read_retry_delay);

	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, 0ULL,
	                 &bar0->write_retry_delay);

	    xge_debug_device(XGE_TRACE, "%s", "optimizing for PCI mode");
	}

	if (hldev->bus_frequency == XGE_HAL_PCI_BUS_FREQUENCY_266MHZ ||
	    hldev->bus_frequency == XGE_HAL_PCI_BUS_FREQUENCY_250MHZ) {

	    /* Optimizing for PCI-X 266/250 */

	    val64 = XGE_HAL_TXREQTO_VAL(0x7F);
	    val64 |= XGE_HAL_TXREQTO_EN;
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	                 &bar0->txreqtimeout);

	    xge_debug_device(XGE_TRACE, "%s", "optimizing for PCI-X 266/250 modes");
	}

	if (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_HERC) {
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, 0x4000000000000ULL,
	                 &bar0->read_retry_delay);

	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, 0x4000000000000ULL,
	                 &bar0->write_retry_delay);
	}

	/* added this to set the no of bytes used to update lso_bytes_sent
	   returned TxD0 */
	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                  &bar0->pic_control_2);
	val64 &= ~XGE_HAL_TXD_WRITE_BC(0x2);
	val64 |= XGE_HAL_TXD_WRITE_BC(0x4);
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	               &bar0->pic_control_2);
	/* added this to clear the EOI_RESET field while leaving XGXS_RESET
	 * in reset, then a 1-second delay */
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	        XGE_HAL_SW_RESET_XGXS, &bar0->sw_reset);
	xge_os_mdelay(1000);

	/* Clear the XGXS_RESET field of the SW_RESET register in order to
	 * release the XGXS from reset. Its reset value is 0xA5; write 0x00
	 * to activate the XGXS. The core requires a minimum 500 us reset.*/
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, 0, &bar0->sw_reset);
	(void) xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	            &bar0->sw_reset);
	xge_os_mdelay(1);

	/* read registers in all blocks */
	(void) xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	               &bar0->mac_int_mask);
	(void) xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	               &bar0->mc_int_mask);
	(void) xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	               &bar0->xgxs_int_mask);

	/* set default MTU and steer based on length*/
	__hal_ring_mtu_set(hldev, hldev->config.mtu+22); // Alway set 22 bytes extra for steering to work

	if (hldev->config.mac.rmac_bcast_en) {
	    xge_hal_device_bcast_enable(hldev);
	} else {
	    xge_hal_device_bcast_disable(hldev);
	}

#ifndef XGE_HAL_HERC_EMULATION
	__hal_device_xaui_configure(hldev);
#endif
	__hal_device_mac_link_util_set(hldev);

	__hal_device_mac_link_util_set(hldev);

	/*
	 * Keep its PCI REQ# line asserted during a write
	 * transaction up to the end of the transaction
	 */
	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	            &bar0->misc_control);

	val64 |= XGE_HAL_MISC_CONTROL_EXT_REQ_EN;

	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	            val64, &bar0->misc_control);

	if (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_HERC) {
	    val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                &bar0->misc_control);

	    val64 |= XGE_HAL_MISC_CONTROL_LINK_FAULT;

	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	                val64, &bar0->misc_control);
	}

	/*
	 * bimodal interrupts is when all Rx traffic interrupts
	 * will go to TTI, so we need to adjust RTI settings and
	 * use adaptive TTI timer. We need to make sure RTI is
	 * properly configured to sane value which will not
	 * distrupt bimodal behavior.
	 */
	if (hldev->config.bimodal_interrupts) {
	    int i;

	    /* force polling_cnt to be "0", otherwise
	     * IRQ workload statistics will be screwed. This could
	     * be worked out in TXPIC handler later. */
	    hldev->config.isr_polling_cnt = 0;
	    hldev->config.sched_timer_us = 10000;

	    /* disable all TTI < 56 */
	    for (i=0; i<XGE_HAL_MAX_FIFO_NUM; i++) {
	        int j;
	        if (!hldev->config.fifo.queue[i].configured)
	            continue;
	        for (j=0; j<XGE_HAL_MAX_FIFO_TTI_NUM; j++) {
	            if (hldev->config.fifo.queue[i].tti[j].enabled)
	            hldev->config.fifo.queue[i].tti[j].enabled = 0;
	        }
	    }

	    /* now configure bimodal interrupts */
	    __hal_device_bimodal_configure(hldev);
	}

	status = __hal_device_tti_configure(hldev, 0);
	if (status != XGE_HAL_OK)
	    return status;

	status = __hal_device_rti_configure(hldev, 0);
	if (status != XGE_HAL_OK)
	    return status;

	status = __hal_device_rth_it_configure(hldev);
	if (status != XGE_HAL_OK)
	    return status;

	status = __hal_device_rth_spdm_configure(hldev);
	if (status != XGE_HAL_OK)
	    return status;

	status = __hal_device_rts_mac_configure(hldev);
	if (status != XGE_HAL_OK) {
	    xge_debug_device(XGE_ERR, "__hal_device_rts_mac_configure Failed ");
	    return status;
	}

	status = __hal_device_rts_port_configure(hldev);
	if (status != XGE_HAL_OK) {
	    xge_debug_device(XGE_ERR, "__hal_device_rts_port_configure Failed ");
	    return status;
	}

	status = __hal_device_rts_qos_configure(hldev);
	if (status != XGE_HAL_OK) {
	    xge_debug_device(XGE_ERR, "__hal_device_rts_qos_configure Failed ");
	    return status;
	}

	__hal_device_pause_frames_configure(hldev);
	__hal_device_rmac_padding_configure(hldev);
	__hal_device_shared_splits_configure(hldev);

	/* make sure all interrupts going to be disabled at the moment */
	__hal_device_intr_mgmt(hldev, XGE_HAL_ALL_INTRS, 0);

	/* SXE-008 Transmit DMA arbitration issue */
	if (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_XENA &&
	    hldev->revision < 4) {
	    xge_os_pio_mem_write64(hldev->pdev,hldev->regh0,
	            XGE_HAL_ADAPTER_PCC_ENABLE_FOUR,
	            &bar0->pcc_enable);
	}
#if 0  // Removing temporarily as FreeBSD is seeing lower performance 
	   // attributable to this fix. 
	/* SXE-2-010 */
	if (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_HERC) {
	    /* Turn off the ECC error reporting for RLDRAM interface */
	    if ((status = xge_hal_fix_rldram_ecc_error(hldev)) != XGE_HAL_OK)
	        return status;
	}
#endif 
	__hal_fifo_hw_initialize(hldev);
	__hal_ring_hw_initialize(hldev);

	if (__hal_device_wait_quiescent(hldev, &val64)) {
	    return XGE_HAL_ERR_DEVICE_IS_NOT_QUIESCENT;
	}

	if (__hal_device_register_poll(hldev, &bar0->adapter_status, 1,
	    XGE_HAL_ADAPTER_STATUS_RC_PRC_QUIESCENT,
	     XGE_HAL_DEVICE_QUIESCENT_WAIT_MAX_MILLIS) != XGE_HAL_OK) {
	    xge_debug_device(XGE_TRACE, "%s", "PRC is not QUIESCENT!");
	    return XGE_HAL_ERR_DEVICE_IS_NOT_QUIESCENT;
	}

	xge_debug_device(XGE_TRACE, "device 0x"XGE_OS_LLXFMT" is quiescent",
	          (unsigned long long)(ulong_t)hldev);

	if (hldev->config.intr_mode == XGE_HAL_INTR_MODE_MSIX ||
	    hldev->config.intr_mode == XGE_HAL_INTR_MODE_MSI) {
	    /*
	     * If MSI is enabled, ensure that One Shot for MSI in PCI_CTRL
	     * is disabled.
	     */
	    val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                    &bar0->pic_control);
	    val64 &= ~(XGE_HAL_PIC_CNTL_ONE_SHOT_TINT);
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	                    &bar0->pic_control);
	}

	hldev->hw_is_initialized = 1;
	hldev->terminating = 0;
	return XGE_HAL_OK;
}

/*
 * __hal_device_reset - Reset device only.
 * @hldev: HAL device handle.
 *
 * Reset the device, and subsequently restore
 * the previously saved PCI configuration space.
 */
#define XGE_HAL_MAX_PCI_CONFIG_SPACE_REINIT 50
static xge_hal_status_e
__hal_device_reset(xge_hal_device_t *hldev)
{
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;
	int i, j, swap_done, pcisize = 0;
	u64 val64, rawval = 0ULL;

	if (hldev->config.intr_mode == XGE_HAL_INTR_MODE_MSIX) {
	    if (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_HERC) {
	        if ( hldev->bar2 ) {
	            u64 *msix_vetor_table = (u64 *)hldev->bar2;

	            // 2 64bit words for each entry
	            for (i = 0; i < XGE_HAL_MAX_MSIX_MESSAGES * 2;
	                 i++) {
	                  hldev->msix_vector_table[i] =
	                   xge_os_pio_mem_read64(hldev->pdev,
	                          hldev->regh2, &msix_vetor_table[i]);
	            }
	        }
	    }
	}
	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                            &bar0->pif_rd_swapper_fb);
	swap_done = (val64 == XGE_HAL_IF_RD_SWAPPER_FB);

	if (swap_done) {
	    __hal_pio_mem_write32_upper(hldev->pdev, hldev->regh0,
	         (u32)(XGE_HAL_SW_RESET_ALL>>32), (char *)&bar0->sw_reset);
	} else {
	    u32 val = (u32)(XGE_HAL_SW_RESET_ALL >> 32);
#if defined(XGE_OS_HOST_LITTLE_ENDIAN) || defined(XGE_OS_PIO_LITTLE_ENDIAN)
	    /* swap it */
	    val = (((val & (u32)0x000000ffUL) << 24) |
	           ((val & (u32)0x0000ff00UL) <<  8) |
	           ((val & (u32)0x00ff0000UL) >>  8) |
	           ((val & (u32)0xff000000UL) >> 24));
#endif
	    xge_os_pio_mem_write32(hldev->pdev, hldev->regh0, val,
	                 &bar0->sw_reset);
	}

	pcisize = (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_HERC)?
	           XGE_HAL_PCISIZE_HERC : XGE_HAL_PCISIZE_XENA;

	xge_os_mdelay(20); /* Wait for 20 ms after reset */

	{
	    /* Poll for no more than 1 second */
	    for (i = 0; i < XGE_HAL_MAX_PCI_CONFIG_SPACE_REINIT; i++)
	    {
	        for (j = 0; j < pcisize; j++) {
	            xge_os_pci_write32(hldev->pdev, hldev->cfgh, j * 4,
	                *((u32*)&hldev->pci_config_space + j));
	        }

	        xge_os_pci_read16(hldev->pdev,hldev->cfgh,
	            xge_offsetof(xge_hal_pci_config_le_t, device_id),
	            &hldev->device_id);

	        if (xge_hal_device_check_id(hldev) != XGE_HAL_CARD_UNKNOWN)
	            break;
	        xge_os_mdelay(20);
	    }
	}

	if (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_UNKNOWN)
	{
	    xge_debug_device(XGE_ERR, "device reset failed");
	        return XGE_HAL_ERR_RESET_FAILED;
	}

	if (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_HERC) {
	    int cnt = 0;

	    rawval = XGE_HAL_SW_RESET_RAW_VAL_HERC;
	    pcisize = XGE_HAL_PCISIZE_HERC;
	    xge_os_mdelay(1);
	    do {
	        val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	            &bar0->sw_reset);
	        if (val64 != rawval) {
	            break;
	        }
	        cnt++;
	        xge_os_mdelay(1); /* Wait for 1ms before retry */
	    } while(cnt < 20);
	} else if (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_XENA) {
	    rawval = XGE_HAL_SW_RESET_RAW_VAL_XENA;
	    pcisize = XGE_HAL_PCISIZE_XENA;
	    xge_os_mdelay(XGE_HAL_DEVICE_RESET_WAIT_MAX_MILLIS);
	}

	/* Restore MSI-X vector table */
	if (hldev->config.intr_mode == XGE_HAL_INTR_MODE_MSIX) {
	    if (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_HERC) {
	        if ( hldev->bar2 ) {
	            /*
	             * 94: MSIXTable 00000004  ( BIR:4  Offset:0x0 )
	             * 98: PBATable  00000404  ( BIR:4  Offset:0x400 )
	             */
	             u64 *msix_vetor_table = (u64 *)hldev->bar2;

	             /* 2 64bit words for each entry */
	             for (i = 0; i < XGE_HAL_MAX_MSIX_MESSAGES * 2;
	              i++) {
	                 xge_os_pio_mem_write64(hldev->pdev,
	                hldev->regh2,
	                hldev->msix_vector_table[i],
	                &msix_vetor_table[i]);
	             }
	        }
	    }
	}

	hldev->link_state = XGE_HAL_LINK_DOWN;
	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                                  &bar0->sw_reset);

	if (val64 != rawval) {
	    xge_debug_device(XGE_ERR, "device has not been reset "
	        "got 0x"XGE_OS_LLXFMT", expected 0x"XGE_OS_LLXFMT,
	        (unsigned long long)val64, (unsigned long long)rawval);
	        return XGE_HAL_ERR_RESET_FAILED;
	}

	hldev->hw_is_initialized = 0;
	return XGE_HAL_OK;
}

/*
 * __hal_device_poll - General private routine to poll the device.
 * @hldev: HAL device handle.
 *
 * Returns: one of the xge_hal_status_e{} enumerated types.
 * XGE_HAL_OK           - for success.
 * XGE_HAL_ERR_CRITICAL         - when encounters critical error.
 */
static xge_hal_status_e
__hal_device_poll(xge_hal_device_t *hldev)
{
	xge_hal_pci_bar0_t *bar0;
	u64 err_reg;

	bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;

	/* Handling SERR errors by forcing a H/W reset. */
	err_reg = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                  &bar0->serr_source);
	if (err_reg & XGE_HAL_SERR_SOURCE_ANY) {
	    __hal_device_handle_serr(hldev, "serr_source", err_reg);
	    return XGE_HAL_ERR_CRITICAL;
	}

	err_reg = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                &bar0->misc_int_reg);

	if (err_reg & XGE_HAL_MISC_INT_REG_DP_ERR_INT) {
	    hldev->stats.sw_dev_err_stats.parity_err_cnt++;
	    __hal_device_handle_parityerr(hldev, "misc_int_reg", err_reg);
	    return XGE_HAL_ERR_CRITICAL;
	}

#ifdef  XGE_HAL_PROCESS_LINK_INT_IN_ISR
	if (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_XENA)
#endif
	{

	    /* Handling link status change error Intr */
	    err_reg = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                    &bar0->mac_rmac_err_reg);
	    if (__hal_device_handle_link_state_change(hldev))
	        xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	                   err_reg, &bar0->mac_rmac_err_reg);
	}

	if (hldev->inject_serr != 0) {
	    err_reg = hldev->inject_serr;
	    hldev->inject_serr = 0;
	    __hal_device_handle_serr(hldev, "inject_serr", err_reg);
	    return XGE_HAL_ERR_CRITICAL;
	    }

	    if (hldev->inject_ecc != 0) {
	            err_reg = hldev->inject_ecc;
	            hldev->inject_ecc = 0;
	    hldev->stats.sw_dev_err_stats.ecc_err_cnt++;
	            __hal_device_handle_eccerr(hldev, "inject_ecc", err_reg);
	    return XGE_HAL_ERR_CRITICAL;
	    }

	if (hldev->inject_bad_tcode != 0) {
	    u8 t_code = hldev->inject_bad_tcode;
	    xge_hal_channel_t channel;
	    xge_hal_fifo_txd_t txd;
	    xge_hal_ring_rxd_1_t rxd;

	    channel.devh =  hldev;

	    if (hldev->inject_bad_tcode_for_chan_type ==
	                    XGE_HAL_CHANNEL_TYPE_FIFO) {
	        channel.type = XGE_HAL_CHANNEL_TYPE_FIFO;

	    } else {
	        channel.type = XGE_HAL_CHANNEL_TYPE_RING;
	    }

	            hldev->inject_bad_tcode = 0;

	    if (channel.type == XGE_HAL_CHANNEL_TYPE_FIFO)
	        return xge_hal_device_handle_tcode(&channel, &txd,
	                                           t_code);
	    else
	        return xge_hal_device_handle_tcode(&channel, &rxd,
	                                           t_code);
	    }

	return XGE_HAL_OK;
}

/*
 * __hal_verify_pcc_idle - Verify All Enbled PCC are IDLE or not
 * @hldev: HAL device handle.
 * @adp_status: Adapter Status value
 * Usage: See xge_hal_device_enable{}.
 */
xge_hal_status_e
__hal_verify_pcc_idle(xge_hal_device_t *hldev, u64 adp_status)
{
	if (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_XENA &&
	    hldev->revision < 4) {
	    /*
	     * For Xena 1,2,3 we enable only 4 PCCs Due to
	     * SXE-008 (Transmit DMA arbitration issue)
	     */
	    if ((adp_status & XGE_HAL_ADAPTER_STATUS_RMAC_PCC_4_IDLE)
	        != XGE_HAL_ADAPTER_STATUS_RMAC_PCC_4_IDLE) {
	        xge_debug_device(XGE_TRACE, "%s",
	            "PCC is not IDLE after adapter enabled!");
	        return XGE_HAL_ERR_DEVICE_IS_NOT_QUIESCENT;
	    }
	} else {
	    if ((adp_status & XGE_HAL_ADAPTER_STATUS_RMAC_PCC_IDLE) !=
	        XGE_HAL_ADAPTER_STATUS_RMAC_PCC_IDLE) {
	        xge_debug_device(XGE_TRACE, "%s",
	        "PCC is not IDLE after adapter enabled!");
	        return XGE_HAL_ERR_DEVICE_IS_NOT_QUIESCENT;
	    }
	}
	return XGE_HAL_OK;
}

static void
__hal_update_bimodal(xge_hal_device_t *hldev, int ring_no)
{
	int tval, d, iwl_avg, len_avg, bytes_avg, bytes_hist, d_hist;
	int iwl_rxcnt, iwl_txcnt, iwl_txavg, len_rxavg, iwl_rxavg, len_txavg;
	int iwl_cnt, i;

#define _HIST_SIZE  50 /* 0.5 sec history */
#define _HIST_ADJ_TIMER 1
#define _STEP       2

	static int bytes_avg_history[_HIST_SIZE] = {0};
	static int d_avg_history[_HIST_SIZE] = {0};
	static int history_idx = 0;
	static int pstep = 1;
	static int hist_adj_timer = 0;

	/*
	 * tval - current value of this bimodal timer
	 */
	tval = hldev->bimodal_tti[ring_no].timer_val_us;

	/*
	 * d - how many interrupts we were getting since last
	 *     bimodal timer tick.
	 */
	d = hldev->stats.sw_dev_info_stats.tx_traffic_intr_cnt -
	    hldev->bimodal_intr_cnt;

	/* advance bimodal interrupt counter */
	hldev->bimodal_intr_cnt =
	    hldev->stats.sw_dev_info_stats.tx_traffic_intr_cnt;

	/*
	 * iwl_cnt - how many interrupts we've got since last
	 *           bimodal timer tick.
	 */
	iwl_rxcnt = (hldev->irq_workload_rxcnt[ring_no] ?
	                 hldev->irq_workload_rxcnt[ring_no] : 1);
	iwl_txcnt = (hldev->irq_workload_txcnt[ring_no] ?
	                 hldev->irq_workload_txcnt[ring_no] : 1);
	iwl_cnt = iwl_rxcnt + iwl_txcnt;
	iwl_cnt = iwl_cnt; /* just to remove the lint warning */

	/*
	 * we need to take hldev->config.isr_polling_cnt into account
	 * but for some reason this line causing GCC to produce wrong
	 * code on Solaris. As of now, if bimodal_interrupts is configured
	 * hldev->config.isr_polling_cnt is forced to be "0".
	 *
	 * iwl_cnt = iwl_cnt / (hldev->config.isr_polling_cnt + 1); */

	/*
	 * iwl_avg - how many RXDs on avarage been processed since
	 *           last bimodal timer tick. This indirectly includes
	 *           CPU utilizations.
	 */
	iwl_rxavg = hldev->irq_workload_rxd[ring_no] / iwl_rxcnt;
	iwl_txavg = hldev->irq_workload_txd[ring_no] / iwl_txcnt;
	iwl_avg = iwl_rxavg + iwl_txavg;
	iwl_avg = iwl_avg == 0 ? 1 : iwl_avg;

	/*
	 * len_avg - how many bytes on avarage been processed since
	 *           last bimodal timer tick. i.e. avarage frame size.
	 */
	len_rxavg = 1 + hldev->irq_workload_rxlen[ring_no] /
	           (hldev->irq_workload_rxd[ring_no] ?
	            hldev->irq_workload_rxd[ring_no] : 1);
	len_txavg = 1 + hldev->irq_workload_txlen[ring_no] /
	           (hldev->irq_workload_txd[ring_no] ?
	            hldev->irq_workload_txd[ring_no] : 1);
	len_avg = len_rxavg + len_txavg;
	if (len_avg < 60)
	    len_avg = 60;

	/* align on low boundary */
	if ((tval -_STEP) < hldev->config.bimodal_timer_lo_us)
	    tval = hldev->config.bimodal_timer_lo_us;

	/* reset faster */
	if (iwl_avg == 1) {
	    tval = hldev->config.bimodal_timer_lo_us;
	    /* reset history */
	    for (i = 0; i < _HIST_SIZE; i++)
	        bytes_avg_history[i] = d_avg_history[i] = 0;
	    history_idx = 0;
	    pstep = 1;
	    hist_adj_timer = 0;
	}

	/* always try to ajust timer to the best throughput value */
	bytes_avg = iwl_avg * len_avg;
	history_idx %= _HIST_SIZE;
	bytes_avg_history[history_idx] = bytes_avg;
	d_avg_history[history_idx] = d;
	history_idx++;
	d_hist = bytes_hist = 0;
	for (i = 0; i < _HIST_SIZE; i++) {
	    /* do not re-configure until history is gathered */
	    if (!bytes_avg_history[i]) {
	        tval = hldev->config.bimodal_timer_lo_us;
	        goto _end;
	    }
	    bytes_hist += bytes_avg_history[i];
	    d_hist += d_avg_history[i];
	}
	bytes_hist /= _HIST_SIZE;
	d_hist /= _HIST_SIZE;

//  xge_os_printf("d %d iwl_avg %d len_avg %d:%d:%d tval %d avg %d hist %d pstep %d",
//            d, iwl_avg, len_txavg, len_rxavg, len_avg, tval, d*bytes_avg,
//            d_hist*bytes_hist, pstep);

	/* make an adaptive step */
	if (d * bytes_avg < d_hist * bytes_hist && hist_adj_timer++ > _HIST_ADJ_TIMER) {
	    pstep = !pstep;
	    hist_adj_timer = 0;
	}

	if (pstep &&
	    (tval + _STEP) <= hldev->config.bimodal_timer_hi_us) {
	    tval += _STEP;
	    hldev->stats.sw_dev_info_stats.bimodal_hi_adjust_cnt++;
	} else if ((tval - _STEP) >= hldev->config.bimodal_timer_lo_us) {
	    tval -= _STEP;
	    hldev->stats.sw_dev_info_stats.bimodal_lo_adjust_cnt++;
	}

	/* enable TTI range A for better latencies */
	hldev->bimodal_urange_a_en = 0;
	if (tval <= hldev->config.bimodal_timer_lo_us && iwl_avg > 2)
	    hldev->bimodal_urange_a_en = 1;

_end:
	/* reset workload statistics counters */
	hldev->irq_workload_rxcnt[ring_no] = 0;
	hldev->irq_workload_rxd[ring_no] = 0;
	hldev->irq_workload_rxlen[ring_no] = 0;
	hldev->irq_workload_txcnt[ring_no] = 0;
	hldev->irq_workload_txd[ring_no] = 0;
	hldev->irq_workload_txlen[ring_no] = 0;

	/* reconfigure TTI56 + ring_no with new timer value */
	hldev->bimodal_timer_val_us = tval;
	(void) __hal_device_rti_configure(hldev, 1);
}

static void
__hal_update_rxufca(xge_hal_device_t *hldev, int ring_no)
{
	int ufc, ic, i;

	ufc = hldev->config.ring.queue[ring_no].rti.ufc_a;
	ic = hldev->stats.sw_dev_info_stats.rx_traffic_intr_cnt;

	/* urange_a adaptive coalescing */
	if (hldev->rxufca_lbolt > hldev->rxufca_lbolt_time) {
	    if (ic > hldev->rxufca_intr_thres) {
	        if (ufc < hldev->config.rxufca_hi_lim) {
	            ufc += 1;
	            for (i=0; i<XGE_HAL_MAX_RING_NUM; i++)
	               hldev->config.ring.queue[i].rti.ufc_a = ufc;
	            (void) __hal_device_rti_configure(hldev, 1);
	            hldev->stats.sw_dev_info_stats.
	                rxufca_hi_adjust_cnt++;
	        }
	        hldev->rxufca_intr_thres = ic +
	            hldev->config.rxufca_intr_thres; /* def: 30 */
	    } else {
	        if (ufc > hldev->config.rxufca_lo_lim) {
	            ufc -= 1;
	            for (i=0; i<XGE_HAL_MAX_RING_NUM; i++)
	               hldev->config.ring.queue[i].rti.ufc_a = ufc;
	            (void) __hal_device_rti_configure(hldev, 1);
	            hldev->stats.sw_dev_info_stats.
	                rxufca_lo_adjust_cnt++;
	        }
	    }
	    hldev->rxufca_lbolt_time = hldev->rxufca_lbolt +
	        hldev->config.rxufca_lbolt_period;
	}
	hldev->rxufca_lbolt++;
}

/*
 * __hal_device_handle_mc - Handle MC interrupt reason
 * @hldev: HAL device handle.
 * @reason: interrupt reason
 */
xge_hal_status_e
__hal_device_handle_mc(xge_hal_device_t *hldev, u64 reason)
{
	xge_hal_pci_bar0_t *isrbar0 =
	        (xge_hal_pci_bar0_t *)(void *)hldev->isrbar0;
	u64 val64;

	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	            &isrbar0->mc_int_status);
	if (!(val64 & XGE_HAL_MC_INT_STATUS_MC_INT))
	    return XGE_HAL_OK;

	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	            &isrbar0->mc_err_reg);
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	            val64, &isrbar0->mc_err_reg);

	if (val64 & XGE_HAL_MC_ERR_REG_ETQ_ECC_SG_ERR_L ||
	    val64 & XGE_HAL_MC_ERR_REG_ETQ_ECC_SG_ERR_U ||
	    val64 & XGE_HAL_MC_ERR_REG_MIRI_ECC_SG_ERR_0 ||
	    val64 & XGE_HAL_MC_ERR_REG_MIRI_ECC_SG_ERR_1 ||
	    (xge_hal_device_check_id(hldev) != XGE_HAL_CARD_XENA &&
	     (val64 & XGE_HAL_MC_ERR_REG_ITQ_ECC_SG_ERR_L ||
	      val64 & XGE_HAL_MC_ERR_REG_ITQ_ECC_SG_ERR_U ||
	      val64 & XGE_HAL_MC_ERR_REG_RLD_ECC_SG_ERR_L ||
	      val64 & XGE_HAL_MC_ERR_REG_RLD_ECC_SG_ERR_U))) {
	    hldev->stats.sw_dev_err_stats.single_ecc_err_cnt++;
	    hldev->stats.sw_dev_err_stats.ecc_err_cnt++;
	}

	if (val64 & XGE_HAL_MC_ERR_REG_ETQ_ECC_DB_ERR_L ||
	    val64 & XGE_HAL_MC_ERR_REG_ETQ_ECC_DB_ERR_U ||
	    val64 & XGE_HAL_MC_ERR_REG_MIRI_ECC_DB_ERR_0 ||
	    val64 & XGE_HAL_MC_ERR_REG_MIRI_ECC_DB_ERR_1 ||
	    (xge_hal_device_check_id(hldev) != XGE_HAL_CARD_XENA &&
	     (val64 & XGE_HAL_MC_ERR_REG_ITQ_ECC_DB_ERR_L ||
	      val64 & XGE_HAL_MC_ERR_REG_ITQ_ECC_DB_ERR_U ||
	      val64 & XGE_HAL_MC_ERR_REG_RLD_ECC_DB_ERR_L ||
	      val64 & XGE_HAL_MC_ERR_REG_RLD_ECC_DB_ERR_U))) {
	    hldev->stats.sw_dev_err_stats.double_ecc_err_cnt++;
	    hldev->stats.sw_dev_err_stats.ecc_err_cnt++;
	}

	if (val64 & XGE_HAL_MC_ERR_REG_SM_ERR) {
	    hldev->stats.sw_dev_err_stats.sm_err_cnt++;
	}

	/* those two should result in device reset */
	if (val64 & XGE_HAL_MC_ERR_REG_MIRI_ECC_DB_ERR_0 ||
	    val64 & XGE_HAL_MC_ERR_REG_MIRI_ECC_DB_ERR_1) {
	            __hal_device_handle_eccerr(hldev, "mc_err_reg", val64);
	    return XGE_HAL_ERR_CRITICAL;
	}

	return XGE_HAL_OK;
}

/*
 * __hal_device_handle_pic - Handle non-traffic PIC interrupt reason
 * @hldev: HAL device handle.
 * @reason: interrupt reason
 */
xge_hal_status_e
__hal_device_handle_pic(xge_hal_device_t *hldev, u64 reason)
{
	xge_hal_pci_bar0_t *isrbar0 =
	        (xge_hal_pci_bar0_t *)(void *)hldev->isrbar0;
	u64 val64;

	if (reason & XGE_HAL_PIC_INT_FLSH) {
	    val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                &isrbar0->flsh_int_reg);
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	                   val64, &isrbar0->flsh_int_reg);
	    /* FIXME: handle register */
	}
	if (reason & XGE_HAL_PIC_INT_MDIO) {
	    val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                &isrbar0->mdio_int_reg);
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	                   val64, &isrbar0->mdio_int_reg);
	    /* FIXME: handle register */
	}
	if (reason & XGE_HAL_PIC_INT_IIC) {
	    val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                &isrbar0->iic_int_reg);
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	                   val64, &isrbar0->iic_int_reg);
	    /* FIXME: handle register */
	}
	if (reason & XGE_HAL_PIC_INT_MISC) {
	    val64 = xge_os_pio_mem_read64(hldev->pdev,
	            hldev->regh0, &isrbar0->misc_int_reg);
#ifdef XGE_HAL_PROCESS_LINK_INT_IN_ISR
	    if (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_HERC) {
	        /*  Check for Link interrupts. If both Link Up/Down
	         *  bits are set, clear both and check adapter status
	         */
	        if ((val64 & XGE_HAL_MISC_INT_REG_LINK_UP_INT) &&
	            (val64 & XGE_HAL_MISC_INT_REG_LINK_DOWN_INT)) {
	            u64 temp64;

	            xge_debug_device(XGE_TRACE,
	            "both link up and link down detected "XGE_OS_LLXFMT,
	            (unsigned long long)val64);

	            temp64 = (XGE_HAL_MISC_INT_REG_LINK_DOWN_INT |
	                  XGE_HAL_MISC_INT_REG_LINK_UP_INT);
	            xge_os_pio_mem_write64(hldev->pdev,
	                           hldev->regh0, temp64,
	                           &isrbar0->misc_int_reg);
	        }
	        else if (val64 & XGE_HAL_MISC_INT_REG_LINK_UP_INT) {
	            xge_debug_device(XGE_TRACE,
	                "link up call request, misc_int "XGE_OS_LLXFMT,
	                (unsigned long long)val64);
	            __hal_device_handle_link_up_ind(hldev);
	        }
	        else if (val64 & XGE_HAL_MISC_INT_REG_LINK_DOWN_INT){
	            xge_debug_device(XGE_TRACE,
	                "link down request, misc_int "XGE_OS_LLXFMT,
	                (unsigned long long)val64);
	            __hal_device_handle_link_down_ind(hldev);
	        }
	    } else
#endif
	    {
	        xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	                   val64, &isrbar0->misc_int_reg);
	    }
	}

	return XGE_HAL_OK;
}

/*
 * __hal_device_handle_txpic - Handle TxPIC interrupt reason
 * @hldev: HAL device handle.
 * @reason: interrupt reason
 */
xge_hal_status_e
__hal_device_handle_txpic(xge_hal_device_t *hldev, u64 reason)
{
	xge_hal_status_e status = XGE_HAL_OK;
	xge_hal_pci_bar0_t *isrbar0 =
	        (xge_hal_pci_bar0_t *)(void *)hldev->isrbar0;
	volatile u64 val64;

	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	            &isrbar0->pic_int_status);
	if ( val64 & (XGE_HAL_PIC_INT_FLSH |
	          XGE_HAL_PIC_INT_MDIO |
	          XGE_HAL_PIC_INT_IIC |
	          XGE_HAL_PIC_INT_MISC) ) {
	    status =  __hal_device_handle_pic(hldev, val64);
	    xge_os_wmb();
	}

	if (!(val64 & XGE_HAL_PIC_INT_TX))
	    return status;

	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	            &isrbar0->txpic_int_reg);
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	               val64, &isrbar0->txpic_int_reg);
	xge_os_wmb();

	if (val64 & XGE_HAL_TXPIC_INT_SCHED_INTR) {
	    int i;

	    if (g_xge_hal_driver->uld_callbacks.sched_timer != NULL)
	        g_xge_hal_driver->uld_callbacks.sched_timer(
	                  hldev, hldev->upper_layer_info);
	    /*
	     * This feature implements adaptive receive interrupt
	     * coalecing. It is disabled by default. To enable it
	     * set hldev->config.rxufca_lo_lim to be not equal to
	     * hldev->config.rxufca_hi_lim.
	     *
	     * We are using HW timer for this feature, so
	     * use needs to configure hldev->config.rxufca_lbolt_period
	     * which is essentially a time slice of timer.
	     *
	     * For those who familiar with Linux, lbolt means jiffies
	     * of this timer. I.e. timer tick.
	     */
	    if (hldev->config.rxufca_lo_lim !=
	            hldev->config.rxufca_hi_lim &&
	        hldev->config.rxufca_lo_lim != 0) {
	        for (i = 0; i < XGE_HAL_MAX_RING_NUM; i++) {
	            if (!hldev->config.ring.queue[i].configured)
	                continue;
	            if (hldev->config.ring.queue[i].rti.urange_a)
	                __hal_update_rxufca(hldev, i);
	        }
	    }

	    /*
	     * This feature implements adaptive TTI timer re-calculation
	     * based on host utilization, number of interrupt processed,
	     * number of RXD per tick and avarage length of packets per
	     * tick.
	     */
	    if (hldev->config.bimodal_interrupts) {
	        for (i = 0; i < XGE_HAL_MAX_RING_NUM; i++) {
	            if (!hldev->config.ring.queue[i].configured)
	                continue;
	            if (hldev->bimodal_tti[i].enabled)
	                __hal_update_bimodal(hldev, i);
	        }
	    }
	}

	return XGE_HAL_OK;
}

/*
 * __hal_device_handle_txdma - Handle TxDMA interrupt reason
 * @hldev: HAL device handle.
 * @reason: interrupt reason
 */
xge_hal_status_e
__hal_device_handle_txdma(xge_hal_device_t *hldev, u64 reason)
{
	xge_hal_pci_bar0_t *isrbar0 =
	        (xge_hal_pci_bar0_t *)(void *)hldev->isrbar0;
	u64 val64, temp64, err;

	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	            &isrbar0->txdma_int_status);
	if (val64 & XGE_HAL_TXDMA_PFC_INT) {
	    err = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	            &isrbar0->pfc_err_reg);
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	            err, &isrbar0->pfc_err_reg);
	    hldev->stats.sw_dev_info_stats.pfc_err_cnt++;
	    temp64 = XGE_HAL_PFC_ECC_DB_ERR|XGE_HAL_PFC_SM_ERR_ALARM
	        |XGE_HAL_PFC_MISC_0_ERR|XGE_HAL_PFC_MISC_1_ERR
	        |XGE_HAL_PFC_PCIX_ERR;
	    if (val64 & temp64)
	        goto reset;
	}
	if (val64 & XGE_HAL_TXDMA_TDA_INT) {
	    err = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	            &isrbar0->tda_err_reg);
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	            err, &isrbar0->tda_err_reg);
	    hldev->stats.sw_dev_info_stats.tda_err_cnt++;
	    temp64 = XGE_HAL_TDA_Fn_ECC_DB_ERR|XGE_HAL_TDA_SM0_ERR_ALARM 
	        |XGE_HAL_TDA_SM1_ERR_ALARM; 
	    if (val64 & temp64)
	        goto reset;
	}
	if (val64 & XGE_HAL_TXDMA_PCC_INT) {
	    err = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	            &isrbar0->pcc_err_reg);
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	            err, &isrbar0->pcc_err_reg);
	    hldev->stats.sw_dev_info_stats.pcc_err_cnt++;
	    temp64 = XGE_HAL_PCC_FB_ECC_DB_ERR|XGE_HAL_PCC_TXB_ECC_DB_ERR
	        |XGE_HAL_PCC_SM_ERR_ALARM|XGE_HAL_PCC_WR_ERR_ALARM
	        |XGE_HAL_PCC_N_SERR|XGE_HAL_PCC_6_COF_OV_ERR
	        |XGE_HAL_PCC_7_COF_OV_ERR|XGE_HAL_PCC_6_LSO_OV_ERR
	        |XGE_HAL_PCC_7_LSO_OV_ERR;
	    if (val64 & temp64)
	        goto reset;
	}
	if (val64 & XGE_HAL_TXDMA_TTI_INT) {
	    err = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	            &isrbar0->tti_err_reg);
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	            err, &isrbar0->tti_err_reg);
	    hldev->stats.sw_dev_info_stats.tti_err_cnt++;
	    temp64 = XGE_HAL_TTI_SM_ERR_ALARM; 
	    if (val64 & temp64)
	        goto reset;
	}
	if (val64 & XGE_HAL_TXDMA_LSO_INT) {
	    err = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	            &isrbar0->lso_err_reg);
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	            err, &isrbar0->lso_err_reg);
	    hldev->stats.sw_dev_info_stats.lso_err_cnt++;
	    temp64 = XGE_HAL_LSO6_ABORT|XGE_HAL_LSO7_ABORT
	        |XGE_HAL_LSO6_SM_ERR_ALARM|XGE_HAL_LSO7_SM_ERR_ALARM; 
	    if (val64 & temp64)
	        goto reset;
	}
	if (val64 & XGE_HAL_TXDMA_TPA_INT) {
	    err = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	            &isrbar0->tpa_err_reg);
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	            err, &isrbar0->tpa_err_reg);
	    hldev->stats.sw_dev_info_stats.tpa_err_cnt++;
	    temp64 = XGE_HAL_TPA_SM_ERR_ALARM; 
	    if (val64 & temp64)
	        goto reset;
	}
	if (val64 & XGE_HAL_TXDMA_SM_INT) {
	    err = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	            &isrbar0->sm_err_reg);
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	            err, &isrbar0->sm_err_reg);
	    hldev->stats.sw_dev_info_stats.sm_err_cnt++;
	    temp64 = XGE_HAL_SM_SM_ERR_ALARM; 
	    if (val64 & temp64)
	        goto reset;
	}

	return XGE_HAL_OK;

reset : xge_hal_device_reset(hldev);
	xge_hal_device_enable(hldev);
	xge_hal_device_intr_enable(hldev);
	return XGE_HAL_OK;
}

/*
 * __hal_device_handle_txmac - Handle TxMAC interrupt reason
 * @hldev: HAL device handle.
 * @reason: interrupt reason
 */
xge_hal_status_e
__hal_device_handle_txmac(xge_hal_device_t *hldev, u64 reason)
{
	xge_hal_pci_bar0_t *isrbar0 =
	        (xge_hal_pci_bar0_t *)(void *)hldev->isrbar0;
	u64 val64, temp64;

	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	            &isrbar0->mac_int_status);
	if (!(val64 & XGE_HAL_MAC_INT_STATUS_TMAC_INT))
	    return XGE_HAL_OK;

	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	            &isrbar0->mac_tmac_err_reg);
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	            val64, &isrbar0->mac_tmac_err_reg);
	hldev->stats.sw_dev_info_stats.mac_tmac_err_cnt++;
	temp64 = XGE_HAL_TMAC_TX_BUF_OVRN|XGE_HAL_TMAC_TX_SM_ERR;
	if (val64 & temp64) {
	    xge_hal_device_reset(hldev);
	    xge_hal_device_enable(hldev);
	    xge_hal_device_intr_enable(hldev);
	}

	return XGE_HAL_OK;
}

/*
 * __hal_device_handle_txxgxs - Handle TxXGXS interrupt reason
 * @hldev: HAL device handle.
 * @reason: interrupt reason
 */
xge_hal_status_e
__hal_device_handle_txxgxs(xge_hal_device_t *hldev, u64 reason)
{
	xge_hal_pci_bar0_t *isrbar0 =
	        (xge_hal_pci_bar0_t *)(void *)hldev->isrbar0;
	u64 val64, temp64;

	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	            &isrbar0->xgxs_int_status);
	if (!(val64 & XGE_HAL_XGXS_INT_STATUS_TXGXS))
	    return XGE_HAL_OK;

	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	            &isrbar0->xgxs_txgxs_err_reg);
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	            val64, &isrbar0->xgxs_txgxs_err_reg);
	hldev->stats.sw_dev_info_stats.xgxs_txgxs_err_cnt++;
	temp64 = XGE_HAL_TXGXS_ESTORE_UFLOW|XGE_HAL_TXGXS_TX_SM_ERR;
	if (val64 & temp64) {
	    xge_hal_device_reset(hldev);
	    xge_hal_device_enable(hldev);
	    xge_hal_device_intr_enable(hldev);
	}

	return XGE_HAL_OK;
}

/*
 * __hal_device_handle_rxpic - Handle RxPIC interrupt reason
 * @hldev: HAL device handle.
 * @reason: interrupt reason
 */
xge_hal_status_e
__hal_device_handle_rxpic(xge_hal_device_t *hldev, u64 reason)
{
	/* FIXME: handle register */

	return XGE_HAL_OK;
}

/*
 * __hal_device_handle_rxdma - Handle RxDMA interrupt reason
 * @hldev: HAL device handle.
 * @reason: interrupt reason
 */
xge_hal_status_e
__hal_device_handle_rxdma(xge_hal_device_t *hldev, u64 reason)
{
	xge_hal_pci_bar0_t *isrbar0 =
	        (xge_hal_pci_bar0_t *)(void *)hldev->isrbar0;
	u64 val64, err, temp64;

	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	            &isrbar0->rxdma_int_status);
	if (val64 & XGE_HAL_RXDMA_RC_INT) {
	    err = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	            &isrbar0->rc_err_reg);
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	            err, &isrbar0->rc_err_reg);
	    hldev->stats.sw_dev_info_stats.rc_err_cnt++;
	    temp64 = XGE_HAL_RC_PRCn_ECC_DB_ERR|XGE_HAL_RC_FTC_ECC_DB_ERR
	        |XGE_HAL_RC_PRCn_SM_ERR_ALARM
	        |XGE_HAL_RC_FTC_SM_ERR_ALARM;
	    if (val64 & temp64)
	        goto reset;
	}
	if (val64 & XGE_HAL_RXDMA_RPA_INT) {
	    err = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	            &isrbar0->rpa_err_reg);
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	            err, &isrbar0->rpa_err_reg);
	    hldev->stats.sw_dev_info_stats.rpa_err_cnt++;
	    temp64 = XGE_HAL_RPA_SM_ERR_ALARM|XGE_HAL_RPA_CREDIT_ERR; 
	    if (val64 & temp64)
	        goto reset;
	}
	if (val64 & XGE_HAL_RXDMA_RDA_INT) {
	    err = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	            &isrbar0->rda_err_reg);
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	            err, &isrbar0->rda_err_reg);
	    hldev->stats.sw_dev_info_stats.rda_err_cnt++;
	    temp64 = XGE_HAL_RDA_RXDn_ECC_DB_ERR
	        |XGE_HAL_RDA_FRM_ECC_DB_N_AERR
	        |XGE_HAL_RDA_SM1_ERR_ALARM|XGE_HAL_RDA_SM0_ERR_ALARM
	        |XGE_HAL_RDA_RXD_ECC_DB_SERR;
	    if (val64 & temp64)
	        goto reset;
	}
	if (val64 & XGE_HAL_RXDMA_RTI_INT) {
	    err = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	            &isrbar0->rti_err_reg);
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	            err, &isrbar0->rti_err_reg);
	    hldev->stats.sw_dev_info_stats.rti_err_cnt++;
	    temp64 = XGE_HAL_RTI_SM_ERR_ALARM; 
	    if (val64 & temp64)
	        goto reset;
	}

	return XGE_HAL_OK;

reset : xge_hal_device_reset(hldev);
	xge_hal_device_enable(hldev);
	xge_hal_device_intr_enable(hldev);
	return XGE_HAL_OK;
}

/*
 * __hal_device_handle_rxmac - Handle RxMAC interrupt reason
 * @hldev: HAL device handle.
 * @reason: interrupt reason
 */
xge_hal_status_e
__hal_device_handle_rxmac(xge_hal_device_t *hldev, u64 reason)
{
	xge_hal_pci_bar0_t *isrbar0 =
	        (xge_hal_pci_bar0_t *)(void *)hldev->isrbar0;
	u64 val64, temp64;

	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	            &isrbar0->mac_int_status);
	if (!(val64 & XGE_HAL_MAC_INT_STATUS_RMAC_INT))
	    return XGE_HAL_OK;

	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	            &isrbar0->mac_rmac_err_reg);
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	            val64, &isrbar0->mac_rmac_err_reg);
	hldev->stats.sw_dev_info_stats.mac_rmac_err_cnt++;
	temp64 = XGE_HAL_RMAC_RX_BUFF_OVRN|XGE_HAL_RMAC_RX_SM_ERR;
	if (val64 & temp64) {
	    xge_hal_device_reset(hldev);
	    xge_hal_device_enable(hldev);
	    xge_hal_device_intr_enable(hldev);
	}

	return XGE_HAL_OK;
}

/*
 * __hal_device_handle_rxxgxs - Handle RxXGXS interrupt reason
 * @hldev: HAL device handle.
 * @reason: interrupt reason
 */
xge_hal_status_e
__hal_device_handle_rxxgxs(xge_hal_device_t *hldev, u64 reason)
{
	xge_hal_pci_bar0_t *isrbar0 =
	        (xge_hal_pci_bar0_t *)(void *)hldev->isrbar0;
	u64 val64, temp64;

	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	            &isrbar0->xgxs_int_status);
	if (!(val64 & XGE_HAL_XGXS_INT_STATUS_RXGXS))
	    return XGE_HAL_OK;

	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	            &isrbar0->xgxs_rxgxs_err_reg);
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	            val64, &isrbar0->xgxs_rxgxs_err_reg);
	hldev->stats.sw_dev_info_stats.xgxs_rxgxs_err_cnt++;
	temp64 = XGE_HAL_RXGXS_ESTORE_OFLOW|XGE_HAL_RXGXS_RX_SM_ERR;
	if (val64 & temp64) {
	    xge_hal_device_reset(hldev);
	    xge_hal_device_enable(hldev);
	    xge_hal_device_intr_enable(hldev);
	}

	return XGE_HAL_OK;
}

/**
 * xge_hal_device_enable - Enable device.
 * @hldev: HAL device handle.
 *
 * Enable the specified device: bring up the link/interface.
 * Returns:  XGE_HAL_OK - success.
 * XGE_HAL_ERR_DEVICE_IS_NOT_QUIESCENT - Failed to restore the device
 * to a "quiescent" state.
 *
 * See also: xge_hal_status_e{}.
 *
 * Usage: See ex_open{}.
 */
xge_hal_status_e
xge_hal_device_enable(xge_hal_device_t *hldev)
{
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;
	u64 val64;
	u64 adp_status;
	int i, j;

	if (!hldev->hw_is_initialized) {
	    xge_hal_status_e status;

	    status = __hal_device_hw_initialize(hldev);
	    if (status != XGE_HAL_OK) {
	        return status;
	    }
	}

	/*
	 * Not needed in most cases, i.e.
	 * when device_disable() is followed by reset -
	 * the latter copies back PCI config space, along with
	 * the bus mastership - see __hal_device_reset().
	 * However, there are/may-in-future be other cases, and
	 * does not hurt.
	 */
	__hal_device_bus_master_enable(hldev);

	if (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_HERC) {
	    /*
	     * Configure the link stability period.
	     */
	    val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                      &bar0->misc_control);
	    if (hldev->config.link_stability_period !=
	            XGE_HAL_DEFAULT_USE_HARDCODE) {

	        val64 |= XGE_HAL_MISC_CONTROL_LINK_STABILITY_PERIOD(
	                hldev->config.link_stability_period);
	    } else {
	        /*
	         * Use the link stability period 1 ms as default
	         */
	        val64 |= XGE_HAL_MISC_CONTROL_LINK_STABILITY_PERIOD(
	                XGE_HAL_DEFAULT_LINK_STABILITY_PERIOD);
	    }
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	                   val64, &bar0->misc_control);

	    /*
	     * Clearing any possible Link up/down interrupts that
	     * could have popped up just before Enabling the card.
	     */
	    val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                      &bar0->misc_int_reg);
	    if (val64) {
	        xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	                       val64, &bar0->misc_int_reg);
	        xge_debug_device(XGE_TRACE, "%s","link state cleared");
	    }
	} else if (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_XENA) {
	    /*
	     * Clearing any possible Link state change interrupts that
	     * could have popped up just before Enabling the card.
	     */
	    val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	        &bar0->mac_rmac_err_reg);
	    if (val64) {
	        xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	                       val64, &bar0->mac_rmac_err_reg);
	        xge_debug_device(XGE_TRACE, "%s", "link state cleared");
	    }
	}

	if (__hal_device_wait_quiescent(hldev, &val64)) {
	    return XGE_HAL_ERR_DEVICE_IS_NOT_QUIESCENT;
	}

	/* Enabling Laser. */
	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                &bar0->adapter_control);
	val64 |= XGE_HAL_ADAPTER_EOI_TX_ON;
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	                     &bar0->adapter_control);

	/* let link establish */
	xge_os_mdelay(1);

	/* set link down untill poll() routine will set it up (maybe) */
	hldev->link_state = XGE_HAL_LINK_DOWN;

	/* If link is UP (adpter is connected) then enable the adapter */
	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                            &bar0->adapter_status);
	if( val64 & (XGE_HAL_ADAPTER_STATUS_RMAC_REMOTE_FAULT |
	         XGE_HAL_ADAPTER_STATUS_RMAC_LOCAL_FAULT) ) {
	    val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                            &bar0->adapter_control);
	    val64 = val64 & (~XGE_HAL_ADAPTER_LED_ON);
	} else {
	    val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                               &bar0->adapter_control);
	    val64 = val64 | ( XGE_HAL_ADAPTER_EOI_TX_ON |
	              XGE_HAL_ADAPTER_LED_ON );
	}

	val64 = val64 | XGE_HAL_ADAPTER_CNTL_EN;   /* adapter enable */
	val64 = val64 & (~XGE_HAL_ADAPTER_ECC_EN); /* ECC enable */
	xge_os_pio_mem_write64 (hldev->pdev, hldev->regh0, val64,
	              &bar0->adapter_control);

	/* We spin here waiting for the Link to come up.
	 * This is the fix for the Link being unstable after the reset. */
	i = 0;
	j = 0;
	do
	{
	    adp_status = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                                    &bar0->adapter_status);

	    /* Read the adapter control register for Adapter_enable bit */
	    val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                               &bar0->adapter_control);
	    if (!(adp_status & (XGE_HAL_ADAPTER_STATUS_RMAC_REMOTE_FAULT |
	                XGE_HAL_ADAPTER_STATUS_RMAC_LOCAL_FAULT)) &&
	        (val64 & XGE_HAL_ADAPTER_CNTL_EN)) {
	        j++;
	        if (j >= hldev->config.link_valid_cnt) {
	            if (xge_hal_device_status(hldev, &adp_status) ==
	                        XGE_HAL_OK) {
	                if (__hal_verify_pcc_idle(hldev,
	                      adp_status) != XGE_HAL_OK) {
	                   return
	                    XGE_HAL_ERR_DEVICE_IS_NOT_QUIESCENT;
	                }
	                xge_debug_device(XGE_TRACE,
	                      "adp_status: "XGE_OS_LLXFMT
	                      ", link is up on "
	                      "adapter enable!",
	                      (unsigned long long)adp_status);
	                val64 = xge_os_pio_mem_read64(
	                        hldev->pdev,
	                        hldev->regh0,
	                        &bar0->adapter_control);
	                val64 = val64|
	                    (XGE_HAL_ADAPTER_EOI_TX_ON |
	                     XGE_HAL_ADAPTER_LED_ON );
	                xge_os_pio_mem_write64(hldev->pdev,
	                                hldev->regh0, val64,
	                                &bar0->adapter_control);
	                xge_os_mdelay(1);

	                val64 = xge_os_pio_mem_read64(
	                        hldev->pdev,
	                        hldev->regh0,
	                        &bar0->adapter_control);
	                break;    /* out of for loop */
	            } else {
	                   return
	                   XGE_HAL_ERR_DEVICE_IS_NOT_QUIESCENT;
	            }
	        }
	    } else {
	        j = 0;  /* Reset the count */
	        /* Turn on the Laser */
	        val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                        &bar0->adapter_control);
	        val64 = val64 | XGE_HAL_ADAPTER_EOI_TX_ON;
	        xge_os_pio_mem_write64 (hldev->pdev, hldev->regh0,
	                    val64, &bar0->adapter_control);

	        xge_os_mdelay(1);

	        /* Now re-enable it as due to noise, hardware
	         * turned it off */
	        val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                                    &bar0->adapter_control);
	        val64 |= XGE_HAL_ADAPTER_CNTL_EN;
	        val64 = val64 & (~XGE_HAL_ADAPTER_ECC_EN);/*ECC enable*/
	        xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	                            &bar0->adapter_control);
	    }
	    xge_os_mdelay(1); /* Sleep for 1 msec */
	    i++;
	} while (i < hldev->config.link_retry_cnt);

	__hal_device_led_actifity_fix(hldev);

#ifndef  XGE_HAL_PROCESS_LINK_INT_IN_ISR
	/* Here we are performing soft reset on XGXS to force link down.
	 * Since link is already up, we will get link state change
	 * poll notificatoin after adapter is enabled */

	__hal_serial_mem_write64(hldev, 0x80010515001E0000ULL,
	             &bar0->dtx_control);
	(void) __hal_serial_mem_read64(hldev, &bar0->dtx_control);

	__hal_serial_mem_write64(hldev, 0x80010515001E00E0ULL,
	             &bar0->dtx_control);
	(void) __hal_serial_mem_read64(hldev, &bar0->dtx_control);

	__hal_serial_mem_write64(hldev, 0x80070515001F00E4ULL,
	             &bar0->dtx_control);
	(void) __hal_serial_mem_read64(hldev, &bar0->dtx_control);

	xge_os_mdelay(100); /* Sleep for 500 msec */
#else
	if (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_XENA)
#endif
	{
	    /*
	     * With some switches the link state change interrupt does not
	     * occur even though the xgxs reset is done as per SPN-006. So,
	     * poll the adapter status register and check if the link state
	     * is ok.
	     */
	    adp_status = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                       &bar0->adapter_status);
	    if (!(adp_status & (XGE_HAL_ADAPTER_STATUS_RMAC_REMOTE_FAULT |
	          XGE_HAL_ADAPTER_STATUS_RMAC_LOCAL_FAULT)))
	    {
	        xge_debug_device(XGE_TRACE, "%s",
	             "enable device causing link state change ind..");
	        (void) __hal_device_handle_link_state_change(hldev);
	    }
	}

	if (hldev->config.stats_refresh_time_sec !=
	    XGE_HAL_STATS_REFRESH_DISABLE)
	        __hal_stats_enable(&hldev->stats);

	return XGE_HAL_OK;
}

/**
 * xge_hal_device_disable - Disable Xframe adapter.
 * @hldev: Device handle.
 *
 * Disable this device. To gracefully reset the adapter, the host should:
 *
 *  - call xge_hal_device_disable();
 *
 *  - call xge_hal_device_intr_disable();
 *
 *  - close all opened channels and clean up outstanding resources;
 *
 *  - do some work (error recovery, change mtu, reset, etc);
 *
 *  - call xge_hal_device_enable();
 *
 *  - open channels, replenish RxDs, etc.
 *
 *  - call xge_hal_device_intr_enable().
 *
 * Note: Disabling the device does _not_ include disabling of interrupts.
 * After disabling the device stops receiving new frames but those frames
 * that were already in the pipe will keep coming for some few milliseconds.
 *
 * Returns:  XGE_HAL_OK - success.
 * XGE_HAL_ERR_DEVICE_IS_NOT_QUIESCENT - Failed to restore the device to
 * a "quiescent" state.
 *
 * See also: xge_hal_status_e{}.
 */
xge_hal_status_e
xge_hal_device_disable(xge_hal_device_t *hldev)
{
	xge_hal_status_e status = XGE_HAL_OK;
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;
	u64 val64;

	xge_debug_device(XGE_TRACE, "%s", "turn off laser, cleanup hardware");

	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                            &bar0->adapter_control);
	val64 = val64 & (~XGE_HAL_ADAPTER_CNTL_EN);
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	                     &bar0->adapter_control);

	if (__hal_device_wait_quiescent(hldev, &val64) != XGE_HAL_OK) {
	    status = XGE_HAL_ERR_DEVICE_IS_NOT_QUIESCENT;
	}

	if (__hal_device_register_poll(hldev, &bar0->adapter_status, 1,
	     XGE_HAL_ADAPTER_STATUS_RC_PRC_QUIESCENT,
	     XGE_HAL_DEVICE_QUIESCENT_WAIT_MAX_MILLIS) != XGE_HAL_OK) {
	    xge_debug_device(XGE_TRACE, "%s", "PRC is not QUIESCENT!");
	    status = XGE_HAL_ERR_DEVICE_IS_NOT_QUIESCENT;
	}

	if (hldev->config.stats_refresh_time_sec !=
	    XGE_HAL_STATS_REFRESH_DISABLE)
	            __hal_stats_disable(&hldev->stats);
#ifdef XGE_DEBUG_ASSERT
	    else
	        xge_assert(!hldev->stats.is_enabled);
#endif

#ifndef XGE_HAL_DONT_DISABLE_BUS_MASTER_ON_STOP
	__hal_device_bus_master_disable(hldev);
#endif

	return status;
}

/**
 * xge_hal_device_reset - Reset device.
 * @hldev: HAL device handle.
 *
 * Soft-reset the device, reset the device stats except reset_cnt.
 *
 * After reset is done, will try to re-initialize HW.
 *
 * Returns:  XGE_HAL_OK - success.
 * XGE_HAL_ERR_DEVICE_NOT_INITIALIZED - Device is not initialized.
 * XGE_HAL_ERR_RESET_FAILED - Reset failed.
 *
 * See also: xge_hal_status_e{}.
 */
xge_hal_status_e
xge_hal_device_reset(xge_hal_device_t *hldev)
{
	xge_hal_status_e status;

	/* increment the soft reset counter */
	u32 reset_cnt = hldev->stats.sw_dev_info_stats.soft_reset_cnt;

	xge_debug_device(XGE_TRACE, "%s (%d)", "resetting the device", reset_cnt);

	if (!hldev->is_initialized)
	    return XGE_HAL_ERR_DEVICE_NOT_INITIALIZED;

	/* actual "soft" reset of the adapter */
	status = __hal_device_reset(hldev);

	/* reset all stats including saved */
	__hal_stats_soft_reset(hldev, 1);

	/* increment reset counter */
	hldev->stats.sw_dev_info_stats.soft_reset_cnt = reset_cnt + 1;

	/* re-initialize rxufca_intr_thres */
	hldev->rxufca_intr_thres = hldev->config.rxufca_intr_thres;

	    hldev->reset_needed_after_close = 0;

	return status;
}

/**
 * xge_hal_device_status - Check whether Xframe hardware is ready for
 * operation.
 * @hldev: HAL device handle.
 * @hw_status: Xframe status register. Returned by HAL.
 *
 * Check whether Xframe hardware is ready for operation.
 * The checking includes TDMA, RDMA, PFC, PIC, MC_DRAM, and the rest
 * hardware functional blocks.
 *
 * Returns: XGE_HAL_OK if the device is ready for operation. Otherwise
 * returns XGE_HAL_FAIL. Also, fills in  adapter status (in @hw_status).
 *
 * See also: xge_hal_status_e{}.
 * Usage: See ex_open{}.
 */
xge_hal_status_e
xge_hal_device_status(xge_hal_device_t *hldev, u64 *hw_status)
{
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;
	u64 tmp64;

	tmp64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                            &bar0->adapter_status);

	*hw_status = tmp64;

	if (!(tmp64 & XGE_HAL_ADAPTER_STATUS_TDMA_READY)) {
	    xge_debug_device(XGE_TRACE, "%s", "TDMA is not ready!");
	    return XGE_HAL_FAIL;
	}
	if (!(tmp64 & XGE_HAL_ADAPTER_STATUS_RDMA_READY)) {
	    xge_debug_device(XGE_TRACE, "%s", "RDMA is not ready!");
	    return XGE_HAL_FAIL;
	}
	if (!(tmp64 & XGE_HAL_ADAPTER_STATUS_PFC_READY)) {
	    xge_debug_device(XGE_TRACE, "%s", "PFC is not ready!");
	    return XGE_HAL_FAIL;
	}
	if (!(tmp64 & XGE_HAL_ADAPTER_STATUS_TMAC_BUF_EMPTY)) {
	    xge_debug_device(XGE_TRACE, "%s", "TMAC BUF is not empty!");
	    return XGE_HAL_FAIL;
	}
	if (!(tmp64 & XGE_HAL_ADAPTER_STATUS_PIC_QUIESCENT)) {
	    xge_debug_device(XGE_TRACE, "%s", "PIC is not QUIESCENT!");
	    return XGE_HAL_FAIL;
	}
	if (!(tmp64 & XGE_HAL_ADAPTER_STATUS_MC_DRAM_READY)) {
	    xge_debug_device(XGE_TRACE, "%s", "MC_DRAM is not ready!");
	    return XGE_HAL_FAIL;
	}
	if (!(tmp64 & XGE_HAL_ADAPTER_STATUS_MC_QUEUES_READY)) {
	    xge_debug_device(XGE_TRACE, "%s", "MC_QUEUES is not ready!");
	    return XGE_HAL_FAIL;
	}
	if (!(tmp64 & XGE_HAL_ADAPTER_STATUS_M_PLL_LOCK)) {
	    xge_debug_device(XGE_TRACE, "%s", "M_PLL is not locked!");
	    return XGE_HAL_FAIL;
	}
#ifndef XGE_HAL_HERC_EMULATION
	/*
	 * Andrew: in PCI 33 mode, the P_PLL is not used, and therefore,
	 * the the P_PLL_LOCK bit in the adapter_status register will
	 * not be asserted.
	 */
	if (!(tmp64 & XGE_HAL_ADAPTER_STATUS_P_PLL_LOCK) &&
	     xge_hal_device_check_id(hldev) == XGE_HAL_CARD_HERC &&
	     hldev->pci_mode != XGE_HAL_PCI_33MHZ_MODE) {
	    xge_debug_device(XGE_TRACE, "%s", "P_PLL is not locked!");
	    return XGE_HAL_FAIL;
	}
#endif

	return XGE_HAL_OK;
}

void
__hal_device_msi_intr_endis(xge_hal_device_t *hldev, int flag)
{
	u16 msi_control_reg;

	xge_os_pci_read16(hldev->pdev, hldev->cfgh,
	     xge_offsetof(xge_hal_pci_config_le_t,
	          msi_control), &msi_control_reg);

	if (flag)
	    msi_control_reg |= 0x1;
	else
	    msi_control_reg &= ~0x1;

	xge_os_pci_write16(hldev->pdev, hldev->cfgh,
	     xge_offsetof(xge_hal_pci_config_le_t,
	             msi_control), msi_control_reg);
}

void
__hal_device_msix_intr_endis(xge_hal_device_t *hldev,
	              xge_hal_channel_t *channel, int flag)
{
	u64 val64;
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)hldev->bar0;

	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	    &bar0->xmsi_mask_reg);

	if (flag)
	    val64 &= ~(1LL << ( 63 - channel->msix_idx ));
	else
	    val64 |= (1LL << ( 63 - channel->msix_idx ));
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	    &bar0->xmsi_mask_reg);
}

/**
 * xge_hal_device_intr_enable - Enable Xframe interrupts.
 * @hldev: HAL device handle.
 * @op: One of the xge_hal_device_intr_e enumerated values specifying
 *      the type(s) of interrupts to enable.
 *
 * Enable Xframe interrupts. The function is to be executed the last in
 * Xframe initialization sequence.
 *
 * See also: xge_hal_device_intr_disable()
 */
void
xge_hal_device_intr_enable(xge_hal_device_t *hldev)
{
	xge_list_t *item;
	u64 val64;

	/* PRC initialization and configuration */
	xge_list_for_each(item, &hldev->ring_channels) {
	    xge_hal_channel_h channel;
	    channel = xge_container_of(item, xge_hal_channel_t, item);
	    __hal_ring_prc_enable(channel);
	}

	/* enable traffic only interrupts */
	if (hldev->config.intr_mode != XGE_HAL_INTR_MODE_IRQLINE) {
	    /*
	     * make sure all interrupts going to be disabled if MSI
	     * is enabled.
	     */
	    __hal_device_intr_mgmt(hldev, XGE_HAL_ALL_INTRS, 0);
	} else {
	    /*
	     * Enable the Tx traffic interrupts only if the TTI feature is
	     * enabled.
	     */
	    val64 = 0;
	    if (hldev->tti_enabled)
	        val64 = XGE_HAL_TX_TRAFFIC_INTR;

	    if (!hldev->config.bimodal_interrupts)
	        val64 |= XGE_HAL_RX_TRAFFIC_INTR;

	    if (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_XENA)
	        val64 |= XGE_HAL_RX_TRAFFIC_INTR;

	    val64 |=XGE_HAL_TX_PIC_INTR |
	        XGE_HAL_MC_INTR |
	        XGE_HAL_TX_DMA_INTR |
	        (hldev->config.sched_timer_us !=
	         XGE_HAL_SCHED_TIMER_DISABLED ? XGE_HAL_SCHED_INTR : 0);
	    __hal_device_intr_mgmt(hldev, val64, 1);
	}

	/*
	 * Enable MSI-X interrupts
	 */
	if (hldev->config.intr_mode == XGE_HAL_INTR_MODE_MSIX) {

	    if (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_HERC) {
	        /*
	         * To enable MSI-X, MSI also needs to be enabled,
	         * due to a bug in the herc NIC.
	         */
	        __hal_device_msi_intr_endis(hldev, 1);
	    }


	    /* Enable the MSI-X interrupt for each configured channel */
	    xge_list_for_each(item, &hldev->fifo_channels) {
	        xge_hal_channel_t *channel;

	        channel = xge_container_of(item,
	                   xge_hal_channel_t, item);

	        /* 0 vector is reserved for alarms */
	        if (!channel->msix_idx)
	            continue;

	        __hal_device_msix_intr_endis(hldev, channel, 1);
	    }

	    xge_list_for_each(item, &hldev->ring_channels) {
	        xge_hal_channel_t *channel;

	        channel = xge_container_of(item,
	                   xge_hal_channel_t, item);

	        /* 0 vector is reserved for alarms */
	        if (!channel->msix_idx)
	            continue;

	        __hal_device_msix_intr_endis(hldev, channel, 1);
	    }
	}

	xge_debug_device(XGE_TRACE, "%s", "interrupts are enabled");
}


/**
 * xge_hal_device_intr_disable - Disable Xframe interrupts.
 * @hldev: HAL device handle.
 * @op: One of the xge_hal_device_intr_e enumerated values specifying
 *      the type(s) of interrupts to disable.
 *
 * Disable Xframe interrupts.
 *
 * See also: xge_hal_device_intr_enable()
 */
void
xge_hal_device_intr_disable(xge_hal_device_t *hldev)
{
	xge_list_t *item;
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;
	u64 val64;

	if (hldev->config.intr_mode == XGE_HAL_INTR_MODE_MSIX) {

	    if (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_HERC) {
	        /*
	         * To disable MSI-X, MSI also needs to be disabled,
	         * due to a bug in the herc NIC.
	         */
	        __hal_device_msi_intr_endis(hldev, 0);
	    }

	    /* Disable the MSI-X interrupt for each configured channel */
	    xge_list_for_each(item, &hldev->fifo_channels) {
	        xge_hal_channel_t *channel;

	        channel = xge_container_of(item,
	                   xge_hal_channel_t, item);

	        /* 0 vector is reserved for alarms */
	        if (!channel->msix_idx)
	            continue;

	        __hal_device_msix_intr_endis(hldev, channel, 0);

	    }

	    xge_os_pio_mem_write64(hldev->pdev,
	        hldev->regh0, 0xFFFFFFFFFFFFFFFFULL,
	        &bar0->tx_traffic_mask);

	    xge_list_for_each(item, &hldev->ring_channels) {
	        xge_hal_channel_t *channel;

	        channel = xge_container_of(item,
	                   xge_hal_channel_t, item);

	        /* 0 vector is reserved for alarms */
	        if (!channel->msix_idx)
	            continue;

	        __hal_device_msix_intr_endis(hldev, channel, 0);
	    }

	    xge_os_pio_mem_write64(hldev->pdev,
	        hldev->regh0, 0xFFFFFFFFFFFFFFFFULL,
	        &bar0->rx_traffic_mask);
	}

	/*
	 * Disable traffic only interrupts.
	 * Tx traffic interrupts are used only if the TTI feature is
	 * enabled.
	 */
	val64 = 0;
	if (hldev->tti_enabled)
	    val64 = XGE_HAL_TX_TRAFFIC_INTR;

	val64 |= XGE_HAL_RX_TRAFFIC_INTR |
	     XGE_HAL_TX_PIC_INTR |
	     XGE_HAL_MC_INTR |
	     (hldev->config.sched_timer_us != XGE_HAL_SCHED_TIMER_DISABLED ?
	                    XGE_HAL_SCHED_INTR : 0);
	__hal_device_intr_mgmt(hldev, val64, 0);

	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	                     0xFFFFFFFFFFFFFFFFULL,
	             &bar0->general_int_mask);


	/* disable all configured PRCs */
	xge_list_for_each(item, &hldev->ring_channels) {
	    xge_hal_channel_h channel;
	    channel = xge_container_of(item, xge_hal_channel_t, item);
	    __hal_ring_prc_disable(channel);
	}

	xge_debug_device(XGE_TRACE, "%s", "interrupts are disabled");
}


/**
 * xge_hal_device_mcast_enable - Enable Xframe multicast addresses.
 * @hldev: HAL device handle.
 *
 * Enable Xframe multicast addresses.
 * Returns: XGE_HAL_OK on success.
 * XGE_HAL_INF_MEM_STROBE_CMD_EXECUTING - Failed to enable mcast
 * feature within the time(timeout).
 *
 * See also: xge_hal_device_mcast_disable(), xge_hal_status_e{}.
 */
xge_hal_status_e
xge_hal_device_mcast_enable(xge_hal_device_t *hldev)
{
	u64 val64;
	xge_hal_pci_bar0_t *bar0;
	int mc_offset = XGE_HAL_MAC_MC_ALL_MC_ADDR_OFFSET;

	if (hldev == NULL)
	    return XGE_HAL_ERR_INVALID_DEVICE;

	if (hldev->mcast_refcnt)
	    return XGE_HAL_OK;

	if (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_HERC)
	    mc_offset = XGE_HAL_MAC_MC_ALL_MC_ADDR_OFFSET_HERC;

	hldev->mcast_refcnt = 1;

	bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;

	/*  Enable all Multicast addresses */
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	      XGE_HAL_RMAC_ADDR_DATA0_MEM_ADDR(0x010203040506ULL),
	      &bar0->rmac_addr_data0_mem);
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	      XGE_HAL_RMAC_ADDR_DATA1_MEM_MASK(0xfeffffffffffULL),
	      &bar0->rmac_addr_data1_mem);
	val64 = XGE_HAL_RMAC_ADDR_CMD_MEM_WE |
	    XGE_HAL_RMAC_ADDR_CMD_MEM_STROBE_NEW_CMD |
	    XGE_HAL_RMAC_ADDR_CMD_MEM_OFFSET(mc_offset);
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	                &bar0->rmac_addr_cmd_mem);

	if (__hal_device_register_poll(hldev,
	    &bar0->rmac_addr_cmd_mem, 0,
	    XGE_HAL_RMAC_ADDR_CMD_MEM_STROBE_CMD_EXECUTING,
	    XGE_HAL_DEVICE_CMDMEM_WAIT_MAX_MILLIS) != XGE_HAL_OK) {
	    /* upper layer may require to repeat */
	    return XGE_HAL_INF_MEM_STROBE_CMD_EXECUTING;
	}

	return XGE_HAL_OK;
}

/**
 * xge_hal_device_mcast_disable - Disable Xframe multicast addresses.
 * @hldev: HAL device handle.
 *
 * Disable Xframe multicast addresses.
 * Returns: XGE_HAL_OK - success.
 * XGE_HAL_INF_MEM_STROBE_CMD_EXECUTING - Failed to disable mcast
 * feature within the time(timeout).
 *
 * See also: xge_hal_device_mcast_enable(), xge_hal_status_e{}.
 */
xge_hal_status_e
xge_hal_device_mcast_disable(xge_hal_device_t *hldev)
{
	u64 val64;
	xge_hal_pci_bar0_t *bar0;
	int mc_offset = XGE_HAL_MAC_MC_ALL_MC_ADDR_OFFSET;

	if (hldev == NULL)
	    return XGE_HAL_ERR_INVALID_DEVICE;

	if (hldev->mcast_refcnt == 0)
	    return XGE_HAL_OK;

	if (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_HERC)
	    mc_offset = XGE_HAL_MAC_MC_ALL_MC_ADDR_OFFSET_HERC;

	hldev->mcast_refcnt = 0;

	bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;

	/*  Disable all Multicast addresses */
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	       XGE_HAL_RMAC_ADDR_DATA0_MEM_ADDR(0xffffffffffffULL),
	           &bar0->rmac_addr_data0_mem);
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	       XGE_HAL_RMAC_ADDR_DATA1_MEM_MASK(0),
	           &bar0->rmac_addr_data1_mem);

	val64 = XGE_HAL_RMAC_ADDR_CMD_MEM_WE |
	    XGE_HAL_RMAC_ADDR_CMD_MEM_STROBE_NEW_CMD |
	    XGE_HAL_RMAC_ADDR_CMD_MEM_OFFSET(mc_offset);
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	                &bar0->rmac_addr_cmd_mem);

	if (__hal_device_register_poll(hldev,
	    &bar0->rmac_addr_cmd_mem, 0,
	    XGE_HAL_RMAC_ADDR_CMD_MEM_STROBE_CMD_EXECUTING,
	    XGE_HAL_DEVICE_CMDMEM_WAIT_MAX_MILLIS) != XGE_HAL_OK) {
	    /* upper layer may require to repeat */
	    return XGE_HAL_INF_MEM_STROBE_CMD_EXECUTING;
	}

	return XGE_HAL_OK;
}

/**
 * xge_hal_device_promisc_enable - Enable promiscuous mode.
 * @hldev: HAL device handle.
 *
 * Enable promiscuous mode of Xframe operation.
 *
 * See also: xge_hal_device_promisc_disable().
 */
void
xge_hal_device_promisc_enable(xge_hal_device_t *hldev)
{
	u64 val64;
	xge_hal_pci_bar0_t *bar0;

	xge_assert(hldev);

	bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;

	if (!hldev->is_promisc) {
	    /*  Put the NIC into promiscuous mode */
	    val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                                &bar0->mac_cfg);
	    val64 |= XGE_HAL_MAC_CFG_RMAC_PROM_ENABLE;

	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	               XGE_HAL_RMAC_CFG_KEY(0x4C0D),
	               &bar0->rmac_cfg_key);

	    __hal_pio_mem_write32_upper(hldev->pdev, hldev->regh0,
	                  (u32)(val64 >> 32),
	                  &bar0->mac_cfg);

	    hldev->is_promisc = 1;
	    xge_debug_device(XGE_TRACE,
	        "mac_cfg 0x"XGE_OS_LLXFMT": promisc enabled",
	        (unsigned long long)val64);
	}
}

/**
 * xge_hal_device_promisc_disable - Disable promiscuous mode.
 * @hldev: HAL device handle.
 *
 * Disable promiscuous mode of Xframe operation.
 *
 * See also: xge_hal_device_promisc_enable().
 */
void
xge_hal_device_promisc_disable(xge_hal_device_t *hldev)
{
	u64 val64;
	xge_hal_pci_bar0_t *bar0;

	xge_assert(hldev);

	bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;

	if (hldev->is_promisc) {
	    /*  Remove the NIC from promiscuous mode */
	    val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                    &bar0->mac_cfg);
	    val64 &= ~XGE_HAL_MAC_CFG_RMAC_PROM_ENABLE;

	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	               XGE_HAL_RMAC_CFG_KEY(0x4C0D),
	               &bar0->rmac_cfg_key);

	    __hal_pio_mem_write32_upper(hldev->pdev, hldev->regh0,
	                  (u32)(val64 >> 32),
	                  &bar0->mac_cfg);

	    hldev->is_promisc = 0;
	    xge_debug_device(XGE_TRACE,
	        "mac_cfg 0x"XGE_OS_LLXFMT": promisc disabled",
	        (unsigned long long)val64);
	}
}

/**
 * xge_hal_device_macaddr_get - Get MAC addresses.
 * @hldev: HAL device handle.
 * @index: MAC address index, in the range from 0 to
 * XGE_HAL_MAX_MAC_ADDRESSES.
 * @macaddr: MAC address. Returned by HAL.
 *
 * Retrieve one of the stored MAC addresses by reading non-volatile
 * memory on the chip.
 *
 * Up to %XGE_HAL_MAX_MAC_ADDRESSES addresses is supported.
 *
 * Returns: XGE_HAL_OK - success.
 * XGE_HAL_INF_MEM_STROBE_CMD_EXECUTING - Failed to retrieve the mac
 * address within the time(timeout).
 * XGE_HAL_ERR_OUT_OF_MAC_ADDRESSES - Invalid MAC address index.
 *
 * See also: xge_hal_device_macaddr_set(), xge_hal_status_e{}.
 */
xge_hal_status_e
xge_hal_device_macaddr_get(xge_hal_device_t *hldev, int index,
	        macaddr_t *macaddr)
{
	xge_hal_pci_bar0_t *bar0;
	u64 val64;
	int i;

	if (hldev == NULL) {
	    return XGE_HAL_ERR_INVALID_DEVICE;
	}

	bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;

	if ( index >= XGE_HAL_MAX_MAC_ADDRESSES ) {
	    return XGE_HAL_ERR_OUT_OF_MAC_ADDRESSES;
	}

#ifdef XGE_HAL_HERC_EMULATION
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,0x0000010000000000,
	                            &bar0->rmac_addr_data0_mem);
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,0x0000000000000000,
	                            &bar0->rmac_addr_data1_mem);
	val64 = XGE_HAL_RMAC_ADDR_CMD_MEM_RD |
	             XGE_HAL_RMAC_ADDR_CMD_MEM_STROBE_NEW_CMD |
	             XGE_HAL_RMAC_ADDR_CMD_MEM_OFFSET((index));
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	                     &bar0->rmac_addr_cmd_mem);

	    /* poll until done */
	__hal_device_register_poll(hldev,
	           &bar0->rmac_addr_cmd_mem, 0,
	           XGE_HAL_RMAC_ADDR_CMD_MEM_STROBE_NEW_CMD,
	           XGE_HAL_DEVICE_CMDMEM_WAIT_MAX_MILLIS);

#endif

	val64 = ( XGE_HAL_RMAC_ADDR_CMD_MEM_RD |
	      XGE_HAL_RMAC_ADDR_CMD_MEM_STROBE_NEW_CMD |
	      XGE_HAL_RMAC_ADDR_CMD_MEM_OFFSET((index)) );
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	                     &bar0->rmac_addr_cmd_mem);

	if (__hal_device_register_poll(hldev, &bar0->rmac_addr_cmd_mem, 0,
	       XGE_HAL_RMAC_ADDR_CMD_MEM_STROBE_CMD_EXECUTING,
	       XGE_HAL_DEVICE_CMDMEM_WAIT_MAX_MILLIS) != XGE_HAL_OK) {
	    /* upper layer may require to repeat */
	    return XGE_HAL_INF_MEM_STROBE_CMD_EXECUTING;
	}

	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                            &bar0->rmac_addr_data0_mem);
	for (i=0; i < XGE_HAL_ETH_ALEN; i++) {
	    (*macaddr)[i] = (u8)(val64 >> ((64 - 8) - (i * 8)));
	}

#ifdef XGE_HAL_HERC_EMULATION
	for (i=0; i < XGE_HAL_ETH_ALEN; i++) {
	    (*macaddr)[i] = (u8)0;
	}
	(*macaddr)[1] = (u8)1;

#endif

	return XGE_HAL_OK;
}

/**
 * xge_hal_device_macaddr_set - Set MAC address.
 * @hldev: HAL device handle.
 * @index: MAC address index, in the range from 0 to
 * XGE_HAL_MAX_MAC_ADDRESSES.
 * @macaddr: New MAC address to configure.
 *
 * Configure one of the available MAC address "slots".
 *
 * Up to %XGE_HAL_MAX_MAC_ADDRESSES addresses is supported.
 *
 * Returns: XGE_HAL_OK - success.
 * XGE_HAL_INF_MEM_STROBE_CMD_EXECUTING - Failed to set the new mac
 * address within the time(timeout).
 * XGE_HAL_ERR_OUT_OF_MAC_ADDRESSES - Invalid MAC address index.
 *
 * See also: xge_hal_device_macaddr_get(), xge_hal_status_e{}.
 */
xge_hal_status_e
xge_hal_device_macaddr_set(xge_hal_device_t *hldev, int index,
	        macaddr_t macaddr)
{
	xge_hal_pci_bar0_t *bar0 =
	    (xge_hal_pci_bar0_t *)(void *)hldev->bar0;
	u64 val64, temp64;
	int i;

	if ( index >= XGE_HAL_MAX_MAC_ADDRESSES )
	    return XGE_HAL_ERR_OUT_OF_MAC_ADDRESSES;

	temp64 = 0;
	for (i=0; i < XGE_HAL_ETH_ALEN; i++) {
	    temp64 |= macaddr[i];
	    temp64 <<= 8;
	}
	temp64 >>= 8;

	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	                XGE_HAL_RMAC_ADDR_DATA0_MEM_ADDR(temp64),
	            &bar0->rmac_addr_data0_mem);

	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	                XGE_HAL_RMAC_ADDR_DATA1_MEM_MASK(0ULL),
	            &bar0->rmac_addr_data1_mem);

	val64 = ( XGE_HAL_RMAC_ADDR_CMD_MEM_WE |
	      XGE_HAL_RMAC_ADDR_CMD_MEM_STROBE_NEW_CMD |
	      XGE_HAL_RMAC_ADDR_CMD_MEM_OFFSET((index)) );

	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	                     &bar0->rmac_addr_cmd_mem);

	if (__hal_device_register_poll(hldev, &bar0->rmac_addr_cmd_mem, 0,
	       XGE_HAL_RMAC_ADDR_CMD_MEM_STROBE_CMD_EXECUTING,
	       XGE_HAL_DEVICE_CMDMEM_WAIT_MAX_MILLIS) != XGE_HAL_OK) {
	    /* upper layer may require to repeat */
	    return XGE_HAL_INF_MEM_STROBE_CMD_EXECUTING;
	}

	return XGE_HAL_OK;
}

/**
 * xge_hal_device_macaddr_clear - Set MAC address.
 * @hldev: HAL device handle.
 * @index: MAC address index, in the range from 0 to
 * XGE_HAL_MAX_MAC_ADDRESSES.
 *
 * Clear one of the available MAC address "slots".
 *
 * Returns: XGE_HAL_OK - success.
 * XGE_HAL_INF_MEM_STROBE_CMD_EXECUTING - Failed to set the new mac
 * address within the time(timeout).
 * XGE_HAL_ERR_OUT_OF_MAC_ADDRESSES - Invalid MAC address index.
 *
 * See also: xge_hal_device_macaddr_set(), xge_hal_status_e{}.
 */
xge_hal_status_e
xge_hal_device_macaddr_clear(xge_hal_device_t *hldev, int index)
{
	xge_hal_status_e status;
	u8 macaddr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

	status = xge_hal_device_macaddr_set(hldev, index, macaddr);
	if (status != XGE_HAL_OK) {
	    xge_debug_device(XGE_ERR, "%s",
	        "Not able to set the mac addr");
	    return status;
	}

	return XGE_HAL_OK;
}

/**
 * xge_hal_device_macaddr_find - Finds index in the rmac table.
 * @hldev: HAL device handle.
 * @wanted: Wanted MAC address.
 *
 * See also: xge_hal_device_macaddr_set().
 */
int
xge_hal_device_macaddr_find(xge_hal_device_t *hldev, macaddr_t wanted)
{
	int i;

	if (hldev == NULL) {
	    return XGE_HAL_ERR_INVALID_DEVICE;
	}

	for (i=1; i<XGE_HAL_MAX_MAC_ADDRESSES; i++) {
	    macaddr_t macaddr;
	    (void) xge_hal_device_macaddr_get(hldev, i, &macaddr);
	    if (!xge_os_memcmp(macaddr, wanted, sizeof(macaddr_t))) {
	        return i;
	    }
	}

	return -1;
}

/**
 * xge_hal_device_mtu_set - Set MTU.
 * @hldev: HAL device handle.
 * @new_mtu: New MTU size to configure.
 *
 * Set new MTU value. Example, to use jumbo frames:
 * xge_hal_device_mtu_set(my_device, my_channel, 9600);
 *
 * Returns: XGE_HAL_OK on success.
 * XGE_HAL_ERR_SWAPPER_CTRL - Failed to configure swapper control
 * register.
 * XGE_HAL_INF_MEM_STROBE_CMD_EXECUTING - Failed to initialize TTI/RTI
 * schemes.
 * XGE_HAL_ERR_DEVICE_IS_NOT_QUIESCENT - Failed to restore the device to
 * a "quiescent" state.
 */
xge_hal_status_e
xge_hal_device_mtu_set(xge_hal_device_t *hldev, int new_mtu)
{
	xge_hal_status_e status;

	/*
	 * reset needed if 1) new MTU differs, and
	 * 2a) device was closed or
	 * 2b) device is being upped for first time.
	 */
	if (hldev->config.mtu != new_mtu) {
	    if (hldev->reset_needed_after_close ||
	        !hldev->mtu_first_time_set) {
	        status = xge_hal_device_reset(hldev);
	        if (status != XGE_HAL_OK) {
	            xge_debug_device(XGE_TRACE, "%s",
	                  "fatal: can not reset the device");
	            return status;
	        }
	    }
	    /* store the new MTU in device, reset will use it */
	    hldev->config.mtu = new_mtu;
	    xge_debug_device(XGE_TRACE, "new MTU %d applied",
	             new_mtu);
	}

	if (!hldev->mtu_first_time_set)
	    hldev->mtu_first_time_set = 1;

	return XGE_HAL_OK;
}

/**
 * xge_hal_device_initialize - Initialize Xframe device.
 * @hldev: HAL device handle.
 * @attr: pointer to xge_hal_device_attr_t structure
 * @device_config: Configuration to be _applied_ to the device,
 *                 For the Xframe configuration "knobs" please
 *                 refer to xge_hal_device_config_t and Xframe
 *                 User Guide.
 *
 * Initialize Xframe device. Note that all the arguments of this public API
 * are 'IN', including @hldev. Upper-layer driver (ULD) cooperates with
 * OS to find new Xframe device, locate its PCI and memory spaces.
 *
 * When done, the ULD allocates sizeof(xge_hal_device_t) bytes for HAL
 * to enable the latter to perform Xframe hardware initialization.
 *
 * Returns: XGE_HAL_OK - success.
 * XGE_HAL_ERR_DRIVER_NOT_INITIALIZED - Driver is not initialized.
 * XGE_HAL_ERR_BAD_DEVICE_CONFIG - Device configuration params are not
 * valid.
 * XGE_HAL_ERR_OUT_OF_MEMORY - Memory allocation failed.
 * XGE_HAL_ERR_BAD_SUBSYSTEM_ID - Device subsystem id is invalid.
 * XGE_HAL_ERR_INVALID_MAC_ADDRESS - Device mac address in not valid.
 * XGE_HAL_INF_MEM_STROBE_CMD_EXECUTING - Failed to retrieve the mac
 * address within the time(timeout) or TTI/RTI initialization failed.
 * XGE_HAL_ERR_SWAPPER_CTRL - Failed to configure swapper control.
 * XGE_HAL_ERR_DEVICE_IS_NOT_QUIESCENT -Device is not queiscent.
 *
 * See also: xge_hal_device_terminate(), xge_hal_status_e{}
 * xge_hal_device_attr_t{}.
 */
xge_hal_status_e
xge_hal_device_initialize(xge_hal_device_t *hldev, xge_hal_device_attr_t *attr,
	    xge_hal_device_config_t *device_config)
{
	int i;
	xge_hal_status_e status;
	xge_hal_channel_t *channel;
	u16 subsys_device;
	u16 subsys_vendor;
	int total_dram_size, ring_auto_dram_cfg, left_dram_size;
	int total_dram_size_max = 0;

	xge_debug_device(XGE_TRACE, "device 0x"XGE_OS_LLXFMT" is initializing",
	         (unsigned long long)(ulong_t)hldev);

	/* sanity check */
	if (g_xge_hal_driver == NULL ||
	    !g_xge_hal_driver->is_initialized) {
	    return XGE_HAL_ERR_DRIVER_NOT_INITIALIZED;
	}

	xge_os_memzero(hldev, sizeof(xge_hal_device_t));

	/*
	 * validate a common part of Xframe-I/II configuration
	 * (and run check_card() later, once PCI inited - see below)
	 */
	status = __hal_device_config_check_common(device_config);
	if (status != XGE_HAL_OK)
	    return status;

	/* apply config */
	xge_os_memcpy(&hldev->config, device_config,
	                  sizeof(xge_hal_device_config_t));

	/* save original attr */
	xge_os_memcpy(&hldev->orig_attr, attr,
	                  sizeof(xge_hal_device_attr_t));

	/* initialize rxufca_intr_thres */
	hldev->rxufca_intr_thres = hldev->config.rxufca_intr_thres;

	hldev->regh0 = attr->regh0;
	hldev->regh1 = attr->regh1;
	hldev->regh2 = attr->regh2;
	hldev->isrbar0 = hldev->bar0 = attr->bar0;
	hldev->bar1 = attr->bar1;
	hldev->bar2 = attr->bar2;
	hldev->pdev = attr->pdev;
	hldev->irqh = attr->irqh;
	hldev->cfgh = attr->cfgh;

	/* set initial bimodal timer for bimodal adaptive schema */
	hldev->bimodal_timer_val_us = hldev->config.bimodal_timer_lo_us;

	hldev->queueh = xge_queue_create(hldev->pdev, hldev->irqh,
	              g_xge_hal_driver->config.queue_size_initial,
	              g_xge_hal_driver->config.queue_size_max,
	              __hal_device_event_queued, hldev);
	if (hldev->queueh == NULL)
	    return XGE_HAL_ERR_OUT_OF_MEMORY;

	hldev->magic = XGE_HAL_MAGIC;

	xge_assert(hldev->regh0);
	xge_assert(hldev->regh1);
	xge_assert(hldev->bar0);
	xge_assert(hldev->bar1);
	xge_assert(hldev->pdev);
	xge_assert(hldev->irqh);
	xge_assert(hldev->cfgh);

	/* initialize some PCI/PCI-X fields of this PCI device. */
	__hal_device_pci_init(hldev);

	/*
	 * initlialize lists to properly handling a potential
	 * terminate request
	 */
	xge_list_init(&hldev->free_channels);
	xge_list_init(&hldev->fifo_channels);
	xge_list_init(&hldev->ring_channels);

	if (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_XENA) {
	    /* fixups for xena */
	    hldev->config.rth_en = 0;
	    hldev->config.rth_spdm_en = 0;
	    hldev->config.rts_mac_en = 0;
	    total_dram_size_max = XGE_HAL_MAX_RING_QUEUE_SIZE_XENA;

	    status = __hal_device_config_check_xena(device_config);
	    if (status != XGE_HAL_OK) {
	        xge_hal_device_terminate(hldev);
	        return status;
	    }
	    if (hldev->config.bimodal_interrupts == 1) {
	        xge_hal_device_terminate(hldev);
	        return XGE_HAL_BADCFG_BIMODAL_XENA_NOT_ALLOWED;
	    } else if (hldev->config.bimodal_interrupts ==
	        XGE_HAL_DEFAULT_USE_HARDCODE)
	        hldev->config.bimodal_interrupts = 0;
	} else if (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_HERC) {
	    /* fixups for herc */
	    total_dram_size_max = XGE_HAL_MAX_RING_QUEUE_SIZE_HERC;
	    status = __hal_device_config_check_herc(device_config);
	    if (status != XGE_HAL_OK) {
	        xge_hal_device_terminate(hldev);
	        return status;
	    }
	    if (hldev->config.bimodal_interrupts ==
	        XGE_HAL_DEFAULT_USE_HARDCODE)
	        hldev->config.bimodal_interrupts = 1;
	} else {
	    xge_debug_device(XGE_ERR,
	          "detected unknown device_id 0x%x", hldev->device_id);
	    xge_hal_device_terminate(hldev);
	    return XGE_HAL_ERR_BAD_DEVICE_ID;
	}

	/* allocate and initialize FIFO types of channels according to
	 * configuration */
	for (i = 0; i < XGE_HAL_MAX_FIFO_NUM; i++) {
	    if (!device_config->fifo.queue[i].configured)
	        continue;

	    channel = __hal_channel_allocate(hldev, i,
	                     XGE_HAL_CHANNEL_TYPE_FIFO);
	    if (channel == NULL) {
	        xge_debug_device(XGE_ERR,
	            "fifo: __hal_channel_allocate failed");
	        xge_hal_device_terminate(hldev);
	        return XGE_HAL_ERR_OUT_OF_MEMORY;
	    }
	    /* add new channel to the device */
	    xge_list_insert(&channel->item, &hldev->free_channels);
	}

	/*
	 * automatic DRAM adjustment
	 */
	total_dram_size = 0;
	ring_auto_dram_cfg = 0;
	for (i = 0; i < XGE_HAL_MAX_RING_NUM; i++) {
	    if (!device_config->ring.queue[i].configured)
	        continue;
	    if (device_config->ring.queue[i].dram_size_mb ==
	        XGE_HAL_DEFAULT_USE_HARDCODE) {
	        ring_auto_dram_cfg++;
	        continue;
	    }
	    total_dram_size += device_config->ring.queue[i].dram_size_mb;
	}
	left_dram_size = total_dram_size_max - total_dram_size;
	if (left_dram_size < 0 ||
	    (ring_auto_dram_cfg && left_dram_size / ring_auto_dram_cfg == 0))  {
	    xge_debug_device(XGE_ERR,
	         "ring config: exceeded DRAM size %d MB",
	         total_dram_size_max);
	    xge_hal_device_terminate(hldev);
	            return XGE_HAL_BADCFG_RING_QUEUE_SIZE;
	    }

	/*
	 * allocate and initialize RING types of channels according to
	 * configuration
	 */
	for (i = 0; i < XGE_HAL_MAX_RING_NUM; i++) {
	    if (!device_config->ring.queue[i].configured)
	        continue;

	    if (device_config->ring.queue[i].dram_size_mb ==
	        XGE_HAL_DEFAULT_USE_HARDCODE) {
	        hldev->config.ring.queue[i].dram_size_mb =
	            device_config->ring.queue[i].dram_size_mb =
	                left_dram_size / ring_auto_dram_cfg;
	    }

	    channel = __hal_channel_allocate(hldev, i,
	                     XGE_HAL_CHANNEL_TYPE_RING);
	    if (channel == NULL) {
	        xge_debug_device(XGE_ERR,
	            "ring: __hal_channel_allocate failed");
	        xge_hal_device_terminate(hldev);
	        return XGE_HAL_ERR_OUT_OF_MEMORY;
	    }
	    /* add new channel to the device */
	    xge_list_insert(&channel->item, &hldev->free_channels);
	}

	/* get subsystem IDs */
	xge_os_pci_read16(hldev->pdev, hldev->cfgh,
	    xge_offsetof(xge_hal_pci_config_le_t, subsystem_id),
	    &subsys_device);
	xge_os_pci_read16(hldev->pdev, hldev->cfgh,
	    xge_offsetof(xge_hal_pci_config_le_t, subsystem_vendor_id),
	    &subsys_vendor);
	xge_debug_device(XGE_TRACE,
	                     "subsystem_id %04x:%04x",
	                     subsys_vendor, subsys_device);

	/* reset device initially */
	(void) __hal_device_reset(hldev);

	/* set host endian before, to assure proper action */
	status = __hal_device_set_swapper(hldev);
	if (status != XGE_HAL_OK) {
	    xge_debug_device(XGE_ERR,
	        "__hal_device_set_swapper failed");
	    xge_hal_device_terminate(hldev);
	    (void) __hal_device_reset(hldev);
	    return status;
	}

#ifndef XGE_HAL_HERC_EMULATION
	if (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_XENA)
	    __hal_device_xena_fix_mac(hldev);
#endif

	/*  MAC address initialization.
	 *  For now only one mac address will be read and used.  */
	status = xge_hal_device_macaddr_get(hldev, 0, &hldev->macaddr[0]);
	if (status != XGE_HAL_OK) {
	    xge_debug_device(XGE_ERR,
	        "xge_hal_device_macaddr_get failed");
	    xge_hal_device_terminate(hldev);
	    return status;
	}

	if (hldev->macaddr[0][0] == 0xFF &&
	    hldev->macaddr[0][1] == 0xFF &&
	    hldev->macaddr[0][2] == 0xFF &&
	    hldev->macaddr[0][3] == 0xFF &&
	    hldev->macaddr[0][4] == 0xFF &&
	    hldev->macaddr[0][5] == 0xFF) {
	    xge_debug_device(XGE_ERR,
	        "xge_hal_device_macaddr_get returns all FFs");
	    xge_hal_device_terminate(hldev);
	    return XGE_HAL_ERR_INVALID_MAC_ADDRESS;
	}

	xge_debug_device(XGE_TRACE,
	          "default macaddr: 0x%02x-%02x-%02x-%02x-%02x-%02x",
	          hldev->macaddr[0][0], hldev->macaddr[0][1],
	          hldev->macaddr[0][2], hldev->macaddr[0][3],
	          hldev->macaddr[0][4], hldev->macaddr[0][5]);

	status = __hal_stats_initialize(&hldev->stats, hldev);
	if (status != XGE_HAL_OK) {
	    xge_debug_device(XGE_ERR,
	        "__hal_stats_initialize failed");
	    xge_hal_device_terminate(hldev);
	    return status;
	}

	status = __hal_device_hw_initialize(hldev);
	if (status != XGE_HAL_OK) {
	    xge_debug_device(XGE_ERR,
	        "__hal_device_hw_initialize failed");
	    xge_hal_device_terminate(hldev);
	    return status;
	}
	hldev->dump_buf=(char*)xge_os_malloc(hldev->pdev, XGE_HAL_DUMP_BUF_SIZE);
	if (hldev->dump_buf == NULL)  {
	    xge_debug_device(XGE_ERR,
	        "__hal_device_hw_initialize failed");
	    xge_hal_device_terminate(hldev);
	            return XGE_HAL_ERR_OUT_OF_MEMORY;
	}


	/* Xena-only: need to serialize fifo posts across all device fifos */
#if defined(XGE_HAL_TX_MULTI_POST)
	xge_os_spin_lock_init(&hldev->xena_post_lock, hldev->pdev);
#elif defined(XGE_HAL_TX_MULTI_POST_IRQ)
	xge_os_spin_lock_init_irq(&hldev->xena_post_lock, hldev->irqh);
#endif
	 /* Getting VPD data */
	    __hal_device_get_vpd_data(hldev);
	
	hldev->is_initialized = 1;

	return XGE_HAL_OK;
}

/**
 * xge_hal_device_terminating - Mark the device as 'terminating'.
 * @devh: HAL device handle.
 *
 * Mark the device as 'terminating', going to terminate. Can be used
 * to serialize termination with other running processes/contexts.
 *
 * See also: xge_hal_device_terminate().
 */
void
xge_hal_device_terminating(xge_hal_device_h devh)
{
	xge_hal_device_t *hldev = (xge_hal_device_t*)devh;
	xge_list_t *item;
	xge_hal_channel_t *channel;
#if defined(XGE_HAL_TX_MULTI_RESERVE_IRQ)
	unsigned long flags=0;
#endif

	/*
	 * go through each opened tx channel and aquire
	 * lock, so it will serialize with HAL termination flag
	 */
	xge_list_for_each(item, &hldev->fifo_channels) {
	    channel = xge_container_of(item, xge_hal_channel_t, item);
#if defined(XGE_HAL_TX_MULTI_RESERVE)
	    xge_os_spin_lock(&channel->reserve_lock);
#elif defined(XGE_HAL_TX_MULTI_RESERVE_IRQ)
	    xge_os_spin_lock_irq(&channel->reserve_lock, flags);
#endif

	    channel->terminating = 1;

#if defined(XGE_HAL_TX_MULTI_RESERVE)
	    xge_os_spin_unlock(&channel->reserve_lock);
#elif defined(XGE_HAL_TX_MULTI_RESERVE_IRQ)
	    xge_os_spin_unlock_irq(&channel->reserve_lock, flags);
#endif
	}

	hldev->terminating = 1;
}

/**
 * xge_hal_device_terminate - Terminate Xframe device.
 * @hldev: HAL device handle.
 *
 * Terminate HAL device.
 *
 * See also: xge_hal_device_initialize().
 */
void
xge_hal_device_terminate(xge_hal_device_t *hldev)
{
	xge_assert(g_xge_hal_driver != NULL);
	xge_assert(hldev != NULL);
	xge_assert(hldev->magic == XGE_HAL_MAGIC);

	xge_queue_flush(hldev->queueh);

	hldev->terminating = 1;
	hldev->is_initialized = 0;
	    hldev->in_poll = 0;
	hldev->magic = XGE_HAL_DEAD;

#if defined(XGE_HAL_TX_MULTI_POST)
	xge_os_spin_lock_destroy(&hldev->xena_post_lock, hldev->pdev);
#elif defined(XGE_HAL_TX_MULTI_POST_IRQ)
	xge_os_spin_lock_destroy_irq(&hldev->xena_post_lock, hldev->pdev);
#endif

	xge_debug_device(XGE_TRACE, "device "XGE_OS_LLXFMT" is terminating",
	            (unsigned long long)(ulong_t)hldev);

	xge_assert(xge_list_is_empty(&hldev->fifo_channels));
	xge_assert(xge_list_is_empty(&hldev->ring_channels));

	if (hldev->stats.is_initialized) {
	    __hal_stats_terminate(&hldev->stats);
	}

	/* close if open and free all channels */
	while (!xge_list_is_empty(&hldev->free_channels)) {
	    xge_hal_channel_t *channel = (xge_hal_channel_t*)
	                hldev->free_channels.next;

	    xge_assert(!channel->is_open);
	    xge_list_remove(&channel->item);
	    __hal_channel_free(channel);
	}

	if (hldev->queueh) {
	    xge_queue_destroy(hldev->queueh);
	}

	if (hldev->spdm_table) {
	    xge_os_free(hldev->pdev,
	          hldev->spdm_table[0],
	          (sizeof(xge_hal_spdm_entry_t) *
	            hldev->spdm_max_entries));
	    xge_os_free(hldev->pdev,
	          hldev->spdm_table,
	          (sizeof(xge_hal_spdm_entry_t *) *
	            hldev->spdm_max_entries));
	    xge_os_spin_lock_destroy(&hldev->spdm_lock, hldev->pdev);
	    hldev->spdm_table = NULL;
	}

	if (hldev->dump_buf)  {
	        xge_os_free(hldev->pdev, hldev->dump_buf,
	            XGE_HAL_DUMP_BUF_SIZE);
	    hldev->dump_buf = NULL;
	}

	if (hldev->device_id != 0) {
	    int j, pcisize;

	    pcisize = (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_HERC)?
	               XGE_HAL_PCISIZE_HERC : XGE_HAL_PCISIZE_XENA;
	    for (j = 0; j < pcisize; j++) {
	        xge_os_pci_write32(hldev->pdev, hldev->cfgh, j * 4,
	            *((u32*)&hldev->pci_config_space_bios + j));
	    }
	}
}
/**
 * __hal_device_get_vpd_data - Getting vpd_data.
 *
 *   @hldev: HAL device handle.
 *
 *   Getting  product name and serial number from vpd capabilites structure
 *
 */
void
__hal_device_get_vpd_data(xge_hal_device_t *hldev)
{
	u8 * vpd_data;
	u8   data;
	int  index = 0, count, fail = 0;
	u8   vpd_addr = XGE_HAL_CARD_XENA_VPD_ADDR;
	if (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_HERC)
	    vpd_addr = XGE_HAL_CARD_HERC_VPD_ADDR;

	xge_os_strcpy((char *) hldev->vpd_data.product_name,
	            "10 Gigabit Ethernet Adapter");
	xge_os_strcpy((char *) hldev->vpd_data.serial_num, "not available");

	vpd_data = ( u8*) xge_os_malloc(hldev->pdev, XGE_HAL_VPD_BUFFER_SIZE + 16);
	if ( vpd_data == 0 )
	    return;

	for (index = 0; index < XGE_HAL_VPD_BUFFER_SIZE; index +=4 ) {
	    xge_os_pci_write8(hldev->pdev, hldev->cfgh, (vpd_addr + 2), (u8)index);
	    xge_os_pci_read8(hldev->pdev, hldev->cfgh,(vpd_addr + 2), &data);
	    xge_os_pci_write8(hldev->pdev, hldev->cfgh, (vpd_addr + 3), 0);
	    for (count = 0; count < 5; count++ ) {
	        xge_os_mdelay(2);
	        xge_os_pci_read8(hldev->pdev, hldev->cfgh,(vpd_addr + 3), &data);
	        if (data == XGE_HAL_VPD_READ_COMPLETE)
	            break;
	    }

	    if (count >= 5) {
	        xge_os_printf("ERR, Reading VPD data failed");
	        fail = 1;
	        break;
	    }

	    xge_os_pci_read32(hldev->pdev, hldev->cfgh,(vpd_addr + 4),
	            (u32 *)&vpd_data[index]);
	}
	
	if(!fail) {

	    /* read serial number of adapter */
	    for (count = 0; count < XGE_HAL_VPD_BUFFER_SIZE; count++) {
	        if ((vpd_data[count] == 'S')     &&
	            (vpd_data[count + 1] == 'N') &&
	            (vpd_data[count + 2] < XGE_HAL_VPD_LENGTH)) {
	                memset(hldev->vpd_data.serial_num, 0, XGE_HAL_VPD_LENGTH);
	                memcpy(hldev->vpd_data.serial_num, &vpd_data[count + 3],
	                    vpd_data[count + 2]);
	                break;
	        }
	    }

	    if (vpd_data[1] < XGE_HAL_VPD_LENGTH) {
	        memset(hldev->vpd_data.product_name, 0, vpd_data[1]);
	        memcpy(hldev->vpd_data.product_name, &vpd_data[3], vpd_data[1]);
	    }

	}

	xge_os_free(hldev->pdev, vpd_data, XGE_HAL_VPD_BUFFER_SIZE + 16);
}

	
/**
 * xge_hal_device_handle_tcode - Handle transfer code.
 * @channelh: Channel handle.
 * @dtrh: Descriptor handle.
 * @t_code: One of the enumerated (and documented in the Xframe user guide)
 *          "transfer codes".
 *
 * Handle descriptor's transfer code. The latter comes with each completed
 * descriptor, see xge_hal_fifo_dtr_next_completed() and
 * xge_hal_ring_dtr_next_completed().
 * Transfer codes are enumerated in xgehal-fifo.h and xgehal-ring.h.
 *
 * Returns: one of the xge_hal_status_e{} enumerated types.
 * XGE_HAL_OK           - for success.
 * XGE_HAL_ERR_CRITICAL         - when encounters critical error.
 */
xge_hal_status_e
xge_hal_device_handle_tcode (xge_hal_channel_h channelh,
	             xge_hal_dtr_h dtrh, u8 t_code)
{
	xge_hal_channel_t *channel = (xge_hal_channel_t *)channelh;
	xge_hal_device_t *hldev = (xge_hal_device_t *)channel->devh;

	if (t_code > 15) {
	    xge_os_printf("invalid t_code %d", t_code);
	    return XGE_HAL_OK;
	}

	if (channel->type == XGE_HAL_CHANNEL_TYPE_FIFO) {
	        hldev->stats.sw_dev_err_stats.txd_t_code_err_cnt[t_code]++;

#if defined(XGE_HAL_DEBUG_BAD_TCODE)
	    xge_hal_fifo_txd_t *txdp = (xge_hal_fifo_txd_t *)dtrh;
	    xge_os_printf(""XGE_OS_LLXFMT":"XGE_OS_LLXFMT":"
	    XGE_OS_LLXFMT":"XGE_OS_LLXFMT,
	    txdp->control_1, txdp->control_2, txdp->buffer_pointer,
	    txdp->host_control);
#endif

	    /* handle link "down" immediately without going through
	     * xge_hal_device_poll() routine. */
	    if (t_code == XGE_HAL_TXD_T_CODE_LOSS_OF_LINK) {
	        /* link is down */
	        if (hldev->link_state != XGE_HAL_LINK_DOWN) {
	            xge_hal_pci_bar0_t *bar0 =
	            (xge_hal_pci_bar0_t *)(void *)hldev->bar0;
	            u64 val64;

	            hldev->link_state = XGE_HAL_LINK_DOWN;

	            val64 = xge_os_pio_mem_read64(hldev->pdev,
	                hldev->regh0, &bar0->adapter_control);

	            /* turn off LED */
	            val64 = val64 & (~XGE_HAL_ADAPTER_LED_ON);
	            xge_os_pio_mem_write64(hldev->pdev,
	                    hldev->regh0, val64,
	                    &bar0->adapter_control);

	            g_xge_hal_driver->uld_callbacks.link_down(
	                    hldev->upper_layer_info);
	        }
	    } else if (t_code == XGE_HAL_TXD_T_CODE_ABORT_BUFFER ||
	               t_code == XGE_HAL_TXD_T_CODE_ABORT_DTOR) {
	                    __hal_device_handle_targetabort(hldev);
	        return XGE_HAL_ERR_CRITICAL;
	    }
	    return XGE_HAL_ERR_PKT_DROP;
	} else if (channel->type == XGE_HAL_CHANNEL_TYPE_RING) {
	        hldev->stats.sw_dev_err_stats.rxd_t_code_err_cnt[t_code]++;

#if defined(XGE_HAL_DEBUG_BAD_TCODE)
	    xge_hal_ring_rxd_1_t *rxdp = (xge_hal_ring_rxd_1_t *)dtrh;
	    xge_os_printf(""XGE_OS_LLXFMT":"XGE_OS_LLXFMT":"XGE_OS_LLXFMT
	        ":"XGE_OS_LLXFMT, rxdp->control_1,
	        rxdp->control_2, rxdp->buffer0_ptr,
	        rxdp->host_control);
#endif
	    if (t_code == XGE_HAL_RXD_T_CODE_BAD_ECC) {
	        hldev->stats.sw_dev_err_stats.ecc_err_cnt++;
	        __hal_device_handle_eccerr(hldev, "rxd_t_code",
	                       (u64)t_code);
	        return XGE_HAL_ERR_CRITICAL;
	    } else if (t_code == XGE_HAL_RXD_T_CODE_PARITY ||
	           t_code == XGE_HAL_RXD_T_CODE_PARITY_ABORT) {
	        hldev->stats.sw_dev_err_stats.parity_err_cnt++;
	        __hal_device_handle_parityerr(hldev, "rxd_t_code",
	                          (u64)t_code);
	        return XGE_HAL_ERR_CRITICAL;
	    /* do not drop if detected unknown IPv6 extension */
	    } else if (t_code != XGE_HAL_RXD_T_CODE_UNKNOWN_PROTO) {
	        return XGE_HAL_ERR_PKT_DROP;
	    }
	}
	return XGE_HAL_OK;
}

/**
 * xge_hal_device_link_state - Get link state.
 * @devh: HAL device handle.
 * @ls: Link state, see xge_hal_device_link_state_e{}.
 *
 * Get link state.
 * Returns: XGE_HAL_OK.
 * See also: xge_hal_device_link_state_e{}.
 */
xge_hal_status_e xge_hal_device_link_state(xge_hal_device_h devh,
	        xge_hal_device_link_state_e *ls)
{
	xge_hal_device_t *hldev = (xge_hal_device_t *)devh;

	xge_assert(ls != NULL);
	*ls = hldev->link_state;
	return XGE_HAL_OK;
}

/**
 * xge_hal_device_sched_timer - Configure scheduled device interrupt.
 * @devh: HAL device handle.
 * @interval_us: Time interval, in miscoseconds.
 *            Unlike transmit and receive interrupts,
 *            the scheduled interrupt is generated independently of
 *            traffic, but purely based on time.
 * @one_shot: 1 - generate scheduled interrupt only once.
 *            0 - generate scheduled interrupt periodically at the specified
 *            @interval_us interval.
 *
 * (Re-)configure scheduled interrupt. Can be called at runtime to change
 * the setting, generate one-shot interrupts based on the resource and/or
 * traffic conditions, other purposes.
 * See also: xge_hal_device_config_t{}.
 */
void xge_hal_device_sched_timer(xge_hal_device_h devh, int interval_us,
	        int one_shot)
{
	u64 val64;
	xge_hal_device_t *hldev = (xge_hal_device_t *)devh;
	xge_hal_pci_bar0_t *bar0 =
	    (xge_hal_pci_bar0_t *)(void *)hldev->bar0;
	unsigned int interval = hldev->config.pci_freq_mherz * interval_us;

	interval = __hal_fix_time_ival_herc(hldev, interval);

	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                &bar0->scheduled_int_ctrl);
	if (interval) {
	    val64 &= XGE_HAL_SCHED_INT_PERIOD_MASK;
	    val64 |= XGE_HAL_SCHED_INT_PERIOD(interval);
	    if (one_shot) {
	        val64 |= XGE_HAL_SCHED_INT_CTRL_ONE_SHOT;
	    }
	    val64 |= XGE_HAL_SCHED_INT_CTRL_TIMER_EN;
	} else {
	    val64 &= ~XGE_HAL_SCHED_INT_CTRL_TIMER_EN;
	}

	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	             val64, &bar0->scheduled_int_ctrl);

	xge_debug_device(XGE_TRACE, "sched_timer 0x"XGE_OS_LLXFMT": %s",
	          (unsigned long long)val64,
	          interval ? "enabled" : "disabled");
}

/**
 * xge_hal_device_check_id - Verify device ID.
 * @devh: HAL device handle.
 *
 * Verify device ID.
 * Returns: one of the xge_hal_card_e{} enumerated types.
 * See also: xge_hal_card_e{}.
 */
xge_hal_card_e
xge_hal_device_check_id(xge_hal_device_h devh)
{
	xge_hal_device_t *hldev = (xge_hal_device_t *)devh;
	switch (hldev->device_id) {
	case XGE_PCI_DEVICE_ID_XENA_1:
	case XGE_PCI_DEVICE_ID_XENA_2:
	    return XGE_HAL_CARD_XENA;
	case XGE_PCI_DEVICE_ID_HERC_1:
	case XGE_PCI_DEVICE_ID_HERC_2:
	    return XGE_HAL_CARD_HERC;
	case XGE_PCI_DEVICE_ID_TITAN_1:
	case XGE_PCI_DEVICE_ID_TITAN_2:
	    return XGE_HAL_CARD_TITAN;
	default:
	    return XGE_HAL_CARD_UNKNOWN;
	}
}

/**
 * xge_hal_device_pci_info_get - Get PCI bus informations such as width,
 *           frequency, and mode from previously stored values.
 * @devh:       HAL device handle.
 * @pci_mode:       pointer to a variable of enumerated type
 *          xge_hal_pci_mode_e{}.
 * @bus_frequency:  pointer to a variable of enumerated type
 *          xge_hal_pci_bus_frequency_e{}.
 * @bus_width:      pointer to a variable of enumerated type
 *          xge_hal_pci_bus_width_e{}.
 *
 * Get pci mode, frequency, and PCI bus width.
 * Returns: one of the xge_hal_status_e{} enumerated types.
 * XGE_HAL_OK           - for success.
 * XGE_HAL_ERR_INVALID_DEVICE   - for invalid device handle.
 * See Also: xge_hal_pci_mode_e, xge_hal_pci_mode_e, xge_hal_pci_width_e.
 */
xge_hal_status_e
xge_hal_device_pci_info_get(xge_hal_device_h devh, xge_hal_pci_mode_e *pci_mode,
	    xge_hal_pci_bus_frequency_e *bus_frequency,
	    xge_hal_pci_bus_width_e *bus_width)
{
	xge_hal_status_e rc_status;
	xge_hal_device_t *hldev = (xge_hal_device_t *)devh;

	if (!hldev || !hldev->is_initialized || hldev->magic != XGE_HAL_MAGIC) {
	    rc_status =  XGE_HAL_ERR_INVALID_DEVICE;
	    xge_debug_device(XGE_ERR,
	            "xge_hal_device_pci_info_get error, rc %d for device %p",
	        rc_status, hldev);

	    return rc_status;
	}

	*pci_mode   = hldev->pci_mode;
	*bus_frequency  = hldev->bus_frequency;
	*bus_width  = hldev->bus_width;
	rc_status   = XGE_HAL_OK;
	return rc_status;
}

/**
 * xge_hal_reinitialize_hw
 * @hldev: private member of the device structure.
 *
 * This function will soft reset the NIC and re-initalize all the
 * I/O registers to the values they had after it's inital initialization
 * through the probe function.
 */
int xge_hal_reinitialize_hw(xge_hal_device_t * hldev)
{
	(void) xge_hal_device_reset(hldev);
	if (__hal_device_hw_initialize(hldev) != XGE_HAL_OK) {
	    xge_hal_device_terminate(hldev);
	    (void) __hal_device_reset(hldev);
	    return 1;
	}
	return 0;
}


/*
 * __hal_read_spdm_entry_line
 * @hldev: pointer to xge_hal_device_t structure
 * @spdm_line: spdm line in the spdm entry to be read.
 * @spdm_entry: spdm entry of the spdm_line in the SPDM table.
 * @spdm_line_val: Contains the value stored in the spdm line.
 *
 * SPDM table contains upto a maximum of 256 spdm entries.
 * Each spdm entry contains 8 lines and each line stores 8 bytes.
 * This function reads the spdm line(addressed by @spdm_line)
 * of the spdm entry(addressed by @spdm_entry) in
 * the SPDM table.
 */
xge_hal_status_e
__hal_read_spdm_entry_line(xge_hal_device_t *hldev, u8 spdm_line,
	        u16 spdm_entry, u64 *spdm_line_val)
{
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;
	u64 val64;

	val64 = XGE_HAL_RTS_RTH_SPDM_MEM_CTRL_STROBE |
	    XGE_HAL_RTS_RTH_SPDM_MEM_CTRL_LINE_SEL(spdm_line) |
	    XGE_HAL_RTS_RTH_SPDM_MEM_CTRL_OFFSET(spdm_entry);

	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	        &bar0->rts_rth_spdm_mem_ctrl);

	/* poll until done */
	if (__hal_device_register_poll(hldev,
	    &bar0->rts_rth_spdm_mem_ctrl, 0,
	    XGE_HAL_RTS_RTH_SPDM_MEM_CTRL_STROBE,
	    XGE_HAL_DEVICE_CMDMEM_WAIT_MAX_MILLIS) != XGE_HAL_OK) {

	    return XGE_HAL_INF_MEM_STROBE_CMD_EXECUTING;
	}

	*spdm_line_val = xge_os_pio_mem_read64(hldev->pdev,
	            hldev->regh0, &bar0->rts_rth_spdm_mem_data);
	return XGE_HAL_OK;
}


/*
 * __hal_get_free_spdm_entry
 * @hldev: pointer to xge_hal_device_t structure
 * @spdm_entry: Contains an index to the unused spdm entry in the SPDM table.
 *
 * This function returns an index of unused spdm entry in the SPDM
 * table.
 */
static xge_hal_status_e
__hal_get_free_spdm_entry(xge_hal_device_t *hldev, u16 *spdm_entry)
{
	xge_hal_status_e status;
	u64 spdm_line_val=0;

	/*
	 * Search in the local SPDM table for a free slot.
	 */
	*spdm_entry = 0;
	for(; *spdm_entry < hldev->spdm_max_entries; (*spdm_entry)++) {
	    if (hldev->spdm_table[*spdm_entry]->in_use) {
	        break;
	    }
	}

	if (*spdm_entry >= hldev->spdm_max_entries) {
	    return XGE_HAL_ERR_SPDM_TABLE_FULL;
	}

	/*
	 * Make sure that the corresponding spdm entry in the SPDM
	 * table is free.
	 * Seventh line of the spdm entry contains information about
	 * whether the entry is free or not.
	 */
	if ((status = __hal_read_spdm_entry_line(hldev, 7, *spdm_entry,
	                &spdm_line_val)) != XGE_HAL_OK) {
	    return status;
	}

	/* BIT(63) in spdm_line 7 corresponds to entry_enable bit */
	if ((spdm_line_val & BIT(63))) {
	    /*
	     * Log a warning
	     */
	    xge_debug_device(XGE_ERR, "Local SPDM table is not "
	          "consistent with the actual one for the spdm "
	          "entry %d", *spdm_entry);
	    return XGE_HAL_ERR_SPDM_TABLE_DATA_INCONSISTENT;
	}

	return XGE_HAL_OK;
}


/*
 * __hal_calc_jhash - Calculate Jenkins hash.
 * @msg: Jenkins hash algorithm key.
 * @length: Length of the key.
 * @golden_ratio: Jenkins hash golden ratio.
 * @init_value: Jenkins hash initial value.
 *
 * This function implements the Jenkins based algorithm used for the
 * calculation of the RTH hash.
 * Returns:  Jenkins hash value.
 *
 */
static u32
__hal_calc_jhash(u8 *msg, u32 length, u32 golden_ratio, u32 init_value)
{

	register u32 a,b,c,len;

	/*
	 * Set up the internal state
	 */
	len = length;
	a = b = golden_ratio;  /* the golden ratio; an arbitrary value */
	c = init_value;         /* the previous hash value */

	/*  handle most of the key */
	while (len >= 12)
	{
	    a += (msg[0] + ((u32)msg[1]<<8) + ((u32)msg[2]<<16)
	                     + ((u32)msg[3]<<24));
	    b += (msg[4] + ((u32)msg[5]<<8) + ((u32)msg[6]<<16)
	                     + ((u32)msg[7]<<24));
	    c += (msg[8] + ((u32)msg[9]<<8) + ((u32)msg[10]<<16)
	                     + ((u32)msg[11]<<24));
	    mix(a,b,c);
	    msg += 12; len -= 12;
	}

	/*  handle the last 11 bytes */
	c += length;
	switch(len)  /* all the case statements fall through */
	{
	    case 11: c+= ((u32)msg[10]<<24);
	         break;
	    case 10: c+= ((u32)msg[9]<<16);
	         break;
	    case 9 : c+= ((u32)msg[8]<<8);
	         break;
	    /* the first byte of c is reserved for the length */
	    case 8 : b+= ((u32)msg[7]<<24);
	         break;
	    case 7 : b+= ((u32)msg[6]<<16);
	         break;
	    case 6 : b+= ((u32)msg[5]<<8);
	         break;
	    case 5 : b+= msg[4];
	         break;
	    case 4 : a+= ((u32)msg[3]<<24);
	         break;
	    case 3 : a+= ((u32)msg[2]<<16);
	         break;
	    case 2 : a+= ((u32)msg[1]<<8);
	         break;
	    case 1 : a+= msg[0];
	         break;
	    /* case 0: nothing left to add */
	}

	mix(a,b,c);

	/* report the result */
	return c;
}


/**
 * xge_hal_spdm_entry_add - Add a new entry to the SPDM table.
 * @devh: HAL device handle.
 * @src_ip: Source ip address(IPv4/IPv6).
 * @dst_ip: Destination ip address(IPv4/IPv6).
 * @l4_sp: L4 source port.
 * @l4_dp: L4 destination port.
 * @is_tcp: Set to 1, if the protocol is TCP.
 *         0, if the protocol is UDP.
 * @is_ipv4: Set to 1, if the protocol is IPv4.
 *         0, if the protocol is IPv6.
 * @tgt_queue: Target queue to route the receive packet.
 *
 * This function add a new entry to the SPDM table.
 *
 * Returns:  XGE_HAL_OK - success.
 * XGE_HAL_ERR_SPDM_NOT_ENABLED -  SPDM support is not enabled.
 * XGE_HAL_INF_MEM_STROBE_CMD_EXECUTING - Failed to add a new entry with in
 *                  the time(timeout).
 * XGE_HAL_ERR_SPDM_TABLE_FULL - SPDM table is full.
 * XGE_HAL_ERR_SPDM_INVALID_ENTRY - Invalid SPDM entry.
 *
 * See also: xge_hal_spdm_entry_remove{}.
 */
xge_hal_status_e
xge_hal_spdm_entry_add(xge_hal_device_h devh, xge_hal_ipaddr_t *src_ip,
	    xge_hal_ipaddr_t *dst_ip, u16 l4_sp, u16 l4_dp,
	    u8 is_tcp, u8 is_ipv4, u8 tgt_queue)
{

	xge_hal_device_t *hldev = (xge_hal_device_t *)devh;
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;
	u32 jhash_value;
	u32 jhash_init_val;
	u32 jhash_golden_ratio;
	u64 val64;
	int off;
	u16 spdm_entry;
	u8  msg[XGE_HAL_JHASH_MSG_LEN];
	int ipaddr_len;
	xge_hal_status_e status;


	if (!hldev->config.rth_spdm_en) {
	    return XGE_HAL_ERR_SPDM_NOT_ENABLED;
	}

	if ((tgt_queue <  XGE_HAL_MIN_RING_NUM) ||
	    (tgt_queue  >  XGE_HAL_MAX_RING_NUM)) {
	    return XGE_HAL_ERR_SPDM_INVALID_ENTRY;
	}


	/*
	 * Calculate the jenkins hash.
	 */
	/*
	 * Create the Jenkins hash algorithm key.
	 * key = {L3SA, L3DA, L4SP, L4DP}, if SPDM is configured to
	 * use L4 information. Otherwize key = {L3SA, L3DA}.
	 */

	if (is_ipv4) {
	    ipaddr_len = 4;   // In bytes
	} else {
	    ipaddr_len = 16;
	}

	/*
	 * Jenkins hash algorithm expects the key in the big endian
	 * format. Since key is the byte array, memcpy won't work in the
	 * case of little endian. So, the current code extracts each
	 * byte starting from MSB and store it in the key.
	 */
	if (is_ipv4) {
	    for (off = 0; off < ipaddr_len; off++) {
	        u32 mask = vBIT32(0xff,(off*8),8);
	        int shift = 32-(off+1)*8;
	        msg[off] = (u8)((src_ip->ipv4.addr & mask) >> shift);
	        msg[off+ipaddr_len] =
	            (u8)((dst_ip->ipv4.addr & mask) >> shift);
	    }
	} else {
	    for (off = 0; off < ipaddr_len; off++) {
	        int loc = off % 8;
	        u64 mask = vBIT(0xff,(loc*8),8);
	        int shift = 64-(loc+1)*8;

	        msg[off] = (u8)((src_ip->ipv6.addr[off/8] & mask)
	                    >> shift);
	        msg[off+ipaddr_len] = (u8)((dst_ip->ipv6.addr[off/8]
	                        & mask) >> shift);
	    }
	}

	off = (2*ipaddr_len);

	if (hldev->config.rth_spdm_use_l4) {
	    msg[off] = (u8)((l4_sp & 0xff00) >> 8);
	    msg[off + 1] = (u8)(l4_sp & 0xff);
	    msg[off + 2] = (u8)((l4_dp & 0xff00) >> 8);
	    msg[off + 3] = (u8)(l4_dp & 0xff);
	    off += 4;
	}

	/*
	 * Calculate jenkins hash for this configuration
	 */
	val64 = xge_os_pio_mem_read64(hldev->pdev,
	                hldev->regh0,
	                &bar0->rts_rth_jhash_cfg);
	jhash_golden_ratio = (u32)(val64 >> 32);
	jhash_init_val = (u32)(val64 & 0xffffffff);

	jhash_value = __hal_calc_jhash(msg, off,
	                   jhash_golden_ratio,
	                   jhash_init_val);

	xge_os_spin_lock(&hldev->spdm_lock);

	/*
	 * Locate a free slot in the SPDM table. To avoid a seach in the
	 * actual SPDM table, which is very expensive in terms of time,
	 * we are maintaining a local copy of  the table and the search for
	 * the free entry is performed in the local table.
	 */
	if ((status = __hal_get_free_spdm_entry(hldev,&spdm_entry))
	        != XGE_HAL_OK) {
	    xge_os_spin_unlock(&hldev->spdm_lock);
	    return status;
	}

	/*
	 * Add this entry to the SPDM table
	 */
	status =  __hal_spdm_entry_add(hldev, src_ip, dst_ip, l4_sp, l4_dp,
	                 is_tcp, is_ipv4, tgt_queue,
	                 jhash_value, /* calculated jhash */
	                 spdm_entry);

	xge_os_spin_unlock(&hldev->spdm_lock);

	return status;
}

/**
 * xge_hal_spdm_entry_remove - Remove an entry from the SPDM table.
 * @devh: HAL device handle.
 * @src_ip: Source ip address(IPv4/IPv6).
 * @dst_ip: Destination ip address(IPv4/IPv6).
 * @l4_sp: L4 source port.
 * @l4_dp: L4 destination port.
 * @is_tcp: Set to 1, if the protocol is TCP.
 *         0, if the protocol os UDP.
 * @is_ipv4: Set to 1, if the protocol is IPv4.
 *         0, if the protocol is IPv6.
 *
 * This function remove an entry from the SPDM table.
 *
 * Returns:  XGE_HAL_OK - success.
 * XGE_HAL_ERR_SPDM_NOT_ENABLED -  SPDM support is not enabled.
 * XGE_HAL_INF_MEM_STROBE_CMD_EXECUTING - Failed to remove an entry with in
 *                  the time(timeout).
 * XGE_HAL_ERR_SPDM_ENTRY_NOT_FOUND - Unable to locate the entry in the SPDM
 *                  table.
 *
 * See also: xge_hal_spdm_entry_add{}.
 */
xge_hal_status_e
xge_hal_spdm_entry_remove(xge_hal_device_h devh, xge_hal_ipaddr_t *src_ip,
	    xge_hal_ipaddr_t *dst_ip, u16 l4_sp, u16 l4_dp,
	    u8 is_tcp, u8 is_ipv4)
{

	xge_hal_device_t *hldev = (xge_hal_device_t *)devh;
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;
	u64 val64;
	u16 spdm_entry;
	xge_hal_status_e status;
	u64 spdm_line_arr[8];
	u8 line_no;
	u8 spdm_is_tcp;
	u8 spdm_is_ipv4;
	u16 spdm_l4_sp;
	u16 spdm_l4_dp;

	if (!hldev->config.rth_spdm_en) {
	    return XGE_HAL_ERR_SPDM_NOT_ENABLED;
	}

	xge_os_spin_lock(&hldev->spdm_lock);

	/*
	 * Poll the rxpic_int_reg register until spdm ready bit is set or
	 * timeout happens.
	 */
	if (__hal_device_register_poll(hldev, &bar0->rxpic_int_reg, 1,
	        XGE_HAL_RX_PIC_INT_REG_SPDM_READY,
	        XGE_HAL_DEVICE_CMDMEM_WAIT_MAX_MILLIS) != XGE_HAL_OK) {

	    /* upper layer may require to repeat */
	    xge_os_spin_unlock(&hldev->spdm_lock);
	    return XGE_HAL_INF_MEM_STROBE_CMD_EXECUTING;
	}

	/*
	 * Clear the SPDM READY bit.
	 */
	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                           &bar0->rxpic_int_reg);
	val64 &= ~XGE_HAL_RX_PIC_INT_REG_SPDM_READY;
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	                  &bar0->rxpic_int_reg);

	/*
	 * Search in the local SPDM table to get the index of the
	 * corresponding entry in the SPDM table.
	 */
	spdm_entry = 0;
	for (;spdm_entry < hldev->spdm_max_entries; spdm_entry++) {
	    if ((!hldev->spdm_table[spdm_entry]->in_use) ||
	        (hldev->spdm_table[spdm_entry]->is_tcp != is_tcp) ||
	        (hldev->spdm_table[spdm_entry]->l4_sp != l4_sp) ||
	        (hldev->spdm_table[spdm_entry]->l4_dp != l4_dp) ||
	        (hldev->spdm_table[spdm_entry]->is_ipv4 != is_ipv4)) {
	        continue;
	    }

	    /*
	     * Compare the src/dst IP addresses of source and target
	     */
	    if (is_ipv4) {
	        if ((hldev->spdm_table[spdm_entry]->src_ip.ipv4.addr
	             != src_ip->ipv4.addr) ||
	            (hldev->spdm_table[spdm_entry]->dst_ip.ipv4.addr
	             != dst_ip->ipv4.addr)) {
	            continue;
	        }
	    } else {
	        if ((hldev->spdm_table[spdm_entry]->src_ip.ipv6.addr[0]
	             != src_ip->ipv6.addr[0]) ||
	            (hldev->spdm_table[spdm_entry]->src_ip.ipv6.addr[1]
	             != src_ip->ipv6.addr[1]) ||
	            (hldev->spdm_table[spdm_entry]->dst_ip.ipv6.addr[0]
	             != dst_ip->ipv6.addr[0]) ||
	            (hldev->spdm_table[spdm_entry]->dst_ip.ipv6.addr[1]
	             != dst_ip->ipv6.addr[1])) {
	            continue;
	        }
	    }
	    break;
	}

	if (spdm_entry >= hldev->spdm_max_entries) {
	    xge_os_spin_unlock(&hldev->spdm_lock);
	    return XGE_HAL_ERR_SPDM_ENTRY_NOT_FOUND;
	}

	/*
	 * Retrieve the corresponding entry from the SPDM table and
	 * make sure that the data is consistent.
	 */
	for(line_no = 0; line_no < 8; line_no++) {

	    /*
	     *  SPDM line 2,3,4 are valid only for IPv6 entry.
	     *  SPDM line 5 & 6 are reserved. We don't have to
	     *  read these entries in the above cases.
	     */
	    if (((is_ipv4) &&
	        ((line_no == 2)||(line_no == 3)||(line_no == 4))) ||
	         (line_no == 5) ||
	         (line_no == 6)) {
	        continue;
	    }

	    if ((status = __hal_read_spdm_entry_line(
	                hldev,
	                line_no,
	                spdm_entry,
	                &spdm_line_arr[line_no]))
	                        != XGE_HAL_OK) {
	        xge_os_spin_unlock(&hldev->spdm_lock);
	        return status;
	    }
	}

	/*
	 * Seventh line of the spdm entry contains the entry_enable
	 * bit. Make sure that the entry_enable bit of this spdm entry
	 * is set.
	 * To remove an entry from the SPDM table, reset this
	 * bit.
	 */
	if (!(spdm_line_arr[7] & BIT(63))) {
	    /*
	     * Log a warning
	     */
	    xge_debug_device(XGE_ERR, "Local SPDM table is not "
	        "consistent with the actual one for the spdm "
	        "entry %d ", spdm_entry);
	    goto err_exit;
	}

	/*
	 *  Retreive the L4 SP/DP, src/dst ip addresses from the SPDM
	 *  table and do a comparision.
	 */
	spdm_is_tcp = (u8)((spdm_line_arr[0] & BIT(59)) >> 4);
	spdm_is_ipv4 = (u8)(spdm_line_arr[0] & BIT(63));
	spdm_l4_sp = (u16)(spdm_line_arr[0] >> 48);
	spdm_l4_dp = (u16)((spdm_line_arr[0] >> 32) & 0xffff);


	if ((spdm_is_tcp != is_tcp) ||
	    (spdm_is_ipv4 != is_ipv4) ||
	    (spdm_l4_sp != l4_sp) ||
	    (spdm_l4_dp != l4_dp)) {
	    /*
	     * Log a warning
	     */
	    xge_debug_device(XGE_ERR, "Local SPDM table is not "
	        "consistent with the actual one for the spdm "
	        "entry %d ", spdm_entry);
	    goto err_exit;
	}

	if (is_ipv4) {
	    /* Upper 32 bits of spdm_line(64 bit) contains the
	     * src IPv4 address. Lower 32 bits of spdm_line
	     * contains the destination IPv4 address.
	     */
	    u32 temp_src_ip = (u32)(spdm_line_arr[1] >> 32);
	    u32 temp_dst_ip = (u32)(spdm_line_arr[1] & 0xffffffff);

	    if ((temp_src_ip != src_ip->ipv4.addr) ||
	        (temp_dst_ip != dst_ip->ipv4.addr)) {
	        xge_debug_device(XGE_ERR, "Local SPDM table is not "
	            "consistent with the actual one for the spdm "
	            "entry %d ", spdm_entry);
	        goto err_exit;
	    }

	} else {
	    /*
	     * SPDM line 1 & 2 contains the src IPv6 address.
	     * SPDM line 3 & 4 contains the dst IPv6 address.
	     */
	    if ((spdm_line_arr[1] != src_ip->ipv6.addr[0]) ||
	        (spdm_line_arr[2] != src_ip->ipv6.addr[1]) ||
	        (spdm_line_arr[3] != dst_ip->ipv6.addr[0]) ||
	        (spdm_line_arr[4] != dst_ip->ipv6.addr[1])) {

	        /*
	         * Log a warning
	         */
	        xge_debug_device(XGE_ERR, "Local SPDM table is not "
	            "consistent with the actual one for the spdm "
	            "entry %d ", spdm_entry);
	        goto err_exit;
	    }
	}

	/*
	 * Reset the entry_enable bit to zero
	 */
	spdm_line_arr[7] &= ~BIT(63);

	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	    spdm_line_arr[7],
	    (void *)((char *)hldev->spdm_mem_base +
	    (spdm_entry * 64) + (7 * 8)));

	/*
	 * Wait for the operation to be completed.
	 */
	if (__hal_device_register_poll(hldev,
	    &bar0->rxpic_int_reg, 1,
	    XGE_HAL_RX_PIC_INT_REG_SPDM_READY,
	    XGE_HAL_DEVICE_CMDMEM_WAIT_MAX_MILLIS) != XGE_HAL_OK) {
	    xge_os_spin_unlock(&hldev->spdm_lock);
	    return XGE_HAL_INF_MEM_STROBE_CMD_EXECUTING;
	}

	/*
	 * Make the corresponding spdm entry in the local SPDM table
	 * available for future use.
	 */
	hldev->spdm_table[spdm_entry]->in_use = 0;
	xge_os_spin_unlock(&hldev->spdm_lock);

	return XGE_HAL_OK;

err_exit:
	xge_os_spin_unlock(&hldev->spdm_lock);
	return XGE_HAL_ERR_SPDM_TABLE_DATA_INCONSISTENT;
}

/*
 * __hal_device_rti_set
 * @ring: The post_qid of the ring.
 * @channel: HAL channel of the ring.
 *
 * This function stores the RTI value associated for the MSI and
 * also unmasks this particular RTI in the rti_mask register.
 */
static void __hal_device_rti_set(int ring_qid, xge_hal_channel_t *channel)
{
	xge_hal_device_t *hldev = (xge_hal_device_t*)channel->devh;
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)hldev->bar0;
	u64 val64;

	if (hldev->config.intr_mode == XGE_HAL_INTR_MODE_MSI ||
	    hldev->config.intr_mode == XGE_HAL_INTR_MODE_MSIX)
	    channel->rti = (u8)ring_qid;

	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	            &bar0->rx_traffic_mask);
	val64 &= ~BIT(ring_qid);
	xge_os_pio_mem_write64(hldev->pdev,
	            hldev->regh0, val64,
	            &bar0->rx_traffic_mask);
}

/*
 * __hal_device_tti_set
 * @ring: The post_qid of the FIFO.
 * @channel: HAL channel the FIFO.
 *
 * This function stores the TTI value associated for the MSI and
 * also unmasks this particular TTI in the tti_mask register.
 */
static void __hal_device_tti_set(int fifo_qid, xge_hal_channel_t *channel)
{
	xge_hal_device_t *hldev = (xge_hal_device_t*)channel->devh;
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)hldev->bar0;
	u64 val64;

	if (hldev->config.intr_mode == XGE_HAL_INTR_MODE_MSI ||
	    hldev->config.intr_mode == XGE_HAL_INTR_MODE_MSIX)
	    channel->tti = (u8)fifo_qid;

	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	            &bar0->tx_traffic_mask);
	val64 &= ~BIT(fifo_qid);
	xge_os_pio_mem_write64(hldev->pdev,
	            hldev->regh0, val64,
	            &bar0->tx_traffic_mask);
}

/**
 * xge_hal_channel_msi_set - Associate a RTI with a ring or TTI with a
 * FIFO for a given MSI.
 * @channelh: HAL channel handle.
 * @msi: MSI Number associated with the channel.
 * @msi_msg: The MSI message associated with the MSI number above.
 *
 * This API will associate a given channel (either Ring or FIFO) with the
 * given MSI number. It will alo program the Tx_Mat/Rx_Mat tables in the
 * hardware to indicate this association to the hardware.
 */
xge_hal_status_e
xge_hal_channel_msi_set(xge_hal_channel_h channelh, int msi, u32 msi_msg)
{
	xge_hal_channel_t *channel = (xge_hal_channel_t *)channelh;
	xge_hal_device_t *hldev = (xge_hal_device_t*)channel->devh;
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)hldev->bar0;
	u64 val64;

	channel->msi_msg = msi_msg;
	if (channel->type == XGE_HAL_CHANNEL_TYPE_RING) {
	    int ring = channel->post_qid;
	    xge_debug_osdep(XGE_TRACE, "MSI Data: 0x%4x, Ring: %d,"
	            " MSI: %d", channel->msi_msg, ring, msi);
	    val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	        &bar0->rx_mat);
	    val64 |= XGE_HAL_SET_RX_MAT(ring, msi);
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	        &bar0->rx_mat);
	    __hal_device_rti_set(ring, channel);
	} else {
	    int fifo = channel->post_qid;
	    xge_debug_osdep(XGE_TRACE, "MSI Data: 0x%4x, Fifo: %d,"
	            " MSI: %d", channel->msi_msg, fifo, msi);
	    val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	        &bar0->tx_mat[0]);
	    val64 |= XGE_HAL_SET_TX_MAT(fifo, msi);
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	        &bar0->tx_mat[0]);
	    __hal_device_tti_set(fifo, channel);
	}

	 return XGE_HAL_OK;
}

/**
 * xge_hal_mask_msix - Begin IRQ processing.
 * @hldev: HAL device handle.
 * @msi_id:  MSI ID
 *
 * The function masks the msix interrupt for the given msi_id
 *
 * Note:
 *
 * Returns: 0,
 * Otherwise, XGE_HAL_ERR_WRONG_IRQ if the msix index is out of range
 * status.
 * See also:
 */
xge_hal_status_e
xge_hal_mask_msix(xge_hal_device_h devh, int msi_id)
{
	xge_hal_status_e  status = XGE_HAL_OK;
	xge_hal_device_t *hldev  = (xge_hal_device_t *)devh;
	u32              *bar2   = (u32 *)hldev->bar2;
	u32               val32;

	xge_assert(msi_id < XGE_HAL_MAX_MSIX_MESSAGES);

	val32 = xge_os_pio_mem_read32(hldev->pdev, hldev->regh2, &bar2[msi_id*4+3]);
	val32 |= 1;
	xge_os_pio_mem_write32(hldev->pdev, hldev->regh2, val32, &bar2[msi_id*4+3]);
	return status;
}

/**
 * xge_hal_mask_msix - Begin IRQ processing.
 * @hldev: HAL device handle.
 * @msi_id:  MSI ID
 *
 * The function masks the msix interrupt for the given msi_id
 *
 * Note:
 *
 * Returns: 0,
 * Otherwise, XGE_HAL_ERR_WRONG_IRQ if the msix index is out of range
 * status.
 * See also:
 */
xge_hal_status_e
xge_hal_unmask_msix(xge_hal_device_h devh, int msi_id)
{
	xge_hal_status_e  status = XGE_HAL_OK;
	xge_hal_device_t *hldev  = (xge_hal_device_t *)devh;
	u32              *bar2   = (u32 *)hldev->bar2;
	u32               val32;

	xge_assert(msi_id < XGE_HAL_MAX_MSIX_MESSAGES);

	val32 = xge_os_pio_mem_read32(hldev->pdev, hldev->regh2, &bar2[msi_id*4+3]);
	val32 &= ~1;
	xge_os_pio_mem_write32(hldev->pdev, hldev->regh2, val32, &bar2[msi_id*4+3]);
	return status;
}

/*
 * __hal_set_msix_vals
 * @devh: HAL device handle.
 * @msix_value: 32bit MSI-X value transferred across PCI to @msix_address.
 *              Filled in by this function.
 * @msix_address: 32bit MSI-X DMA address.
 *              Filled in by this function.
 * @msix_idx: index that corresponds to the (@msix_value, @msix_address)
 *            entry in the table of MSI-X (value, address) pairs.
 *
 * This function will program the hardware associating the given
 * address/value cobination to the specified msi number.
 */
static void __hal_set_msix_vals (xge_hal_device_h devh,
	             u32 *msix_value,
	             u64 *msix_addr,
	             int msix_idx)
{
	int cnt = 0;

	xge_hal_device_t *hldev = (xge_hal_device_t*)devh;
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)hldev->bar0;
	u64 val64;

	val64 = XGE_HAL_XMSI_NO(msix_idx) | XGE_HAL_XMSI_STROBE;
	__hal_pio_mem_write32_upper(hldev->pdev, hldev->regh0,
	        (u32)(val64 >> 32), &bar0->xmsi_access);
	__hal_pio_mem_write32_lower(hldev->pdev, hldev->regh0,
	               (u32)(val64), &bar0->xmsi_access);
	do {
	    val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                    &bar0->xmsi_access);
	    if (val64 & XGE_HAL_XMSI_STROBE)
	        break;
	    cnt++;
	    xge_os_mdelay(20);
	} while(cnt < 5);
	*msix_value = (u32)(xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	             &bar0->xmsi_data));
	*msix_addr = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	             &bar0->xmsi_address);
}

/**
 * xge_hal_channel_msix_set - Associate MSI-X with a channel.
 * @channelh: HAL channel handle.
 * @msix_idx: index that corresponds to a particular (@msix_value,
 *            @msix_address) entry in the MSI-X table.
 *
 * This API associates a given channel (either Ring or FIFO) with the
 * given MSI-X number. It programs the Xframe's Tx_Mat/Rx_Mat tables
 * to indicate this association.
 */
xge_hal_status_e
xge_hal_channel_msix_set(xge_hal_channel_h channelh, int msix_idx)
{
	xge_hal_channel_t *channel = (xge_hal_channel_t *)channelh;
	xge_hal_device_t *hldev = (xge_hal_device_t*)channel->devh;
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)hldev->bar0;
	u64 val64;

	 if (channel->type == XGE_HAL_CHANNEL_TYPE_RING) {
	     /* Currently Ring and RTI is one on one. */
	    int ring = channel->post_qid;
	    val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	        &bar0->rx_mat);
	    val64 |= XGE_HAL_SET_RX_MAT(ring, msix_idx);
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	        &bar0->rx_mat);
	    __hal_device_rti_set(ring, channel);
	    hldev->config.fifo.queue[channel->post_qid].intr_vector =
	                            msix_idx;
	 } else if (channel->type == XGE_HAL_CHANNEL_TYPE_FIFO) {
	    int fifo = channel->post_qid;
	    val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	        &bar0->tx_mat[0]);
	    val64 |= XGE_HAL_SET_TX_MAT(fifo, msix_idx);
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	        &bar0->tx_mat[0]);
	    __hal_device_tti_set(fifo, channel);
	    hldev->config.ring.queue[channel->post_qid].intr_vector =
	                            msix_idx;
	}
	 channel->msix_idx = msix_idx;
	__hal_set_msix_vals(hldev, &channel->msix_data,
	            &channel->msix_address,
	            channel->msix_idx);

	 return XGE_HAL_OK;
}

#if defined(XGE_HAL_CONFIG_LRO)
/**
 * xge_hal_lro_terminate - Terminate lro resources.
 * @lro_scale: Amount of  lro memory.
 * @hldev: Hal device structure.
 *
 */
void
xge_hal_lro_terminate(u32 lro_scale,
	            xge_hal_device_t *hldev)
{
}

/**
 * xge_hal_lro_init - Initiate lro resources.
 * @lro_scale: Amount of  lro memory.
 * @hldev: Hal device structure.
 * Note: For time being I am using only one LRO per device. Later on size
 * will be increased.
 */

xge_hal_status_e
xge_hal_lro_init(u32 lro_scale,
	       xge_hal_device_t *hldev)
{
	int i;

	if (hldev->config.lro_sg_size == XGE_HAL_DEFAULT_USE_HARDCODE)
	    hldev->config.lro_sg_size = XGE_HAL_LRO_DEFAULT_SG_SIZE;

	if (hldev->config.lro_frm_len == XGE_HAL_DEFAULT_USE_HARDCODE)
	    hldev->config.lro_frm_len = XGE_HAL_LRO_DEFAULT_FRM_LEN;

	for (i=0; i < XGE_HAL_MAX_RING_NUM; i++)
	{
	    xge_os_memzero(hldev->lro_desc[i].lro_pool,
	               sizeof(lro_t) * XGE_HAL_LRO_MAX_BUCKETS);

	    hldev->lro_desc[i].lro_next_idx = 0;
	    hldev->lro_desc[i].lro_recent = NULL;
	}

	return XGE_HAL_OK;
}
#endif


/**
 * xge_hal_device_poll - HAL device "polling" entry point.
 * @devh: HAL device.
 *
 * HAL "polling" entry point. Note that this is part of HAL public API.
 * Upper-Layer driver _must_ periodically poll HAL via
 * xge_hal_device_poll().
 *
 * HAL uses caller's execution context to serially process accumulated
 * slow-path events, such as link state changes and hardware error
 * indications.
 *
 * The rate of polling could be somewhere between 500us to 10ms,
 * depending on requirements (e.g., the requirement to support fail-over
 * could mean that 500us or even 100us polling interval need to be used).
 *
 * The need and motivation for external polling includes
 *
 *   - remove the error-checking "burden" from the HAL interrupt handler
 *     (see xge_hal_device_handle_irq());
 *
 *   - remove the potential source of portability issues by _not_
 *     implementing separate polling thread within HAL itself.
 *
 * See also: xge_hal_event_e{}, xge_hal_driver_config_t{}.
 * Usage: See ex_slow_path{}.
 */
void
xge_hal_device_poll(xge_hal_device_h devh)
{
	unsigned char item_buf[sizeof(xge_queue_item_t) +
	            XGE_DEFAULT_EVENT_MAX_DATA_SIZE];
	xge_queue_item_t *item = (xge_queue_item_t *)(void *)item_buf;
	xge_queue_status_e qstatus;
	xge_hal_status_e hstatus;
	int i = 0;
	int queue_has_critical_event = 0;
	xge_hal_device_t *hldev = (xge_hal_device_t*)devh;

  xge_os_memzero(item_buf, (sizeof(xge_queue_item_t) +
	                         XGE_DEFAULT_EVENT_MAX_DATA_SIZE));  

_again:
	if (!hldev->is_initialized ||
	    hldev->terminating ||
	    hldev->magic != XGE_HAL_MAGIC)
	    return;

	if(hldev->stats.sw_dev_err_stats.xpak_counter.tick_period < 72000)
	{
	    /*
	     * Wait for an Hour
	     */
	    hldev->stats.sw_dev_err_stats.xpak_counter.tick_period++;
	} else {
	    /*
	     * Logging Error messages in the excess temperature,
	     * Bias current, laser ouput for three cycle
	     */
	    __hal_updt_stats_xpak(hldev);
	    hldev->stats.sw_dev_err_stats.xpak_counter.tick_period = 0;
	}

	if (!queue_has_critical_event)
	        queue_has_critical_event =
	        __queue_get_reset_critical(hldev->queueh);

	hldev->in_poll = 1;
	while (i++ < XGE_HAL_DRIVER_QUEUE_CONSUME_MAX || queue_has_critical_event) {

	    qstatus = xge_queue_consume(hldev->queueh,
	                XGE_DEFAULT_EVENT_MAX_DATA_SIZE,
	                item);
	    if (qstatus == XGE_QUEUE_IS_EMPTY)
	        break;

	    xge_debug_queue(XGE_TRACE,
	         "queueh 0x"XGE_OS_LLXFMT" consumed event: %d ctxt 0x"
	         XGE_OS_LLXFMT, (u64)(ulong_t)hldev->queueh, item->event_type,
	         (u64)(ulong_t)item->context);

	    if (!hldev->is_initialized ||
	        hldev->magic != XGE_HAL_MAGIC) {
	        hldev->in_poll = 0;
	        return;
	    }

	    switch (item->event_type) {
	    case XGE_HAL_EVENT_LINK_IS_UP: {
	        if (!queue_has_critical_event &&
	            g_xge_hal_driver->uld_callbacks.link_up) {
	            g_xge_hal_driver->uld_callbacks.link_up(
	                hldev->upper_layer_info);
	            hldev->link_state = XGE_HAL_LINK_UP;
	        }
	    } break;
	    case XGE_HAL_EVENT_LINK_IS_DOWN: {
	        if (!queue_has_critical_event &&
	            g_xge_hal_driver->uld_callbacks.link_down) {
	            g_xge_hal_driver->uld_callbacks.link_down(
	                hldev->upper_layer_info);
	            hldev->link_state = XGE_HAL_LINK_DOWN;
	        }
	    } break;
	    case XGE_HAL_EVENT_SERR:
	    case XGE_HAL_EVENT_ECCERR:
	    case XGE_HAL_EVENT_PARITYERR:
	    case XGE_HAL_EVENT_TARGETABORT:
	    case XGE_HAL_EVENT_SLOT_FREEZE: {
	        void *item_data = xge_queue_item_data(item);
	        xge_hal_event_e event_type = item->event_type;
	        u64 val64 = *((u64*)item_data);

	        if (event_type != XGE_HAL_EVENT_SLOT_FREEZE)
	            if (xge_hal_device_is_slot_freeze(hldev))
	                event_type = XGE_HAL_EVENT_SLOT_FREEZE;
	        if (g_xge_hal_driver->uld_callbacks.crit_err) {
	            g_xge_hal_driver->uld_callbacks.crit_err(
	                hldev->upper_layer_info,
	                event_type,
	                val64);
	            /* handle one critical event per poll cycle */
	            hldev->in_poll = 0;
	            return;
	        }
	    } break;
	    default: {
	        xge_debug_queue(XGE_TRACE,
	            "got non-HAL event %d",
	            item->event_type);
	    } break;
	    }

	    /* broadcast this event */
	    if (g_xge_hal_driver->uld_callbacks.event)
	        g_xge_hal_driver->uld_callbacks.event(item);
	}

	if (g_xge_hal_driver->uld_callbacks.before_device_poll) {
	    if (g_xge_hal_driver->uld_callbacks.before_device_poll(
	                     hldev) != 0) {
	        hldev->in_poll = 0;
	        return;
	    }
	}

	hstatus = __hal_device_poll(hldev);
	if (g_xge_hal_driver->uld_callbacks.after_device_poll)
	    g_xge_hal_driver->uld_callbacks.after_device_poll(hldev);

	/*
	 * handle critical error right away:
	 * - walk the device queue again
	 * - drop non-critical events, if any
	 * - look for the 1st critical
	 */
	if (hstatus == XGE_HAL_ERR_CRITICAL) {
	        queue_has_critical_event = 1;
	    goto _again;
	}

	hldev->in_poll = 0;
}

/**
 * xge_hal_rts_rth_init - Set enhanced mode for  RTS hashing.
 * @hldev: HAL device handle.
 *
 * This function is used to set the adapter to enhanced mode.
 *
 * See also: xge_hal_rts_rth_clr(), xge_hal_rts_rth_set().
 */
void
xge_hal_rts_rth_init(xge_hal_device_t *hldev)
{
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;
	u64 val64;

	/*
	 * Set the receive traffic steering mode from default(classic)
	 * to enhanced.
	 */
	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                  &bar0->rts_ctrl);
	val64 |= XGE_HAL_RTS_CTRL_ENHANCED_MODE;
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	               val64, &bar0->rts_ctrl);
}

/**
 * xge_hal_rts_rth_clr - Clear RTS hashing.
 * @hldev: HAL device handle.
 *
 * This function is used to clear all RTS hashing related stuff.
 * It brings the adapter out from enhanced mode to classic mode.
 * It also clears RTS_RTH_CFG register i.e clears hash type, function etc.
 *
 * See also: xge_hal_rts_rth_set(), xge_hal_rts_rth_itable_set().
 */
void
xge_hal_rts_rth_clr(xge_hal_device_t *hldev)
{
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;
	u64 val64;

	/*
	 * Set the receive traffic steering mode from default(classic)
	 * to enhanced.
	 */
	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                  &bar0->rts_ctrl);
	val64 &=  ~XGE_HAL_RTS_CTRL_ENHANCED_MODE;
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	               val64, &bar0->rts_ctrl);
	val64 = 0;
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	               &bar0->rts_rth_cfg);
}

/**
 * xge_hal_rts_rth_set - Set/configure RTS hashing.
 * @hldev: HAL device handle.
 * @def_q: default queue
 * @hash_type: hash type i.e TcpIpV4, TcpIpV6 etc.
 * @bucket_size: no of least significant bits to be used for hashing.
 *
 * Used to set/configure all RTS hashing related stuff.
 * - set the steering mode to enhanced.
 * - set hash function i.e algo selection.
 * - set the default queue.
 *
 * See also: xge_hal_rts_rth_clr(), xge_hal_rts_rth_itable_set().
 */
void
xge_hal_rts_rth_set(xge_hal_device_t *hldev, u8 def_q, u64 hash_type,
	        u16 bucket_size)
{
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;
	u64 val64;

	val64 = XGE_HAL_RTS_DEFAULT_Q(def_q);
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	               &bar0->rts_default_q);

	val64 = hash_type;
	val64 |= XGE_HAL_RTS_RTH_EN;
	val64 |= XGE_HAL_RTS_RTH_BUCKET_SIZE(bucket_size);
	val64 |= XGE_HAL_RTS_RTH_ALG_SEL_MS;
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	               &bar0->rts_rth_cfg);
}

/**
 * xge_hal_rts_rth_start - Start RTS hashing.
 * @hldev: HAL device handle.
 *
 * Used to Start RTS hashing .
 *
 * See also: xge_hal_rts_rth_clr(), xge_hal_rts_rth_itable_set(), xge_hal_rts_rth_start.
 */
void
xge_hal_rts_rth_start(xge_hal_device_t *hldev)
{
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;
	u64 val64;


	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                  &bar0->rts_rth_cfg);
	val64 |= XGE_HAL_RTS_RTH_EN;
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	               &bar0->rts_rth_cfg);
}

/**
 * xge_hal_rts_rth_stop - Stop the RTS hashing.
 * @hldev: HAL device handle.
 *
 * Used to Staop RTS hashing .
 *
 * See also: xge_hal_rts_rth_clr(), xge_hal_rts_rth_itable_set(), xge_hal_rts_rth_start.
 */
void
xge_hal_rts_rth_stop(xge_hal_device_t *hldev)
{
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;
	u64 val64;

	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                  &bar0->rts_rth_cfg);
	val64 &=  ~XGE_HAL_RTS_RTH_EN;
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	               &bar0->rts_rth_cfg);
}

/**
 * xge_hal_rts_rth_itable_set - Set/configure indirection table (IT).
 * @hldev: HAL device handle.
 * @itable: Pointer to the indirection table
 * @itable_size: no of least significant bits to be used for hashing
 *
 * Used to set/configure indirection table.
 * It enables the required no of entries in the IT.
 * It adds entries to the IT.
 *
 * See also: xge_hal_rts_rth_clr(), xge_hal_rts_rth_set().
 */
xge_hal_status_e
xge_hal_rts_rth_itable_set(xge_hal_device_t *hldev, u8 *itable, u32 itable_size)
{
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)(void  *)hldev->bar0;
	u64 val64;
	u32 idx;

	for (idx = 0; idx < itable_size; idx++) {
	    val64 = XGE_HAL_RTS_RTH_MAP_MEM_DATA_ENTRY_EN |
	        XGE_HAL_RTS_RTH_MAP_MEM_DATA(itable[idx]);

	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	                   &bar0->rts_rth_map_mem_data);

	    /* execute */
	    val64 = (XGE_HAL_RTS_RTH_MAP_MEM_CTRL_WE |
	         XGE_HAL_RTS_RTH_MAP_MEM_CTRL_STROBE |
	         XGE_HAL_RTS_RTH_MAP_MEM_CTRL_OFFSET(idx));
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	                   &bar0->rts_rth_map_mem_ctrl);

	    /* poll until done */
	    if (__hal_device_register_poll(hldev,
	           &bar0->rts_rth_map_mem_ctrl, 0,
	           XGE_HAL_RTS_RTH_MAP_MEM_CTRL_STROBE,
	           XGE_HAL_DEVICE_CMDMEM_WAIT_MAX_MILLIS) != XGE_HAL_OK) {
	        /* upper layer may require to repeat */
	        return XGE_HAL_INF_MEM_STROBE_CMD_EXECUTING;
	    }
	}

	return XGE_HAL_OK;
}


/**
 * xge_hal_device_rts_rth_key_set - Configure 40byte secret for hash calc.
 *
 * @hldev: HAL device handle.
 * @KeySize: Number of 64-bit words
 * @Key: upto 40-byte array of 8-bit values
 * This function configures the 40-byte secret which is used for hash
 * calculation.
 *
 * See also: xge_hal_rts_rth_clr(), xge_hal_rts_rth_set().
 */
void
xge_hal_device_rts_rth_key_set(xge_hal_device_t *hldev, u8 KeySize, u8 *Key)
{
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)(void *) hldev->bar0;
	u64 val64;
	u32 entry, nreg, i;

	entry = 0;
	nreg = 0;

	while( KeySize ) {
	    val64 = 0;
	    for ( i = 0; i < 8 ; i++) {
	        /* Prepare 64-bit word for 'nreg' containing 8 keys. */
	        if (i)
	            val64 <<= 8;
	        val64 |= Key[entry++];
	    }

	    KeySize--;

	    /* temp64 = XGE_HAL_RTH_HASH_MASK_n(val64, (n<<3), (n<<3)+7);*/
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	                   &bar0->rts_rth_hash_mask[nreg++]);
	}

	while( nreg < 5 ) {
	    /* Clear the rest if key is less than 40 bytes */
	    val64 = 0;
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	                   &bar0->rts_rth_hash_mask[nreg++]);
	}
}


/**
 * xge_hal_device_is_closed - Device is closed
 *
 * @devh: HAL device handle.
 */
int
xge_hal_device_is_closed(xge_hal_device_h devh)
{
	xge_hal_device_t *hldev = (xge_hal_device_t *)devh;

	if (xge_list_is_empty(&hldev->fifo_channels) &&
	    xge_list_is_empty(&hldev->ring_channels))
	    return 1;

	return 0;
}

xge_hal_status_e
xge_hal_device_rts_section_enable(xge_hal_device_h devh, int index)
{
	u64 val64;
	int section;
	int max_addr = XGE_HAL_MAX_MAC_ADDRESSES;

	xge_hal_device_t *hldev = (xge_hal_device_t *)devh;
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;

	if (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_HERC)
	    max_addr = XGE_HAL_MAX_MAC_ADDRESSES_HERC;

	if ( index >= max_addr )
	    return XGE_HAL_ERR_OUT_OF_MAC_ADDRESSES;

	/*
	 * Calculate the section value
	 */
	section = index / 32;

	    xge_debug_device(XGE_TRACE, "the Section value is %d ", section);

	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	            &bar0->rts_mac_cfg);
	switch(section)
	{
	    case 0:
	        val64 |=  XGE_HAL_RTS_MAC_SECT0_EN;
	        break;
	    case 1:
	        val64 |=  XGE_HAL_RTS_MAC_SECT1_EN;
	        break;
	    case 2:
	        val64 |=  XGE_HAL_RTS_MAC_SECT2_EN;
	        break;
	    case 3:
	        val64 |=  XGE_HAL_RTS_MAC_SECT3_EN;
	        break;
	    case 4:
	        val64 |=  XGE_HAL_RTS_MAC_SECT4_EN;
	        break;
	    case 5:
	        val64 |=  XGE_HAL_RTS_MAC_SECT5_EN;
	        break;
	    case 6:
	        val64 |=  XGE_HAL_RTS_MAC_SECT6_EN;
	        break;
	    case 7:
	        val64 |=  XGE_HAL_RTS_MAC_SECT7_EN;
	        break;
	    default:
	        xge_debug_device(XGE_ERR, "Invalid Section value %d "
	                , section);
	    }

	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	            val64, &bar0->rts_mac_cfg);
	return XGE_HAL_OK;
}


/**
 * xge_hal_fix_rldram_ecc_error
 * @hldev: private member of the device structure.
 *
 * SXE-02-010. This function will turn OFF the ECC error reporting for the 
 * interface bet'n external Micron RLDRAM II device and memory controller.
 * The error would have been reported in RLD_ECC_DB_ERR_L and RLD_ECC_DB_ERR_U
 * fields of MC_ERR_REG register. Issue reported by HP-Unix folks during the
 * qualification of Herc. 
 */
xge_hal_status_e 
xge_hal_fix_rldram_ecc_error(xge_hal_device_t * hldev)
{
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)hldev->bar0;
	u64 val64;

	// Enter Test Mode. 
	val64 = XGE_HAL_MC_RLDRAM_TEST_MODE;
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	                       &bar0->mc_rldram_test_ctrl);

	// Enable fg/bg tests.
	val64 = 0x0100000000000000ULL;
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	                       &bar0->mc_driver);

	// Enable RLDRAM configuration.
	val64 = 0x0000000000017B00ULL;
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	                       &bar0->mc_rldram_mrs);

	// Enable RLDRAM queues. 
	val64 = 0x0000000001017B00ULL;
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	                       &bar0->mc_rldram_mrs);

	// Setup test ranges
	val64 = 0x00000000001E0100ULL;
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	                       &bar0->mc_rldram_test_add);

	val64 = 0x00000100001F0100ULL;
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	                       &bar0->mc_rldram_test_add_bkg);
	// Start Reads.
	val64 = 0x0001000000010000ULL;
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	                       &bar0->mc_rldram_test_ctrl);

	if (__hal_device_register_poll(hldev, &bar0->mc_rldram_test_ctrl, 1,
	                           XGE_HAL_MC_RLDRAM_TEST_DONE,
	                           XGE_HAL_DEVICE_CMDMEM_WAIT_MAX_MILLIS) != XGE_HAL_OK){
	    return XGE_HAL_INF_MEM_STROBE_CMD_EXECUTING;
	}

	// Exit test mode
	val64 = 0x0000000000000000ULL;
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	                       &bar0->mc_rldram_test_ctrl);

	return XGE_HAL_OK;
}
