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

#include "vxge_log.h"

static FILE *fdAll;

/*
 * vxge_print_registers
 * Prints/logs Register values
 * @registers Register values
 */
void
vxge_print_registers(void *registers)
{
	int i = 0, j = 0;
	u64 noffset, nRegValue = 0;
	char szName[64];

	fdAll = fopen("vxge_regs.log", "w+");
	if (!fdAll)
		return;

	VXGE_PRINT_REG_NAME(fdAll, "Registers : COMMON");
	VXGE_PRINT_HEADER_REGS(fdAll);

	for (i = 0; i < VXGE_HAL_MGMT_REG_COUNT_COMMON; i++) {
		if (reginfo_registers[i].size == 1)
			strlcpy(szName, reginfo_registers[i].name,
			    sizeof(szName));

		for (j = 0; j < reginfo_registers[i].size; j++) {
			noffset = reginfo_registers[i].offset + (0x8 * j);

			if (reginfo_registers[i].size > 1)
				snprintf(szName, sizeof(szName),
				    reginfo_registers[i].name, j);

			nRegValue = *((u64 *) ((unsigned char *) registers +
			    noffset));

			VXGE_PRINT_REGS(fdAll, (const char *) szName, noffset,
			    nRegValue);
		}
	}

	VXGE_PRINT_LINE(fdAll);
	fclose(fdAll);
}

/*
 * vxge_print_registers_legacy
 * Prints/logs legacy Register values
 * @registers Register values
 */
void
vxge_print_registers_legacy(void *registers)
{
	int i = 0, j = 0;
	u64 noffset, nRegValue = 0;
	char szName[64];

	fdAll = fopen("vxge_regs.log", "a+");
	if (!fdAll)
		return;

	VXGE_PRINT_REG_NAME(fdAll, "Registers : LEGACY");
	VXGE_PRINT_HEADER_REGS(fdAll);

	for (i = 0; i < VXGE_HAL_MGMT_REG_COUNT_LEGACY; i++) {
		if (reginfo_legacy[i].size == 1)
			strlcpy(szName, reginfo_legacy[i].name, sizeof(szName));

		for (j = 0; j < reginfo_legacy[i].size; j++) {
			noffset = reginfo_legacy[i].offset + (0x8 * j);

			if (reginfo_legacy[i].size > 1)
				snprintf(szName, sizeof(szName),
				    reginfo_legacy[i].name, j);

			nRegValue = *((u64 *) ((unsigned char *) registers +
			    noffset));

			VXGE_PRINT_REGS(fdAll, (const char *) szName, noffset,
			    nRegValue);
		}
	}

	VXGE_PRINT_LINE(fdAll);
	fclose(fdAll);
}

/*
 * vxge_print_registers_toc
 * Prints/logs toc Register values
 * @registers Register values
 */
void
vxge_print_registers_toc(void *registers)
{
	int i = 0, j = 0;
	u64 noffset, nRegValue = 0;
	char szName[64];

	fdAll = fopen("vxge_regs.log", "a+");
	if (!fdAll)
		return;

	VXGE_PRINT_REG_NAME(fdAll, "Registers : TOC");
	VXGE_PRINT_HEADER_REGS(fdAll);

	for (i = 0; i < VXGE_HAL_MGMT_REG_COUNT_TOC; i++) {
		if (reginfo_toc[i].size == 1)
			strlcpy(szName, reginfo_toc[i].name, sizeof(szName));

		for (j = 0; j < reginfo_toc[i].size; j++) {
			noffset = reginfo_toc[i].offset + (0x8 * j);

			if (reginfo_toc[i].size > 1)
				snprintf(szName, sizeof(szName),
				    reginfo_toc[i].name, j);

			nRegValue = *((u64 *) ((unsigned char *) registers +
			    noffset));

			VXGE_PRINT_REGS(fdAll, (const char *) szName, noffset,
			    nRegValue);
		}
	}

	VXGE_PRINT_LINE(fdAll);
	fclose(fdAll);
}

/*
 * vxge_print_registers_pcicfgmgmt
 * Prints/logs pcicfgmgmt Register values
 * @registers Register values
 */
