/*-
 * Copyright (c) 2025, Samsung Electronics Co., Ltd.
 * Written by Jaeyoon Choi
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include <sys/param.h>
#include <sys/bus.h>
#include <sys/conf.h>

#include "ufshci_private.h"
#include "ufshci_reg.h"

int
ufshci_uic_power_mode_ready(struct ufshci_controller *ctrlr)
{
	uint32_t is, hcs;
	int timeout;

	/* Wait for the IS flag to change */
	timeout = ticks + MSEC_2_TICKS(ctrlr->device_init_timeout_in_ms);

	while (1) {
		is = ufshci_mmio_read_4(ctrlr, is);
		if (UFSHCIV(UFSHCI_IS_REG_UPMS, is)) {
			ufshci_mmio_write_4(ctrlr, is,
			    UFSHCIM(UFSHCI_IS_REG_UPMS));
			break;
		}

		if (timeout - ticks < 0) {
			ufshci_printf(ctrlr,
			    "Power mode is not changed "
			    "within %d ms\n",
			    ctrlr->uic_cmd_timeout_in_ms);
			return (ENXIO);
		}

		/* TODO: Replace busy-wait with interrupt-based pause. */
		DELAY(10);
	}

	/* Check HCS power mode change request status */
	hcs = ufshci_mmio_read_4(ctrlr, hcs);
	if (UFSHCIV(UFSHCI_HCS_REG_UPMCRS, hcs) != 0x01) {
		ufshci_printf(ctrlr,
		    "Power mode change request status error: 0x%x\n",
		    UFSHCIV(UFSHCI_HCS_REG_UPMCRS, hcs));
		return (ENXIO);
	}

	return (0);
}

int
ufshci_uic_cmd_ready(struct ufshci_controller *ctrlr)
{
	uint32_t hcs;
	int timeout;

	/* Wait for the HCS flag to change */
	timeout = ticks + MSEC_2_TICKS(ctrlr->uic_cmd_timeout_in_ms);

	while (1) {
		hcs = ufshci_mmio_read_4(ctrlr, hcs);
		if (UFSHCIV(UFSHCI_HCS_REG_UCRDY, hcs))
			break;

		if (timeout - ticks < 0) {
			ufshci_printf(ctrlr,
			    "UIC command is not ready "
			    "within %d ms\n",
			    ctrlr->uic_cmd_timeout_in_ms);
			return (ENXIO);
		}

		/* TODO: Replace busy-wait with interrupt-based pause. */
		DELAY(10);
	}

	return (0);
}

static int
ufshci_uic_wait_cmd(struct ufshci_controller *ctrlr,
    struct ufshci_uic_cmd *uic_cmd)
{
	uint32_t is;
	int timeout;

	mtx_assert(&ctrlr->uic_cmd_lock, MA_OWNED);

	/* Wait for the IS flag to change */
	timeout = ticks + MSEC_2_TICKS(ctrlr->uic_cmd_timeout_in_ms);
	int delta = 10;

	while (1) {
		is = ufshci_mmio_read_4(ctrlr, is);
		if (UFSHCIV(UFSHCI_IS_REG_UCCS, is)) {
			ufshci_mmio_write_4(ctrlr, is,
			    UFSHCIM(UFSHCI_IS_REG_UCCS));
			break;
		}
		if (timeout - ticks < 0) {
			ufshci_printf(ctrlr,
			    "UIC command is not completed "
			    "within %d ms\n",
			    ctrlr->uic_cmd_timeout_in_ms);
			return (ENXIO);
		}

		DELAY(delta);
		delta = min(1000, delta * 2);
	}

	return (0);
}

static int
ufshci_uic_send_cmd(struct ufshci_controller *ctrlr,
    struct ufshci_uic_cmd *uic_cmd, uint32_t *return_value)
{
	int error;
	uint32_t config_result_code;

	mtx_lock(&ctrlr->uic_cmd_lock);

	error = ufshci_uic_cmd_ready(ctrlr);
	if (error) {
		mtx_unlock(&ctrlr->uic_cmd_lock);
		return (ENXIO);
	}

