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

#ifndef	VXGE_HAL_MGMTAUX_H
#define	VXGE_HAL_MGMTAUX_H

__EXTERN_BEGIN_DECLS

/*
 * vxge_hal_aux_about_read - Retrieve and format about info.
 * @devh: HAL device handle.
 * @bufsize: Buffer size.
 * @retbuf: Buffer pointer.
 * @retsize: Size of the result. Cannot be greater than @bufsize.
 *
 * Retrieve about info (using vxge_hal_mgmt_about()) and sprintf it
 * into the provided @retbuf.
 *
 * Returns: VXGE_HAL_OK - success.
 * VXGE_HAL_ERR_INVALID_DEVICE - Device is not valid.
 * VXGE_HAL_ERR_VERSION_CONFLICT - Version it not matching.
 * VXGE_HAL_FAIL - Failed to retrieve the information.
 *
 * See also: vxge_hal_mgmt_about(), vxge_hal_aux_device_dump().
 */
vxge_hal_status_e vxge_hal_aux_about_read(vxge_hal_device_h devh, int bufsize,
    char *retbuf, int *retsize);

/*
 * vxge_hal_aux_driver_config_read - Read Driver configuration.
 * @bufsize: Buffer size.
 * @retbuf: Buffer pointer.
 * @retsize: Size of the result. Cannot be greater than @bufsize.
 *
 * Read driver configuration,
 *
 * Returns: VXGE_HAL_OK - success.
 * VXGE_HAL_ERR_VERSION_CONFLICT - Version it not matching.
 *
 * See also: vxge_hal_aux_device_config_read().
 */
vxge_hal_status_e
vxge_hal_aux_driver_config_read(int bufsize, char *retbuf, int *retsize);

/*
 * vxge_hal_aux_pci_config_read - Retrieve and format PCI Configuration
 * info.
 * @devh: HAL device handle.
 * @bufsize: Buffer size.
 * @retbuf: Buffer pointer.
 * @retsize: Size of the result. Cannot be greater than @bufsize.
 *
 * Retrieve about info (using vxge_hal_mgmt_pci_config()) and sprintf it
 * into the provided @retbuf.
 *
 * Returns: VXGE_HAL_OK - success.
 * VXGE_HAL_ERR_INVALID_DEVICE - Device is not valid.
 * VXGE_HAL_ERR_VERSION_CONFLICT - Version it not matching.
 *
 * See also: vxge_hal_mgmt_pci_config(), vxge_hal_aux_device_dump().
 */
vxge_hal_status_e
vxge_hal_aux_pci_config_read(vxge_hal_device_h devh,
    int bufsize,
    char *retbuf,
    int *retsize);

/*
 * vxge_hal_aux_device_config_read - Read device configuration.
 * @devh: HAL device handle.
 * @bufsize: Buffer size.
 * @retbuf: Buffer pointer.
 * @retsize: Size of the result. Cannot be greater than @bufsize.
 *
 * Read device configuration,
 *
 * Returns: VXGE_HAL_OK - success.
 * VXGE_HAL_ERR_INVALID_DEVICE - Device is not valid.
 * VXGE_HAL_ERR_VERSION_CONFLICT - Version it not matching.
 *
 * See also: vxge_hal_aux_driver_config_read().
 */
vxge_hal_status_e
vxge_hal_aux_device_config_read(vxge_hal_device_h devh,
    int bufsize, char *retbuf, int *retsize);

/*
 * vxge_hal_aux_bar0_read - Read and format X3100 BAR0 register.
 * @devh: HAL device handle.
 * @offset: Register offset in the BAR0 space.
 * @bufsize: Buffer size.
 * @retbuf: Buffer pointer.
 * @retsize: Size of the result. Cannot be greater than @bufsize.
 *
 * Read X3100 register from BAR0 space. The result is formatted as an
 * ascii string.
 *
 * Returns: VXGE_HAL_OK - success.
 * VXGE_HAL_ERR_OUT_OF_SPACE - Buffer size is very small.
 * VXGE_HAL_ERR_INVALID_DEVICE - Device is not valid.
 * VXGE_HAL_ERR_INVALID_OFFSET - Register offset in the BAR space is not
 * valid.
 *
 * See also: vxge_hal_mgmt_reg_read().
 */
vxge_hal_status_e
vxge_hal_aux_bar0_read(vxge_hal_device_h devh,
    unsigned int offset, int bufsize, char *retbuf,
    int *retsize);

