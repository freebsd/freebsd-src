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

static int
ufshci_dev_read_descriptor(struct ufshci_controller *ctrlr,
    enum ufshci_descriptor_type desc_type, uint8_t index, uint8_t selector,
    void *desc, size_t desc_size)
{
	struct ufshci_completion_poll_status status;
	struct ufshci_query_param param;

	param.function = UFSHCI_QUERY_FUNC_STANDARD_READ_REQUEST;
	param.opcode = UFSHCI_QUERY_OPCODE_READ_DESCRIPTOR;
	param.type = desc_type;
	param.index = index;
	param.selector = selector;
	param.value = 0;
	param.desc_size = desc_size;

	status.done = 0;
	ufshci_ctrlr_cmd_send_query_request(ctrlr, ufshci_completion_poll_cb,
	    &status, param);
	ufshci_completion_poll(&status);
	if (status.error) {
		ufshci_printf(ctrlr, "ufshci_dev_read_descriptor failed!\n");
		return (ENXIO);
	}

	memcpy(desc, status.cpl.response_upiu.query_response_upiu.command_data,
	    desc_size);

	return (0);
}

static int
ufshci_dev_read_device_descriptor(struct ufshci_controller *ctrlr,
    struct ufshci_device_descriptor *desc)
{
	return (ufshci_dev_read_descriptor(ctrlr, UFSHCI_DESC_TYPE_DEVICE, 0, 0,
	    desc, sizeof(struct ufshci_device_descriptor)));
}

static int
ufshci_dev_read_geometry_descriptor(struct ufshci_controller *ctrlr,
    struct ufshci_geometry_descriptor *desc)
{
	return (ufshci_dev_read_descriptor(ctrlr, UFSHCI_DESC_TYPE_GEOMETRY, 0,
	    0, desc, sizeof(struct ufshci_geometry_descriptor)));
}

static int
ufshci_dev_read_flag(struct ufshci_controller *ctrlr,
    enum ufshci_flags flag_type, uint8_t *flag)
{
	struct ufshci_completion_poll_status status;
	struct ufshci_query_param param;

	param.function = UFSHCI_QUERY_FUNC_STANDARD_READ_REQUEST;
	param.opcode = UFSHCI_QUERY_OPCODE_READ_FLAG;
	param.type = flag_type;
	param.index = 0;
	param.selector = 0;
	param.value = 0;

	status.done = 0;
	ufshci_ctrlr_cmd_send_query_request(ctrlr, ufshci_completion_poll_cb,
	    &status, param);
	ufshci_completion_poll(&status);
	if (status.error) {
		ufshci_printf(ctrlr, "ufshci_dev_read_flag failed!\n");
		return (ENXIO);
	}

	*flag = status.cpl.response_upiu.query_response_upiu.flag_value;

	return (0);
}

static int
ufshci_dev_set_flag(struct ufshci_controller *ctrlr,
    enum ufshci_flags flag_type)
{
	struct ufshci_completion_poll_status status;
	struct ufshci_query_param param;

	param.function = UFSHCI_QUERY_FUNC_STANDARD_WRITE_REQUEST;
	param.opcode = UFSHCI_QUERY_OPCODE_SET_FLAG;
	param.type = flag_type;
	param.index = 0;
	param.selector = 0;
	param.value = 0;

	status.done = 0;
	ufshci_ctrlr_cmd_send_query_request(ctrlr, ufshci_completion_poll_cb,
	    &status, param);
	ufshci_completion_poll(&status);
	if (status.error) {
		ufshci_printf(ctrlr, "ufshci_dev_set_flag failed!\n");
		return (ENXIO);
	}

	return (0);
}

