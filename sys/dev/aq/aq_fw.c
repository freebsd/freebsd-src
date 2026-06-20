/*
 * aQuantia Corporation Network Driver
 * Copyright (C) 2014-2017 aQuantia Corporation. All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   (1) Redistributions of source code must retain the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer.
 *
 *   (2) Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer in the documentation and/or other materials provided
 *   with the distribution.
 *
 *   (3) The name of the author may not be used to endorse or promote
 *   products derived from this software without specific prior
 *   written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @file aq_fw.c
 * Firmware-related functions implementation.
 * @date 2017.12.07  @author roman.agafonov@aquantia.com
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/errno.h>

#include "aq_common.h"

#include "aq_hw.h"
#include "aq_hw_llh.h"
#include "aq_hw_llh_internal.h"

#include "aq_fw.h"

#include "aq_dbg.h"


enum aq_fw_bootloader_mode
{
	boot_mode_unknown = 0,
	boot_mode_flb,
	boot_mode_rbl_flash,
	boot_mode_rbl_host_bootload,
};

#define AQ_CFG_HOST_BOOT_DISABLE 0

// Timeouts
#define RBL_TIMEOUT_MS              10000
#define MAC_FW_START_TIMEOUT_MS     10000
#define FW_LOADER_START_TIMEOUT_MS  10000

const uint32_t NO_RESET_SCRATCHPAD_ADDRESS = 0;
const uint32_t NO_RESET_SCRATCHPAD_LEN_RES = 1;
const uint32_t NO_RESET_SCRATCHPAD_RBL_STATUS = 2;
const uint32_t NO_RESET_SCRATCHPAD_RBL_STATUS_2 = 3;
const uint32_t WRITE_DATA_COMPLETE = 0x55555555;
const uint32_t WRITE_DATA_CHUNK_DONE = 0xaaaaaaaa;
const uint32_t WRITE_DATA_FAIL_WRONG_ADDRESS = 0x66666666;

const uint32_t WAIT_WRITE_TIMEOUT = 1;
const uint32_t WAIT_WRITE_TIMEOUT_COUNT = 1000;

const uint32_t RBL_STATUS_SUCCESS = 0xabba;
const uint32_t RBL_STATUS_FAILURE = 0xbad;
const uint32_t RBL_STATUS_HOST_BOOT = 0xf1a7;

const uint32_t SCRATCHPAD_FW_LOADER_STATUS = (0x40 / sizeof(uint32_t));


static int mac_soft_reset(struct aq_hw* hw, enum aq_fw_bootloader_mode* mode);
static int mac_soft_reset_flb(struct aq_hw* hw);
static int mac_soft_reset_rbl(struct aq_hw* hw, enum aq_fw_bootloader_mode* mode);
static int wait_init_mac_firmware(struct aq_hw* hw);


int
aq_fw_reset(struct aq_hw* hw)
{
	int ver = AQ_READ_REG(hw, 0x18);
	uint32_t bootExitCode = 0;
	int k;

	for (k = 0; k < 1000; ++k) {
		uint32_t flbStatus = reg_glb_daisy_chain_status1_get(hw);
		bootExitCode = AQ_READ_REG(hw, 0x388);
		if (flbStatus != 0x06000000 || bootExitCode != 0)
			break;
	}

	if (k == 1000) {
		device_printf(hw->dev, "Neither RBL nor FLB started\n");
		return (EBUSY);
	}

	hw->rbl_enabled = bootExitCode != 0;

	trace(dbg_init, "RBL enabled = %d", hw->rbl_enabled);

	/* Having FW version 0 is an indicator that cold start
	 * is in progress. This means two things:
	 * 1) Driver have to wait for FW/HW to finish boot (500ms giveup)
	 * 2) Driver may skip reset sequence and save time.
	 */
	if (hw->fast_start_enabled && !ver) {
		int err = wait_init_mac_firmware(hw);
		/* Skip reset as it just completed */
		if (!err)
			return (0);
	}

	enum aq_fw_bootloader_mode mode = boot_mode_unknown;
	int err = mac_soft_reset(hw, &mode);
	if (err != 0) {
		device_printf(hw->dev, "MAC reset failed: %d\n", err);
		return (err);
	}

	switch (mode) {
	case boot_mode_flb:
		aq_log("FLB> F/W successfully loaded from flash.");
		hw->flash_present = true;
		return wait_init_mac_firmware(hw);

	case boot_mode_rbl_flash:
		aq_log("RBL> F/W loaded from flash. Host Bootload disabled.");
		hw->flash_present = true;
		return wait_init_mac_firmware(hw);

	case boot_mode_unknown:
		device_printf(hw->dev, "F/W bootload error: unknown bootloader type\n");
		return (ENOTSUP);

	case boot_mode_rbl_host_bootload:
#if AQ_CFG_HOST_BOOT_DISABLE
		device_printf(hw->dev, "RBL> Host Bootload mode: this driver does not support Host Boot\n");
		return (ENOTSUP);
#else
		trace(dbg_init, "RBL> Host Bootload mode");
		break;
#endif // HOST_BOOT_DISABLE
	}

	/*
	 * #todo: Host Boot
	 */
	device_printf(hw->dev, "RBL> F/W Host Bootload not implemented\n");

	return (ENOTSUP);
}