/*
 * vxge_hal_aux_bar1_read - Read and format X3100 BAR1 register.
 * @devh: HAL device handle.
 * @offset: Register offset in the BAR1 space.
 * @bufsize: Buffer size.
 * @retbuf: Buffer pointer.
 * @retsize: Size of the result. Cannot be greater than @bufsize.
 *
 * Read X3100 register from BAR1 space. The result is formatted as ascii string
 * Returns: VXGE_HAL_OK - success.
 * VXGE_HAL_ERR_OUT_OF_SPACE - Buffer size is very small.
 * VXGE_HAL_ERR_INVALID_DEVICE - Device is not valid.
 * VXGE_HAL_ERR_INVALID_OFFSET - Register offset in the BAR space is not
 * valid.
 *
 */
vxge_hal_status_e
vxge_hal_aux_bar1_read(vxge_hal_device_h devh,
    unsigned int offset, int bufsize, char *retbuf,
    int *retsize);

/*
 * vxge_hal_aux_bar0_write - Write BAR0 register.
 * @devh: HAL device handle.
 * @offset: Register offset in the BAR0 space.
 * @value: Regsister value (to write).
 *
 * Write BAR0 register.
 *
 * Returns: VXGE_HAL_OK - success.
 * VXGE_HAL_ERR_INVALID_DEVICE - Device is not valid.
 * VXGE_HAL_ERR_INVALID_OFFSET - Register offset in the BAR space is not
 * valid.
 *
 * See also: vxge_hal_mgmt_reg_write().
 */
vxge_hal_status_e
vxge_hal_aux_bar0_write(vxge_hal_device_h devh,
    unsigned int offset, u64 value);

/*
 * vxge_hal_aux_stats_vpath_hw_read - Read vpath hardware statistics.
 * @vpath_handle: HAL Vpath handle.
 * @bufsize: Buffer size.
 * @retbuf: Buffer pointer.
 * @retsize: Size of the result. Cannot be greater than @bufsize.
 *
 * Read vpath hardware statistics. This is a subset of stats counters
 * from vxge_hal_vpath_stats_hw_info_t {}.
 *
 */
vxge_hal_status_e
vxge_hal_aux_stats_vpath_hw_read(
    vxge_hal_vpath_h vpath_handle,
    int bufsize,
    char *retbuf,
    int *retsize);

/*
 * vxge_hal_aux_stats_device_hw_read - Read device hardware statistics.
 * @devh: HAL device handle.
 * @bufsize: Buffer size.
 * @retbuf: Buffer pointer.
 * @retsize: Size of the result. Cannot be greater than @bufsize.
 *
 * Read device hardware statistics. This is a subset of stats counters
 * from vxge_hal_device_stats_hw_info_t {}.
 *
 */
vxge_hal_status_e
vxge_hal_aux_stats_device_hw_read(vxge_hal_device_h devh,
    int bufsize, char *retbuf, int *retsize);

/*
 * vxge_hal_aux_stats_vpath_sw_fifo_read - Read vpath fifo software statistics.
 * @vpath_handle: HAL Vpath handle.
 * @bufsize: Buffer size.
 * @retbuf: Buffer pointer.
 * @retsize: Size of the result. Cannot be greater than @bufsize.
 *
 * Read vpath fifo software statistics. This is a subset of stats counters
 * from vxge_hal_vpath_stats_sw_fifo_info_t {}.
 *
 */
vxge_hal_status_e
vxge_hal_aux_stats_vpath_sw_fifo_read(
    vxge_hal_vpath_h vpath_handle,
    int bufsize,
    char *retbuf,
    int *retsize);

/*
 * vxge_hal_aux_stats_vpath_sw_ring_read - Read vpath ring software statistics.
 * @vpath_handle: HAL Vpath handle.
 * @bufsize: Buffer size.
 * @retbuf: Buffer pointer.
 * @retsize: Size of the result. Cannot be greater than @bufsize.
 *
 * Read vpath ring software statistics. This is a subset of stats counters
 * from vxge_hal_vpath_stats_sw_ring_info_t {}.
 *
 */
vxge_hal_status_e
vxge_hal_aux_stats_vpath_sw_ring_read(
    vxge_hal_vpath_h vpath_handle,
    int bufsize,
    char *retbuf,
    int *retsize);


/*
 * vxge_hal_aux_stats_vpath_sw_err_read - Read vpath err software statistics.
 * @vpath_handle: HAL Vpath handle.
 * @bufsize: Buffer size.
 * @retbuf: Buffer pointer.
 * @retsize: Size of the result. Cannot be greater than @bufsize.
 *
 * Read vpath err software statistics. This is a subset of stats counters
 * from vxge_hal_vpath_stats_sw_err_info_t {}.
 *
 */
