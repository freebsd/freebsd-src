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
 * $FreeBSD: src/sys/dev/nxge/include/xgehal-mgmt.h,v 1.1.2.1.4.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef XGE_HAL_MGMT_H
#define XGE_HAL_MGMT_H

#include <dev/nxge/include/xge-os-pal.h>
#include <dev/nxge/include/xge-debug.h>
#include <dev/nxge/include/xgehal-types.h>
#include <dev/nxge/include/xgehal-config.h>
#include <dev/nxge/include/xgehal-stats.h>
#include <dev/nxge/include/xgehal-regs.h>
#include <dev/nxge/include/xgehal-device.h>

__EXTERN_BEGIN_DECLS

/**
 * struct xge_hal_mgmt_about_info_t - About info.
 * @vendor: PCI Vendor ID.
 * @device: PCI Device ID.
 * @subsys_vendor: PCI Subsystem Vendor ID.
 * @subsys_device: PCI Subsystem Device ID.
 * @board_rev: PCI Board revision, e.g. 3 - for Xena 3.
 * @vendor_name: Neterion, Inc.
 * @chip_name: Xframe.
 * @media: Fiber, copper.
 * @hal_major: HAL major version number.
 * @hal_minor: HAL minor version number.
 * @hal_fix: HAL fix number.
 * @hal_build: HAL build number.
 * @ll_major: Link-layer ULD major version number.
 * @ll_minor: Link-layer ULD minor version number.
 * @ll_fix: Link-layer ULD fix version number.
 * @ll_build: Link-layer ULD build number.
 * @transponder_temperature: TODO
 */
typedef struct xge_hal_mgmt_about_info_t {
	u16     vendor;
	u16     device;
	u16     subsys_vendor;
	u16     subsys_device;
	u8      board_rev;
	char        vendor_name[16];
	char        chip_name[16];
	char        media[16];
	char        hal_major[4];
	char        hal_minor[4];
	char        hal_fix[4];
	char        hal_build[16];
	char        ll_major[4];
	char        ll_minor[4];
	char        ll_fix[4];
	char        ll_build[16];
	u32     transponder_temperature;
} xge_hal_mgmt_about_info_t;

typedef xge_hal_stats_hw_info_t     xge_hal_mgmt_hw_stats_t;
typedef xge_hal_stats_pcim_info_t   xge_hal_mgmt_pcim_stats_t;
typedef xge_hal_stats_sw_err_t      xge_hal_mgmt_sw_stats_t;
typedef xge_hal_stats_device_info_t xge_hal_mgmt_device_stats_t;
typedef xge_hal_stats_channel_info_t    xge_hal_mgmt_channel_stats_t;
typedef xge_hal_device_config_t     xge_hal_mgmt_device_config_t;
typedef xge_hal_driver_config_t     xge_hal_mgmt_driver_config_t;
typedef xge_hal_pci_config_t        xge_hal_mgmt_pci_config_t;

xge_hal_status_e
xge_hal_mgmt_about(xge_hal_device_h devh, xge_hal_mgmt_about_info_t *about_info,
	    int size);

xge_hal_status_e
xge_hal_mgmt_hw_stats(xge_hal_device_h devh, xge_hal_mgmt_hw_stats_t *hw_stats,
	    int size);

xge_hal_status_e
xge_hal_mgmt_hw_stats_off(xge_hal_device_h devh, int off, int size, char *out);

xge_hal_status_e
xge_hal_mgmt_pcim_stats(xge_hal_device_h devh,
	    xge_hal_mgmt_pcim_stats_t *pcim_stats, int size);

xge_hal_status_e
xge_hal_mgmt_pcim_stats_off(xge_hal_device_h devh, int off, int size,
	    char *out);

xge_hal_status_e
xge_hal_mgmt_sw_stats(xge_hal_device_h devh, xge_hal_mgmt_sw_stats_t *hw_stats,
	    int size);