int
aq_fw_ops_init(struct aq_hw* hw)
{
	if (hw->fw_version.raw == 0)
		hw->fw_version.raw = AQ_READ_REG(hw, 0x18);

	aq_log("MAC F/W version is %d.%d.%d",
	    hw->fw_version.major_version, hw->fw_version.minor_version,
	    hw->fw_version.build_number);

	if (hw->fw_version.major_version == 1) {
		trace(dbg_init, "using F/W ops v1.x");
		hw->fw_ops = &aq_fw1x_ops;
		return (0);
	} else if (hw->fw_version.major_version >= 2) {
		trace(dbg_init, "using F/W ops v2.x");
		hw->fw_ops = &aq_fw2x_ops;
		return (0);
	}

	device_printf(hw->dev, "aq_fw_ops_init(): invalid F/W version %#x\n",
	    hw->fw_version.raw);
	return (ENOTSUP);
}


static int
mac_soft_reset(struct aq_hw* hw, enum aq_fw_bootloader_mode* mode /*= nullptr*/)
{
	if (hw->rbl_enabled) {
		return mac_soft_reset_rbl(hw, mode);
	} else {
		if (mode)
			*mode = boot_mode_flb;

		return mac_soft_reset_flb(hw);
	}
}

static int
mac_soft_reset_flb(struct aq_hw* hw)
{
	int k;

	reg_global_ctl2_set(hw, 0x40e1);
	// Let Felicity hardware complete SMBUS transaction before Global
	// software reset.
	DELAY((50) * 1000);

	/*
	 * If SPI burst transaction was interrupted(before running the script),
	 * global software reset may not clear SPI interface. Clean it up
	 * manually before global reset.
	 */
	reg_glb_nvr_provisioning2_set(hw, 0xa0);
	reg_glb_nvr_interface1_set(hw, 0x9f);
	reg_glb_nvr_interface1_set(hw, 0x809f);
	DELAY((50) * 1000);

	reg_glb_standard_ctl1_set(hw, (reg_glb_standard_ctl1_get(hw) & ~glb_reg_res_dis_msk) | glb_soft_res_msk);

	// Kickstart.
	reg_global_ctl2_set(hw, 0x80e0);
	reg_mif_power_gating_enable_control_set(hw, 0);
	if (!hw->fast_start_enabled)
		reg_glb_general_provisioning9_set(hw, 1);

	/*
	 * For the case SPI burst transaction was interrupted (by MCP reset
	 * above), wait until it is completed by hardware.
	 */
	DELAY(50 * 1000);

	/* MAC Kickstart */
	if (!hw->fast_start_enabled) {
		reg_global_ctl2_set(hw, 0x180e0);

		uint32_t flb_status = 0;
		int k;
		for (k = 0; k < 1000; ++k) {
			flb_status = reg_glb_daisy_chain_status1_get(hw) & 0x10;
			if (flb_status != 0)
				break;
			DELAY(10 * 1000);
		}

		if (flb_status == 0) {
			trace_error(dbg_init,
			    "FLB> MAC kickstart failed: timed out");
			return (ETIMEDOUT);
		}

		trace(dbg_init, "FLB> MAC kickstart done, %d ms", k);
		/* FW reset */
		reg_global_ctl2_set(hw, 0x80e0);
		// Let Felicity hardware complete SMBUS transaction before
		// Global software reset.
		DELAY((50) * 1000);
	}
	reg_glb_cpu_sem_set(hw, 1, 0);

	// PHY Kickstart: #undone

	// Global software reset
	rx_rx_reg_res_dis_set(hw, 0);
	tx_tx_reg_res_dis_set(hw, 0);
	mpi_tx_reg_res_dis_set(hw, 0);
	reg_glb_standard_ctl1_set(hw, (reg_glb_standard_ctl1_get(hw) & ~glb_reg_res_dis_msk) | glb_soft_res_msk);

	bool restart_completed = false;
	for (k = 0; k < 1000; ++k) {
		restart_completed = reg_glb_fw_image_id1_get(hw) != 0;
		if (restart_completed)
			break;
		DELAY((10) * 1000);
	}

	if (!restart_completed) {
		trace_error(dbg_init, "FLB> Global Soft Reset failed");
		return (ETIMEDOUT);
	}

	trace(dbg_init, "FLB> F/W restart: %d ms", k * 10);
	return (0);
}