void
vxge_print_registers_pcicfgmgmt(void *registers)
{
	int i = 0, j = 0;
	u64 noffset, nRegValue;
	char szName[64];

	fdAll = fopen("vxge_regs.log", "a+");
	if (!fdAll)
		return;

	VXGE_PRINT_REG_NAME(fdAll, "Registers : PCICFGMGMT");
	VXGE_PRINT_HEADER_REGS(fdAll);

	for (i = 0; i < VXGE_HAL_MGMT_REG_COUNT_PCICFGMGMT; i++) {
		if (reginfo_pcicfgmgmt[i].size == 1)
			strlcpy(szName, reginfo_pcicfgmgmt[i].name,
			    sizeof(szName));

		for (j = 0; j < reginfo_pcicfgmgmt[i].size; j++) {

			noffset = reginfo_pcicfgmgmt[i].offset + (0x8 * j);

			if (reginfo_pcicfgmgmt[i].size > 1)
				snprintf(szName, sizeof(szName),
				    reginfo_pcicfgmgmt[i].name, j);

			nRegValue = *((u64 *) ((unsigned char *) registers +
			    noffset));

			VXGE_PRINT_REGS(fdAll, (const char *) szName, noffset,
			    nRegValue);
		}
	}

	VXGE_PRINT_LINE(fdAll);
	fclose(fdAll);
}

/*
 * vxge_print_registers_vpath
 * Prints/logs vpath Register values
 * @registers Register values
 */
void
vxge_print_registers_vpath(void *registers, int vpath_num)
{
	int i = 0, j = 0;
	u64 noffset, nRegValue = 0;
	char szName[64];

	fdAll = fopen("vxge_regs.log", "a+");
	if (!fdAll)
		return;

	VXGE_PRINT_REG_NAME(fdAll, "Registers : VPATH");
	VXGE_PRINT_HEADER_REGS(fdAll);

	for (i = 0; i < VXGE_HAL_MGMT_REG_COUNT_VPATH; i++) {
		if (reginfo_vpath[i].size == 1)
			snprintf(szName, sizeof(szName),
			    reginfo_vpath[i].name, vpath_num);

		for (j = 0; j < reginfo_vpath[i].size; j++) {
			noffset = reginfo_vpath[i].offset + (0x8 * j);

			if (reginfo_vpath[i].size > 1)
				snprintf(szName, sizeof(szName), reginfo_vpath[i].name, j, vpath_num);

			nRegValue = *((u64 *) ((unsigned char *) registers +
			    noffset));

			VXGE_PRINT_REGS(fdAll, (const char *) szName, noffset,
			    nRegValue);
		}
	}

	VXGE_PRINT_LINE(fdAll);
	fclose(fdAll);
}

/*
 * vxge_print_registers_vpmgmt
 * Prints/logs vpmgmt Register values
 * @registers Register values
 */
void
vxge_print_registers_vpmgmt(void *registers)
{
	int i = 0, j = 0;
	u64 noffset, nRegValue = 0;
	char szName[64];

	fdAll = fopen("vxge_regs.log", "a+");
	if (!fdAll)
		return;

	VXGE_PRINT_REG_NAME(fdAll, "Registers : VPMGMT");
	VXGE_PRINT_HEADER_REGS(fdAll);

	for (i = 0; i < VXGE_HAL_MGMT_REG_COUNT_VPMGMT; i++) {

		if (reginfo_vpmgmt[i].size == 1)
			strlcpy(szName, reginfo_vpmgmt[i].name, sizeof(szName));

		for (j = 0; j < reginfo_vpmgmt[i].size; j++) {

			noffset = reginfo_vpmgmt[i].offset + (0x8 * j);

			if (reginfo_vpmgmt[i].size > 1)
				snprintf(szName, sizeof(szName),
				    reginfo_vpmgmt[i].name, j);

			nRegValue = *((u64 *) ((unsigned char *) registers +
			    noffset));

			VXGE_PRINT_REGS(fdAll, (const char *) szName, noffset,
			    nRegValue);
		}
	}

	VXGE_PRINT_LINE(fdAll);
	fclose(fdAll);
}

/*
 * vxge_print_registers_mrpcim
 * Prints/logs mrpcim Register values
 * @registers Register values
 */
