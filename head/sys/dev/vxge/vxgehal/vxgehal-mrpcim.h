/*-
 * Copyright(c) 2002-2011 Exar Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification are permitted provided the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 *    3. Neither the name of the Exar Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*$FreeBSD$*/

#ifndef	VXGE_HAL_MRPCIM_H
#define	VXGE_HAL_MRPCIM_H

__EXTERN_BEGIN_DECLS

/*
 * __hal_mrpcim_t
 *
 * HAL mrpcim object. Represents privileged mode device.
 */
typedef struct __hal_mrpcim_t {
	u32	mdio_phy_prtad0;
	u32	mdio_phy_prtad1;
	u32	mdio_dte_prtad0;
	u32	mdio_dte_prtad1;
	vxge_hal_vpd_data_t vpd_data;
	__hal_blockpool_entry_t *mrpcim_stats_block;
	vxge_hal_mrpcim_stats_hw_info_t *mrpcim_stats;
	vxge_hal_mrpcim_stats_hw_info_t mrpcim_stats_sav;
	vxge_hal_mrpcim_xpak_stats_t xpak_stats[VXGE_HAL_MAC_MAX_WIRE_PORTS];
} __hal_mrpcim_t;

#define	VXGE_HAL_MRPCIM_STATS_PIO_READ(loc, offset) {			\
	status = vxge_hal_mrpcim_stats_access(devh,			\
				VXGE_HAL_STATS_OP_READ,			\
				loc,					\
				offset,					\
				&val64);				\
									\
	if (status != VXGE_HAL_OK) {					\
		vxge_hal_trace_log_stats("<== %s:%s:%d Result = %d",	\
				__FILE__, __func__, __LINE__, status);	\
		return (status);					\
	}								\
}

#define	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(reg)				\
	vxge_os_pio_mem_write64(					\
		hldev->header.pdev,					\
		hldev->header.regh0,					\
		VXGE_HAL_INTR_MASK_ALL,					\
		(reg));

#define	VXGE_HAL_MRPCIM_ERROR_REG_MASK(reg)				\
	vxge_os_pio_mem_write64(					\
		hldev->header.pdev,					\
		hldev->header.regh0,					\
		VXGE_HAL_INTR_MASK_ALL,					\
		(reg));

#define	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(mask, reg)			\
	vxge_os_pio_mem_write64(					\
		hldev->header.pdev,					\
		hldev->header.regh0,					\
		~mask,							\
		(reg));

vxge_hal_status_e
__hal_mrpcim_mdio_access(
    vxge_hal_device_h devh,
    u32 port,
    u32 operation,
    u32 device,
    u16 addr,
    u16 *data);

vxge_hal_status_e
__hal_mrpcim_rts_table_access(
    vxge_hal_device_h devh,
    u32 action,
    u32 rts_table,
    u32 offset,
    u64 *data1,
    u64 *data2,
    u64 *vpath_vector);

vxge_hal_status_e
__hal_mrpcim_initialize(__hal_device_t *hldev);

vxge_hal_status_e
__hal_mrpcim_terminate(__hal_device_t *hldev);

void
__hal_mrpcim_get_vpd_data(__hal_device_t *hldev);

void
__hal_mrpcim_xpak_counter_check(__hal_device_t *hldev,
    u32 port, u32 type, u32 value);

vxge_hal_status_e
__hal_mrpcim_stats_get(
    __hal_device_t *hldev,
    vxge_hal_mrpcim_stats_hw_info_t *mrpcim_stats);

vxge_hal_status_e
__hal_mrpcim_mac_configure(__hal_device_t *hldev);

vxge_hal_status_e
__hal_mrpcim_lag_configure(__hal_device_t *hldev);

__EXTERN_END_DECLS

#endif	/* VXGE_HAL_MRPCIM_H */