xge_hal_status_e
xge_hal_mgmt_device_stats(xge_hal_device_h devh,
	    xge_hal_mgmt_device_stats_t *device_stats, int size);

xge_hal_status_e
xge_hal_mgmt_channel_stats(xge_hal_channel_h channelh,
	    xge_hal_mgmt_channel_stats_t *channel_stats, int size);

xge_hal_status_e
xge_hal_mgmt_reg_read(xge_hal_device_h devh, int bar_id, unsigned int offset,
	    u64 *value);

xge_hal_status_e
xge_hal_mgmt_reg_write(xge_hal_device_h devh, int bar_id, unsigned int offset,
	    u64 value);

xge_hal_status_e
xge_hal_mgmt_pcireg_read(xge_hal_device_h devh, unsigned int offset,
	    int bits, u32 *value);

xge_hal_status_e
xge_hal_mgmt_device_config(xge_hal_device_h devh,
	    xge_hal_mgmt_device_config_t *dev_config, int size);

xge_hal_status_e
xge_hal_mgmt_driver_config(xge_hal_mgmt_driver_config_t *drv_config,
	    int size);

xge_hal_status_e
xge_hal_mgmt_pci_config(xge_hal_device_h devh,
	    xge_hal_mgmt_pci_config_t *pci_config, int size);

xge_hal_status_e
xge_hal_pma_loopback( xge_hal_device_h devh, int enable );

xge_hal_status_e
xge_hal_rldram_test(xge_hal_device_h devh, u64 * data);

u16
xge_hal_mdio_read( xge_hal_device_h devh, u32 mmd_type, u64 addr );

xge_hal_status_e
xge_hal_mdio_write( xge_hal_device_h devh, u32 mmd_type, u64 addr, u32 value );

u32
xge_hal_read_xfp_current_temp(xge_hal_device_h devh);

xge_hal_status_e
xge_hal_read_eeprom(xge_hal_device_h devh, int off, u32* data);

xge_hal_status_e
xge_hal_write_eeprom(xge_hal_device_h devh, int off, u32 data, int cnt);

xge_hal_status_e
xge_hal_register_test(xge_hal_device_h devh, u64 *data);

xge_hal_status_e
xge_hal_eeprom_test(xge_hal_device_h devh, u64 *data);

xge_hal_status_e
xge_hal_bist_test(xge_hal_device_h devh, u64 *data);

xge_hal_status_e
xge_hal_link_test(xge_hal_device_h devh, u64 *data);

int
xge_hal_setpause_data(xge_hal_device_h devh, int tx, int rx);

void
xge_hal_getpause_data(xge_hal_device_h devh, int *tx, int *rx);

void
__hal_updt_stats_xpak(xge_hal_device_t *hldev);

void
__hal_chk_xpak_counter(xge_hal_device_t *hldev, int type, u32 value);

#ifdef XGE_TRACE_INTO_CIRCULAR_ARR
xge_hal_status_e
xge_hal_mgmt_trace_read(char *buffer, unsigned buf_size, unsigned *offset,
	    unsigned *read_length);
#endif

void
xge_hal_restore_link_led(xge_hal_device_h devh);


void
xge_hal_flick_link_led(xge_hal_device_h devh);

/*
 * Some set of Xena3 Cards were known to have some link LED
 * Problems. This macro identifies if the card is among them
 * given its Sub system ID.
 */
#define CARDS_WITH_FAULTY_LINK_INDICATORS(subid) \
	    ((((subid >= 0x600B) && (subid <= 0x600D)) || \
	     ((subid >= 0x640B) && (subid <= 0x640D))) ? 1 : 0)
#define CHECKBIT(value, nbit) (value & (1 << nbit))

#ifdef XGE_HAL_USE_MGMT_AUX
#include <dev/nxge/include/xgehal-mgmtaux.h>
#endif

__EXTERN_END_DECLS

#endif /* XGE_HAL_MGMT_H */