void
vxge_print_registers_mrpcim(void *registers)
{
	int i = 0, j = 0;
	u64 noffset, nRegValue = 0;
	char szName[64];

	fdAll = fopen("vxge_regs.log", "a+");
	if (!fdAll)
		return;

	VXGE_PRINT_REG_NAME(fdAll, "Registers : MRPCIM");
	VXGE_PRINT_HEADER_REGS(fdAll);

	for (i = 0; i < VXGE_HAL_MGMT_REG_COUNT_MRPCIM; i++) {

		if (reginfo_mrpcim[i].size == 1)
			strlcpy(szName, reginfo_mrpcim[i].name, sizeof(szName));

		for (j = 0; j < reginfo_mrpcim[i].size; j++) {

			noffset = reginfo_mrpcim[i].offset + (0x8 * j);

			if (reginfo_mrpcim[i].size > 1)
				snprintf(szName, sizeof(szName),
				    reginfo_mrpcim[i].name, j);

			nRegValue = *((u64 *) ((unsigned char *) registers +
			    noffset));

			VXGE_PRINT_REGS(fdAll, (const char *) szName, noffset,
			    nRegValue);
		}
	}

	VXGE_PRINT_LINE(fdAll);
	fclose(fdAll);
}

/*
 * vxge_print_registers_srpcim
 * Prints/logs srpcim Register values
 * @registers Register values
 */
void
vxge_print_registers_srpcim(void *registers)
{
	int i = 0, j = 0;
	u64 noffset, nRegValue = 0;
	char szName[64];

	fdAll = fopen("vxge_regs.log", "a+");
	if (!fdAll)
		return;

	VXGE_PRINT_REG_NAME(fdAll, "Registers : SRPCIM");
	VXGE_PRINT_HEADER_REGS(fdAll);

	for (i = 0; i < VXGE_HAL_MGMT_REG_COUNT_SRPCIM; i++) {

		if (reginfo_srpcim[i].size == 1)
			strlcpy(szName, reginfo_srpcim[i].name, sizeof(szName));

		for (j = 0; j < reginfo_srpcim[i].size; j++) {

			noffset = reginfo_srpcim[i].offset + (0x8 * j);

			if (reginfo_srpcim[i].size > 1)
				snprintf(szName, sizeof(szName),
				    reginfo_srpcim[i].name, j);

			nRegValue = *((u64 *) ((unsigned char *) registers +
			    noffset));

			VXGE_PRINT_REGS(fdAll, (const char *) szName, noffset,
			    nRegValue);
		}
	}

	VXGE_PRINT_LINE(fdAll);
	fclose(fdAll);
}

/*
 * vxge_print_stats_drv
 * Prints/logs Driver Statistics
 * @driver_stats Driver Statistics
 */
void
vxge_print_stats_drv(void *driver_stats, int vpath_num)
{
	int i, j;
	u32 no_of_vpath;

	no_of_vpath = vxge_get_num_vpath();
	fdAll = fopen("vxge_drv_stats.log", "w+");
	if (!fdAll)
		return;

	for (i = 0; i < no_of_vpath; i++) {

		if (vpath_num != -1) {
			if (vpath_num != i) {
				driver_stats = driver_stats +
				    (VXGE_HAL_MGMT_STATS_COUNT_DRIVER * sizeof(u64));
				continue;
			}
		}

		VXGE_PRINT_LINE(fdAll);
		VXGE_PRINT(fdAll, " VPath # %d ", i);
		VXGE_PRINT_LINE(fdAll);

		for (j = 0; j < VXGE_HAL_MGMT_STATS_COUNT_DRIVER; j++) {

			driverInfo[j].value =
			    *((u64 *) ((unsigned char *) driver_stats +
			    (j * (sizeof(u64)))));

			VXGE_PRINT_STATS(fdAll, (const char *)
			    driverInfo[j].name, driverInfo[j].value);
		}
		driver_stats = driver_stats + (j * sizeof(u64));
	}

	VXGE_PRINT_LINE(fdAll);
	fclose(fdAll);
}

/*
 * vxge_print_stats
 * Prints/logs Statistics
 * @driver_stats Driver Statistics
 */
void
vxge_print_stats(void *stats, vxge_query_device_info_e stat_type)
{
	fdAll = fopen("vxge_stats.log", "a+");
	if (!fdAll)
		return;

	switch (stat_type) {
	case VXGE_GET_MRPCIM_STATS:
		VXGE_PRINT_LINE(fdAll);
		VXGE_PRINT_REG_NAME(fdAll, "Statistics : MRPCIM");
		VXGE_PRINT_LINE(fdAll);
		break;

	case VXGE_GET_DEVICE_STATS:
		VXGE_PRINT_LINE(fdAll);
		VXGE_PRINT_REG_NAME(fdAll, "Statistics: COMMON");
		VXGE_PRINT_LINE(fdAll);
		break;
	}

	VXGE_PRINT(fdAll, "%s", stats);
	fclose(fdAll);
}