static int
mac_soft_reset_rbl(struct aq_hw* hw, enum aq_fw_bootloader_mode* mode)
{
	trace(dbg_init, "RBL> MAC reset STARTED!");

	reg_global_ctl2_set(hw, 0x40e1);
	reg_glb_cpu_sem_set(hw, 1, 0);
	reg_mif_power_gating_enable_control_set(hw, 0);

	// MAC FW will reload PHY FW if 1E.1000.3 was cleaned - #undone

	reg_glb_cpu_no_reset_scratchpad_set(hw, 0xDEAD,
	    NO_RESET_SCRATCHPAD_RBL_STATUS);

	// Global software reset
	rx_rx_reg_res_dis_set(hw, 0);
	tx_tx_reg_res_dis_set(hw, 0);
	mpi_tx_reg_res_dis_set(hw, 0);
	reg_glb_standard_ctl1_set(hw, (reg_glb_standard_ctl1_get(hw) & ~glb_reg_res_dis_msk) | glb_soft_res_msk);

	reg_global_ctl2_set(hw, 0x40e0);

	// Wait for RBL to finish boot process.
	uint16_t rbl_status = 0;
	for (int k = 0; k < RBL_TIMEOUT_MS; ++k) {
		rbl_status = ((uint16_t)(reg_glb_cpu_no_reset_scratchpad_get(hw, NO_RESET_SCRATCHPAD_RBL_STATUS)));
		if (rbl_status != 0 && rbl_status != 0xDEAD)
			break;

		DELAY((1) * 1000);
	}

	if (rbl_status == 0 || rbl_status == 0xDEAD) {
		trace_error(dbg_init, "RBL> RBL restart failed: timeout");
		return (EBUSY);
	}

	if (rbl_status == RBL_STATUS_SUCCESS) {
		if (mode)
			*mode = boot_mode_rbl_flash;
		trace(dbg_init, "RBL> reset complete! [Flash]");
	} else if (rbl_status == RBL_STATUS_HOST_BOOT) {
		if (mode)
			*mode = boot_mode_rbl_host_bootload;
		trace(dbg_init, "RBL> reset complete! [Host Bootload]");
	} else {
		trace_error(dbg_init, "unknown RBL status 0x%x", rbl_status);
		return (EBUSY);
	}

	return (0);
}

static int
wait_init_mac_firmware(struct aq_hw* hw)
{
	for (int i = 0; i < MAC_FW_START_TIMEOUT_MS; ++i) {
		if ((hw->fw_version.raw = AQ_READ_REG(hw, 0x18)) != 0)
			return (0);

		DELAY((1) * 1000);
	}

	trace_error(dbg_init,
	    "timeout waiting for reg 0x18. MAC f/w NOT READY");
	return (EBUSY);
}
