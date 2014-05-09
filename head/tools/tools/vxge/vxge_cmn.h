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

#ifndef	_VXGE_CMN_H_
#define	_VXGE_CMN_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#if BYTE_ORDER == BIG_ENDIAN
#define	VXGE_OS_HOST_BIG_ENDIAN
#else
#define	VXGE_OS_HOST_LITTLE_ENDIAN
#endif

#if defined(VXGE_OS_HOST_BIG_ENDIAN)

#define	GET_OFFSET_STATS(index)		statsInfo[(index)].be_offset
#define	GET_OFFSET_PCICONF(index)	pciconfInfo[(index)].be_offset

#else

#define	GET_OFFSET_STATS(index)		statsInfo[(index)].le_offset
#define	GET_OFFSET_PCICONF(index)	pciconfInfo[(index)].le_offset

#endif

#define	vxge_mem_free(x)	\
	if (NULL != x) { free(x); x = NULL; }

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef unsigned long long u64;
typedef u_long ulong_t;

typedef enum _vxge_query_device_info_e {

	VXGE_GET_PCI_CONF = 100,
	VXGE_GET_MRPCIM_STATS = 101,
	VXGE_GET_DEVICE_STATS = 102,
	VXGE_GET_DEVICE_HWINFO = 103,
	VXGE_GET_DRIVER_STATS = 104,
	VXGE_GET_INTR_STATS = 105,
	VXGE_GET_VERSION = 106,
	VXGE_GET_TCODE = 107,
	VXGE_GET_VPATH_COUNT = 108,
	VXGE_GET_BANDWIDTH = 109,
	VXGE_SET_BANDWIDTH = 110,
	VXGE_GET_PORT_MODE = 111,
	VXGE_SET_PORT_MODE = 112

} vxge_query_device_info_e;

/* Register type enumaration */
typedef enum vxge_hal_mgmt_reg_type_e {

	vxge_hal_mgmt_reg_type_legacy = 0,
	vxge_hal_mgmt_reg_type_toc = 1,
	vxge_hal_mgmt_reg_type_common = 2,
	vxge_hal_mgmt_reg_type_memrepair = 3,
	vxge_hal_mgmt_reg_type_pcicfgmgmt = 4,
	vxge_hal_mgmt_reg_type_mrpcim = 5,
	vxge_hal_mgmt_reg_type_srpcim = 6,
	vxge_hal_mgmt_reg_type_vpmgmt = 7,
	vxge_hal_mgmt_reg_type_vpath = 8

} vxge_hal_mgmt_reg_type_e;

typedef enum vxge_hal_xmac_nwif_dp_mode {

	VXGE_HAL_DP_NP_MODE_DEFAULT,
	VXGE_HAL_DP_NP_MODE_LINK_AGGR,
	VXGE_HAL_DP_NP_MODE_ACTIVE_PASSIVE,
	VXGE_HAL_DP_NP_MODE_SINGLE_PORT,
	VXGE_HAL_DP_NP_MODE_DUAL_PORT,
	VXGE_HAL_DP_NP_MODE_DISABLE_PORT_MGMT

} vxge_hal_xmac_nwif_dp_mode;

typedef enum vxge_hal_xmac_nwif_behavior_on_failure {

	VXGE_HAL_XMAC_NWIF_OnFailure_NoMove,
	VXGE_HAL_XMAC_NWIF_OnFailure_OtherPort,
	VXGE_HAL_XMAC_NWIF_OnFailure_OtherPortBackOnRestore

} vxge_hal_xmac_nwif_behavior_on_failure;

#define	VXGE_HAL_MGMT_REG_COUNT_LEGACY		7
#define	VXGE_HAL_MGMT_REG_COUNT_TOC		11
#define	VXGE_HAL_MGMT_REG_COUNT_COMMON		65
#define	VXGE_HAL_MGMT_REG_COUNT_PCICFGMGMT	3
#define	VXGE_HAL_MGMT_REG_COUNT_MRPCIM		1370
#define	VXGE_HAL_MGMT_REG_COUNT_SRPCIM		48
#define	VXGE_HAL_MGMT_REG_COUNT_VPMGMT		29
#define	VXGE_HAL_MGMT_REG_COUNT_VPATH		139
#define	VXGE_HAL_MGMT_STATS_COUNT_DRIVER	17
#define	VXGE_HAL_MGMT_STATS_COUNT		160
#define	VXGE_HAL_MGMT_STATS_COUNT_SW		54
#define	VXGE_HAL_MGMT_STATS_COUNT_EXTENDED	56
#define	VXGE_MAX_BANDWIDTH			10000

#define	VXGE_HAL_MAX_VIRTUAL_PATHS		17
#define	ETH_LENGTH_OF_ADDRESS			6

typedef char macaddr[ETH_LENGTH_OF_ADDRESS];

#define	VXGE_PRINT(fd, fmt...) {	\
	fprintf(fd, fmt);		\
	fprintf(fd, "\n");		\
	printf(fmt);			\
	printf("\n");			\
}

/* Read	& Write	Register */
typedef struct _vxge_register_info_t {

	u64	value;
	u64	offset;
	char	option[2];

} vxge_register_info_t;

/* Register Dump */
typedef struct _vxge_pci_bar0_t {
	char	name[64];
	u64	offset;
	u32	size;

} vxge_pci_bar0_t;

typedef struct _vxge_stats_driver_info_t {

	char	name[32];
	u64	value;

} vxge_stats_driver_info_t;

typedef struct _vxge_hal_device_pmd_info_t {

	u32	type;
	u32	unused;
	char	vendor[24];
	char	part_num[24];
	char	ser_num[24];

} vxge_hal_device_pmd_info_t;

typedef struct _vxge_hal_device_version_t {

	u32	major;
	u32	minor;
	u32	build;
	char	version[32];

} vxge_hal_device_version_t;

typedef struct _vxge_hal_device_date_t {

	u32	day;
	u32	month;
	u32	year;
	char	date[16];

} vxge_hal_device_date_t;

typedef struct _vxge_hal_device_hw_info_t {

	u32	host_type;
	u64	function_mode;
	u32	func_id;
	u64	vpath_mask;

	vxge_hal_device_version_t fw_version;
	vxge_hal_device_date_t fw_date;
	vxge_hal_device_version_t flash_version;
	vxge_hal_device_date_t flash_date;

	char	serial_number[24];
	char	part_number[24];
	char	product_description[72];
	u32	unused;
	u32	ports;

	vxge_hal_device_pmd_info_t pmd_port0;
	vxge_hal_device_pmd_info_t pmd_port1;

	macaddr	mac_addrs[VXGE_HAL_MAX_VIRTUAL_PATHS];
	macaddr	mac_addr_masks[VXGE_HAL_MAX_VIRTUAL_PATHS];

} vxge_hal_device_hw_info_t;

typedef struct _vxge_device_hw_info_t {

	vxge_hal_device_hw_info_t hw_info;
	u32	port_mode;
	u32	port_failure;

} vxge_device_hw_info_t;

typedef struct _vxge_bw_info_t {

	char	query;
	u64	func_id;
	int	priority;
	int	bandwidth;

} vxge_bw_info_t;

typedef struct _vxge_port_info_t {

	char	query;
	int	port_mode;
	int	port_failure;

} vxge_port_info_t;

u32	vxge_get_num_vpath(void);
void	vxge_null_terminate(char *, size_t);

#endif	/* _VXGE_CMN_H_ */