void
vxge_print_pci_config(void *info)
{
	fdAll = fopen("vxge_regs.log", "a+");
	if (!fdAll)
		return;

	VXGE_PRINT_LINE(fdAll);
	VXGE_PRINT_REG_NAME(fdAll, "PCI CONFIG SPACE");
	VXGE_PRINT_LINE(fdAll);
	VXGE_PRINT(fdAll, "%s", info);
	fclose(fdAll);
}

void
vxge_print_hw_info(void *info)
{
	u32 i;
	vxge_device_hw_info_t *dev_hw_info;
	vxge_hal_device_hw_info_t *hw_info;
	vxge_hal_device_pmd_info_t *pmd_port;

	fdAll = fopen("vxge_regs.log", "w+");
	if (!fdAll)
		return;

	dev_hw_info = (vxge_device_hw_info_t *) info;
	hw_info = &(dev_hw_info->hw_info);
	pmd_port = &(hw_info->pmd_port0);

	VXGE_PRINT_LINE(fdAll);
	VXGE_PRINT_REG_NAME(fdAll, "HARDWARE INFO");
	VXGE_PRINT_LINE(fdAll);

	VXGE_PRINT(fdAll, "Description \t\t: %s",
	    hw_info->product_description);

	VXGE_PRINT(fdAll, "Serial Number \t\t: %s", hw_info->serial_number);
	VXGE_PRINT(fdAll, "Part Number \t\t: %s", hw_info->part_number);

	VXGE_PRINT(fdAll, "Firmware Version \t: %s",
	    hw_info->fw_version.version);

	VXGE_PRINT(fdAll, "Firmware Date \t\t: %s", hw_info->fw_date.date);

	VXGE_PRINT(fdAll, "Function Mode \t\t: %s",
	    vxge_func_mode[hw_info->function_mode]);

	for (i = 0; i < hw_info->ports; i++) {

		vxge_null_terminate(pmd_port->vendor,
		    sizeof(pmd_port->vendor));

		if (strlen(pmd_port->vendor) == 0) {
			VXGE_PRINT(fdAll,
			    "PMD Port %d \t\t: vendor=??, sn=??, pn=??", i);

			pmd_port = &(hw_info->pmd_port1);
			continue;
		}

		vxge_null_terminate(pmd_port->ser_num,
		    sizeof(pmd_port->ser_num));

		vxge_null_terminate(pmd_port->part_num,
		    sizeof(pmd_port->part_num));

		VXGE_PRINT(fdAll,
		    "PMD Port %d \t\t: vendor=%s, sn=%s, pn=%s", i,
		    pmd_port->vendor, pmd_port->ser_num,
		    pmd_port->part_num);

		pmd_port = &(hw_info->pmd_port1);
	}

	if (hw_info->ports > 1) {

		VXGE_PRINT(fdAll, "Port mode \t\t: %s",
		    vxge_port_mode[dev_hw_info->port_mode]);

		if (dev_hw_info->port_mode != VXGE_HAL_DP_NP_MODE_SINGLE_PORT) {
			VXGE_PRINT(fdAll, "Port failure \t\t: %s",
			    vxge_port_failure[dev_hw_info->port_failure]);
		}
	}

	VXGE_PRINT_LINE(fdAll);
	fclose(fdAll);
}

void
vxge_print_bw_priority(void *info)
{
	u32 i;
	u64 func_id;
	vxge_bw_info_t *buffer;

	fdAll = fopen("vxge_stats.log", "a+");
	if (!fdAll)
		return;

	buffer = (vxge_bw_info_t *) info;
	func_id = buffer->func_id;

	VXGE_PRINT_LINE(fdAll);

	VXGE_PRINT(fdAll,
	    "Function : %02lld Bandwidth : %05d\tPriority : %d",
	    func_id, (buffer->bandwidth ?
	    buffer->bandwidth : VXGE_MAX_BANDWIDTH),
	    buffer->priority);

	VXGE_PRINT_LINE(fdAll);
	fclose(fdAll);
}

void
vxge_print_port_mode(void *info)
{
	vxge_port_info_t *buffer;

	fdAll = fopen("vxge_stats.log", "a+");
	if (!fdAll)
		return;

	buffer = (vxge_port_info_t *) info;

	VXGE_PRINT_LINE(fdAll);

	VXGE_PRINT(fdAll,
	    "Port Mode: %s\tPort Failure: %s",
	    vxge_port_mode[buffer->port_mode],
	    vxge_port_failure[buffer->port_failure]);

	VXGE_PRINT_LINE(fdAll);
	fclose(fdAll);
}