static int
ufshci_dev_write_attribute(struct ufshci_controller *ctrlr,
    enum ufshci_attributes attr_type, uint8_t index, uint8_t selector,
    uint64_t value)
{
	struct ufshci_completion_poll_status status;
	struct ufshci_query_param param;

	param.function = UFSHCI_QUERY_FUNC_STANDARD_WRITE_REQUEST;
	param.opcode = UFSHCI_QUERY_OPCODE_WRITE_ATTRIBUTE;
	param.type = attr_type;
	param.index = index;
	param.selector = selector;
	param.value = value;

	status.done = 0;
	ufshci_ctrlr_cmd_send_query_request(ctrlr, ufshci_completion_poll_cb,
	    &status, param);
	ufshci_completion_poll(&status);
	if (status.error) {
		ufshci_printf(ctrlr, "ufshci_dev_write_attribute failed!\n");
		return (ENXIO);
	}

	return (0);
}

int
ufshci_dev_init(struct ufshci_controller *ctrlr)
{
	int timeout = ticks + MSEC_2_TICKS(ctrlr->device_init_timeout_in_ms);
	sbintime_t delta_t = SBT_1US;
	uint8_t flag;
	int error;
	const uint8_t device_init_completed = 0;

	error = ufshci_dev_set_flag(ctrlr, UFSHCI_FLAG_F_DEVICE_INIT);
	if (error)
		return (error);

	/* Wait for the UFSHCI_FLAG_F_DEVICE_INIT flag to change */
	while (1) {
		error = ufshci_dev_read_flag(ctrlr, UFSHCI_FLAG_F_DEVICE_INIT,
		    &flag);
		if (error)
			return (error);
		if (flag == device_init_completed)
			break;
		if (timeout - ticks < 0) {
			ufshci_printf(ctrlr,
			    "device init did not become %d "
			    "within %d ms\n",
			    device_init_completed,
			    ctrlr->device_init_timeout_in_ms);
			return (ENXIO);
		}

		pause_sbt("ufshciinit", delta_t, 0, C_PREL(1));
		delta_t = min(SBT_1MS, delta_t * 3 / 2);
	}

	return (0);
}

int
ufshci_dev_reset(struct ufshci_controller *ctrlr)
{
	if (ufshci_uic_send_dme_endpoint_reset(ctrlr))
		return (ENXIO);

	return (ufshci_dev_init(ctrlr));
}

int
ufshci_dev_init_reference_clock(struct ufshci_controller *ctrlr)
{
	int error;
	uint8_t index, selector;

	index = 0;    /* bRefClkFreq is device type attribute */
	selector = 0; /* bRefClkFreq is device type attribute */

	error = ufshci_dev_write_attribute(ctrlr, UFSHCI_ATTR_B_REF_CLK_FREQ,
	    index, selector, ctrlr->ref_clk);
	if (error)
		return (error);

	return (0);
}

int
ufshci_dev_init_unipro(struct ufshci_controller *ctrlr)
{
	uint32_t pa_granularity, peer_pa_granularity;
	uint32_t t_activate, pear_t_activate;

	/*
	 * Unipro Version:
	 * - 7~15 = Above 2.0, 6 = 2.0, 5 = 1.8, 4 = 1.61, 3 = 1.6, 2 = 1.41,
	 * 1 = 1.40, 0 = Reserved
	 */
	if (ufshci_uic_send_dme_get(ctrlr, PA_LocalVerInfo,
		&ctrlr->unipro_version))
		return (ENXIO);
	if (ufshci_uic_send_dme_get(ctrlr, PA_RemoteVerInfo,
		&ctrlr->ufs_dev.unipro_version))
		return (ENXIO);

	/*
	 * PA_Granularity: Granularity for PA_TActivate and PA_Hibern8Time
	 * - 1=1us, 2=4us, 3=8us, 4=16us, 5=32us, 6=100us
	 */
	if (ufshci_uic_send_dme_get(ctrlr, PA_Granularity, &pa_granularity))
		return (ENXIO);
	if (ufshci_uic_send_dme_peer_get(ctrlr, PA_Granularity,
		&peer_pa_granularity))
		return (ENXIO);

	/*
	 * PA_TActivate: Time to wait before activating a burst in order to
	 * wake-up peer M-RX
	 * UniPro automatically sets timing information such as PA_TActivate
	 * through the PACP_CAP_EXT1_ind command during Link Startup operation.
	 */
	if (ufshci_uic_send_dme_get(ctrlr, PA_TActivate, &t_activate))
		return (ENXIO);
	if (ufshci_uic_send_dme_peer_get(ctrlr, PA_TActivate, &pear_t_activate))
		return (ENXIO);

	if (ctrlr->quirks & UFSHCI_QUIRK_LONG_PEER_PA_TACTIVATE) {
		/*
		 * Intel Lake-field UFSHCI has a quirk. We need to add 200us to
		 * the PEER's PA_TActivate.
		 */
		if (pa_granularity == peer_pa_granularity) {
			pear_t_activate = t_activate + 2;
			if (ufshci_uic_send_dme_peer_set(ctrlr, PA_TActivate,
				pear_t_activate))
				return (ENXIO);
		}
	}

	return (0);
}