vxge_hal_status_e
vxge_hal_aux_stats_vpath_sw_err_read(
    vxge_hal_vpath_h vpath_handle,
    int bufsize,
    char *retbuf,
    int *retsize);

/*
 * vxge_hal_aux_stats_vpath_sw_read - Read vpath soft statistics.
 * @vpath_handle: HAL Vpath handle.
 * @bufsize: Buffer size.
 * @retbuf: Buffer pointer.
 * @retsize: Size of the result. Cannot be greater than @bufsize.
 *
 * Read device hardware statistics. This is a subset of stats counters
 * from vxge_hal_vpath_stats_sw_info_t {}.
 *
 */
vxge_hal_status_e
vxge_hal_aux_stats_vpath_sw_read(
    vxge_hal_vpath_h vpath_handle,
    int bufsize,
    char *retbuf,
    int *retsize);

/*
 * vxge_hal_aux_stats_device_sw_read - Read device software statistics.
 * @devh: HAL device handle.
 * @bufsize: Buffer size.
 * @retbuf: Buffer pointer.
 * @retsize: Size of the result. Cannot be greater than @bufsize.
 *
 * Read device software statistics. This is a subset of stats counters
 * from vxge_hal_device_stats_sw_info_t {}.
 *
 */
vxge_hal_status_e
vxge_hal_aux_stats_device_sw_read(vxge_hal_device_h devh,
    int bufsize, char *retbuf, int *retsize);

/*
 * vxge_hal_aux_stats_device_sw_err_read - Read device software error statistics
 * @devh: HAL device handle.
 * @bufsize: Buffer size.
 * @retbuf: Buffer pointer.
 * @retsize: Size of the result. Cannot be greater than @bufsize.
 *
 * Read device software error statistics. This is a subset of stats counters
 * from vxge_hal_device_stats_sw_info_t {}.
 *
 */
vxge_hal_status_e
vxge_hal_aux_stats_device_sw_err_read(vxge_hal_device_h devh,
    int bufsize, char *retbuf, int *retsize);

/*
 * vxge_hal_aux_stats_device_read - Read device statistics.
 * @devh: HAL device handle.
 * @bufsize: Buffer size.
 * @retbuf: Buffer pointer.
 * @retsize: Size of the result. Cannot be greater than @bufsize.
 *
 * Read device statistics. This is a subset of stats counters
 * from vxge_hal_device_stats_t {}.
 *
 */
vxge_hal_status_e
vxge_hal_aux_stats_device_read(vxge_hal_device_h devh,
    int bufsize, char *retbuf, int *retsize);

/*
 * vxge_hal_aux_stats_xpak_read - Read device xpak statistics.
 * @devh: HAL device handle.
 * @bufsize: Buffer size.
 * @retbuf: Buffer pointer.
 * @retsize: Size of the result. Cannot be greater than @bufsize.
 *
 * Read device xpak statistics. This is valid for function 0 device only
 *
 */
vxge_hal_status_e
vxge_hal_aux_stats_xpak_read(vxge_hal_device_h devh,
    int bufsize, char *retbuf, int *retsize);

/*
 * vxge_hal_aux_stats_mrpcim_read - Read device mrpcim statistics.
 * @devh: HAL device handle.
 * @bufsize: Buffer size.
 * @retbuf: Buffer pointer.
 * @retsize: Size of the result. Cannot be greater than @bufsize.
 *
 * Read mrpcim statistics. This is valid for function 0 device only
 *
 */
vxge_hal_status_e
vxge_hal_aux_stats_mrpcim_read(vxge_hal_device_h devh,
    int bufsize, char *retbuf, int *retsize);

/*
 * vxge_hal_aux_vpath_ring_dump - Dump vpath ring.
 * @vpath_handle: Vpath handle.
 *
 * Dump vpath ring.
 */
vxge_hal_status_e
vxge_hal_aux_vpath_ring_dump(vxge_hal_vpath_h vpath_handle);

/*
 * vxge_hal_aux_vpath_fifo_dump - Dump vpath fifo.
 * @vpath_handle: Vpath handle.
 *
 * Dump vpath fifo.
 */
vxge_hal_status_e
vxge_hal_aux_vpath_fifo_dump(vxge_hal_vpath_h vpath_handle);

/*
 * vxge_hal_aux_device_dump - Dump driver "about" info and device state.
 * @devh: HAL device handle.
 *
 * Dump driver & device "about" info and device state,
 * including all BAR0 registers, hardware and software statistics, PCI
 * configuration space.
 */
vxge_hal_status_e vxge_hal_aux_device_dump(vxge_hal_device_h devh);

__EXTERN_END_DECLS

#endif	/* VXGE_HAL_MGMTAUX_H */