	ufshci_mmio_write_4(ctrlr, ucmdarg1, uic_cmd->argument1);
	ufshci_mmio_write_4(ctrlr, ucmdarg2, uic_cmd->argument2);
	ufshci_mmio_write_4(ctrlr, ucmdarg3, uic_cmd->argument3);

	ufshci_mmio_write_4(ctrlr, uiccmd, uic_cmd->opcode);

	error = ufshci_uic_wait_cmd(ctrlr, uic_cmd);

	mtx_unlock(&ctrlr->uic_cmd_lock);

	if (error)
		return (ENXIO);

	config_result_code = ufshci_mmio_read_4(ctrlr, ucmdarg2);
	if (config_result_code) {
		ufshci_printf(ctrlr,
		    "Failed to send UIC command. (config result code = 0x%x)\n",
		    config_result_code);
	}

	if (return_value != NULL)
		*return_value = ufshci_mmio_read_4(ctrlr, ucmdarg3);

	return (0);
}

int
ufshci_uic_send_dme_link_startup(struct ufshci_controller *ctrlr)
{
	struct ufshci_uic_cmd uic_cmd;
	uic_cmd.opcode = UFSHCI_DME_LINK_STARTUP;
	uic_cmd.argument1 = 0;
	uic_cmd.argument2 = 0;
	uic_cmd.argument3 = 0;

	return (ufshci_uic_send_cmd(ctrlr, &uic_cmd, NULL));
}

int
ufshci_uic_send_dme_get(struct ufshci_controller *ctrlr, uint16_t attribute,
    uint32_t *return_value)
{
	struct ufshci_uic_cmd uic_cmd;

	uic_cmd.opcode = UFSHCI_DME_GET;
	uic_cmd.argument1 = attribute << 16;
	uic_cmd.argument2 = 0;
	uic_cmd.argument3 = 0;

	return (ufshci_uic_send_cmd(ctrlr, &uic_cmd, return_value));
}

int
ufshci_uic_send_dme_set(struct ufshci_controller *ctrlr, uint16_t attribute,
    uint32_t value)
{
	struct ufshci_uic_cmd uic_cmd;

	uic_cmd.opcode = UFSHCI_DME_SET;
	uic_cmd.argument1 = attribute << 16;
	/* This drvier always sets only volatile values. */
	uic_cmd.argument2 = UFSHCI_ATTR_SET_TYPE_NORMAL << 16;
	uic_cmd.argument3 = value;

	return (ufshci_uic_send_cmd(ctrlr, &uic_cmd, NULL));
}

int
ufshci_uic_send_dme_peer_get(struct ufshci_controller *ctrlr,
    uint16_t attribute, uint32_t *return_value)
{
	struct ufshci_uic_cmd uic_cmd;

	uic_cmd.opcode = UFSHCI_DME_PEER_GET;
	uic_cmd.argument1 = attribute << 16;
	uic_cmd.argument2 = 0;
	uic_cmd.argument3 = 0;

	return (ufshci_uic_send_cmd(ctrlr, &uic_cmd, return_value));
}

int
ufshci_uic_send_dme_peer_set(struct ufshci_controller *ctrlr,
    uint16_t attribute, uint32_t value)
{
	struct ufshci_uic_cmd uic_cmd;

	uic_cmd.opcode = UFSHCI_DME_PEER_SET;
	uic_cmd.argument1 = attribute << 16;
	/* This drvier always sets only volatile values. */
	uic_cmd.argument2 = UFSHCI_ATTR_SET_TYPE_NORMAL << 16;
	uic_cmd.argument3 = value;

	return (ufshci_uic_send_cmd(ctrlr, &uic_cmd, NULL));
}

int
ufshci_uic_send_dme_endpoint_reset(struct ufshci_controller *ctrlr)
{
	struct ufshci_uic_cmd uic_cmd;

	uic_cmd.opcode = UFSHCI_DME_ENDPOINT_RESET;
	uic_cmd.argument1 = 0;
	uic_cmd.argument2 = 0;
	uic_cmd.argument3 = 0;

	return (ufshci_uic_send_cmd(ctrlr, &uic_cmd, NULL));
}