int
ufshci_dev_init_uic_power_mode(struct ufshci_controller *ctrlr)
{
	/* HSSerise: A = 1, B = 2 */
	const uint32_t hs_series = 2;
	/*
	 * TX/RX PWRMode:
	 * - TX[3:0], RX[7:4]
	 * - Fast Mode = 1, Slow Mode = 2, FastAuto Mode = 4, SlowAuto Mode = 5
	 */
	const uint32_t fast_mode = 1;
	const uint32_t rx_bit_shift = 4;
	const uint32_t power_mode = (fast_mode << rx_bit_shift) | fast_mode;

	/* Update lanes with available TX/RX lanes */
	if (ufshci_uic_send_dme_get(ctrlr, PA_AvailTxDataLanes,
		&ctrlr->max_tx_lanes))
		return (ENXIO);
	if (ufshci_uic_send_dme_get(ctrlr, PA_AvailRxDataLanes,
		&ctrlr->max_rx_lanes))
		return (ENXIO);

	/* Get max HS-GEAR value */
	if (ufshci_uic_send_dme_get(ctrlr, PA_MaxRxHSGear,
		&ctrlr->max_rx_hs_gear))
		return (ENXIO);

	/* Set the data lane to max */
	ctrlr->tx_lanes = ctrlr->max_tx_lanes;
	ctrlr->rx_lanes = ctrlr->max_rx_lanes;
	if (ufshci_uic_send_dme_set(ctrlr, PA_ActiveTxDataLanes,
		ctrlr->tx_lanes))
		return (ENXIO);
	if (ufshci_uic_send_dme_set(ctrlr, PA_ActiveRxDataLanes,
		ctrlr->rx_lanes))
		return (ENXIO);

	/* Set HS-GEAR to max gear */
	ctrlr->hs_gear = ctrlr->max_rx_hs_gear;
	if (ufshci_uic_send_dme_set(ctrlr, PA_TxGear, ctrlr->hs_gear))
		return (ENXIO);
	if (ufshci_uic_send_dme_set(ctrlr, PA_RxGear, ctrlr->hs_gear))
		return (ENXIO);

	/*
	 * Set termination
	 * - HS-MODE = ON / LS-MODE = OFF
	 */
	if (ufshci_uic_send_dme_set(ctrlr, PA_TxTermination, true))
		return (ENXIO);
	if (ufshci_uic_send_dme_set(ctrlr, PA_RxTermination, true))
		return (ENXIO);

	/* Set HSSerise (A = 1, B = 2) */
	if (ufshci_uic_send_dme_set(ctrlr, PA_HSSeries, hs_series))
		return (ENXIO);

	/* Set Timeout values */
	if (ufshci_uic_send_dme_set(ctrlr, PA_PWRModeUserData0,
		DL_FC0ProtectionTimeOutVal_Default))
		return (ENXIO);
	if (ufshci_uic_send_dme_set(ctrlr, PA_PWRModeUserData1,
		DL_TC0ReplayTimeOutVal_Default))
		return (ENXIO);
	if (ufshci_uic_send_dme_set(ctrlr, PA_PWRModeUserData2,
		DL_AFC0ReqTimeOutVal_Default))
		return (ENXIO);
	if (ufshci_uic_send_dme_set(ctrlr, PA_PWRModeUserData3,
		DL_FC0ProtectionTimeOutVal_Default))
		return (ENXIO);
	if (ufshci_uic_send_dme_set(ctrlr, PA_PWRModeUserData4,
		DL_TC0ReplayTimeOutVal_Default))
		return (ENXIO);
	if (ufshci_uic_send_dme_set(ctrlr, PA_PWRModeUserData5,
		DL_AFC0ReqTimeOutVal_Default))
		return (ENXIO);

	if (ufshci_uic_send_dme_set(ctrlr, DME_LocalFC0ProtectionTimeOutVal,
		DL_FC0ProtectionTimeOutVal_Default))
		return (ENXIO);
	if (ufshci_uic_send_dme_set(ctrlr, DME_LocalTC0ReplayTimeOutVal,
		DL_TC0ReplayTimeOutVal_Default))
		return (ENXIO);
	if (ufshci_uic_send_dme_set(ctrlr, DME_LocalAFC0ReqTimeOutVal,
		DL_AFC0ReqTimeOutVal_Default))
		return (ENXIO);

	/* Set TX/RX PWRMode */
	if (ufshci_uic_send_dme_set(ctrlr, PA_PWRMode, power_mode))
		return (ENXIO);

	/* Wait for power mode changed. */
	if (ufshci_uic_power_mode_ready(ctrlr)) {
		ufshci_reg_dump(ctrlr);
		return (ENXIO);
	}

	/* Clear 'Power Mode completion status' */
	ufshci_mmio_write_4(ctrlr, is, UFSHCIM(UFSHCI_IS_REG_UPMS));

	if (ctrlr->quirks & UFSHCI_QUIRK_WAIT_AFTER_POWER_MODE_CHANGE) {
		/*
		 * Intel Lake-field UFSHCI has a quirk.
		 * We need to wait 1250us and clear dme error.
		 */
		pause_sbt("ufshci", ustosbt(1250), 0, C_PREL(1));

		/* Test with dme_peer_get to make sure there are no errors. */
		if (ufshci_uic_send_dme_peer_get(ctrlr, PA_Granularity, NULL))
			return (ENXIO);
	}

	return (0);
}

int
ufshci_dev_init_ufs_power_mode(struct ufshci_controller *ctrlr)
{
	/* TODO: Need to implement */

	return (0);
}

int
ufshci_dev_get_descriptor(struct ufshci_controller *ctrlr)
{
	struct ufshci_device *device = &ctrlr->ufs_dev;
	/*
	 * The kDeviceDensityUnit is defined in the spec as 512.
	 * qTotalRawDeviceCapacity use big-endian byte ordering.
	 */
	const uint32_t device_density_unit = 512;
	uint32_t ver;
	int error;

	error = ufshci_dev_read_device_descriptor(ctrlr, &device->dev_desc);
	if (error)
		return (error);

	ver = be16toh(device->dev_desc.wSpecVersion);
	ufshci_printf(ctrlr, "UFS device spec version %u.%u%u\n",
	    UFSHCIV(UFSHCI_VER_REG_MJR, ver), UFSHCIV(UFSHCI_VER_REG_MNR, ver),
	    UFSHCIV(UFSHCI_VER_REG_VS, ver));
	ufshci_printf(ctrlr, "%u enabled LUNs found\n",
	    device->dev_desc.bNumberLU);

	error = ufshci_dev_read_geometry_descriptor(ctrlr, &device->geo_desc);
	if (error)
		return (error);

	if (device->geo_desc.bMaxNumberLU == 0) {
		device->max_lun_count = 8;
	} else if (device->geo_desc.bMaxNumberLU == 1) {
		device->max_lun_count = 32;
	} else {
		ufshci_printf(ctrlr,
		    "Invalid Geometry Descriptor bMaxNumberLU value=%d\n",
		    device->geo_desc.bMaxNumberLU);
		return (ENXIO);
	}
	ctrlr->max_lun_count = device->max_lun_count;

	ufshci_printf(ctrlr, "UFS device total size is %lu bytes\n",
	    be64toh(device->geo_desc.qTotalRawDeviceCapacity) *
		device_density_unit);

	return (0);
}
