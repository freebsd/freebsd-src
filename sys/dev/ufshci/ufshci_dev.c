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
ufshci_dev_read_unit_descriptor(struct ufshci_controller *ctrlr, uint8_t lun,
    struct ufshci_unit_descriptor *desc)
{
	return (ufshci_dev_read_descriptor(ctrlr, UFSHCI_DESC_TYPE_UNIT, lun, 0,
	    desc, sizeof(struct ufshci_unit_descriptor)));
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
ufshci_dev_clear_flag(struct ufshci_controller *ctrlr,
    enum ufshci_flags flag_type)
{
	struct ufshci_completion_poll_status status;
	struct ufshci_query_param param;

	param.function = UFSHCI_QUERY_FUNC_STANDARD_WRITE_REQUEST;
	param.opcode = UFSHCI_QUERY_OPCODE_CLEAR_FLAG;
	param.type = flag_type;
	param.index = 0;
	param.selector = 0;
	param.value = 0;

	status.done = 0;
	ufshci_ctrlr_cmd_send_query_request(ctrlr, ufshci_completion_poll_cb,
	    &status, param);
	ufshci_completion_poll(&status);
	if (status.error) {
		ufshci_printf(ctrlr, "ufshci_dev_clear_flag failed!\n");
		return (ENXIO);
	}

	return (0);
}

static int
ufshci_dev_read_attribute(struct ufshci_controller *ctrlr,
    enum ufshci_attributes attr_type, uint8_t index, uint8_t selector,
    uint64_t *value)
{
	struct ufshci_completion_poll_status status;
	struct ufshci_query_param param;

	param.function = UFSHCI_QUERY_FUNC_STANDARD_READ_REQUEST;
	param.opcode = UFSHCI_QUERY_OPCODE_READ_ATTRIBUTE;
	param.type = attr_type;
	param.index = index;
	param.selector = selector;
	param.value = 0;

	status.done = 0;
	ufshci_ctrlr_cmd_send_query_request(ctrlr, ufshci_completion_poll_cb,
	    &status, param);
	ufshci_completion_poll(&status);
	if (status.error) {
		ufshci_printf(ctrlr, "ufshci_dev_read_attribute failed!\n");
		return (ENXIO);
	}

	*value = status.cpl.response_upiu.query_response_upiu.value_64;

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
	uint32_t power_mode, peer_granularity;

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

	if (ctrlr->quirks & UFSHCI_QUIRK_CHANGE_LANE_AND_GEAR_SEPARATELY) {
		/* Before changing gears, first change the number of lanes. */
		if (ufshci_uic_send_dme_get(ctrlr, PA_PWRMode, &power_mode))
			return (ENXIO);
		if (ufshci_uic_send_dme_set(ctrlr, PA_PWRMode, power_mode))
			return (ENXIO);

		/* Wait for power mode changed. */
		if (ufshci_uic_power_mode_ready(ctrlr)) {
			ufshci_reg_dump(ctrlr);
			return (ENXIO);
		}
	}

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
	power_mode = (fast_mode << rx_bit_shift) | fast_mode;
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
		if (ufshci_uic_send_dme_peer_get(ctrlr, PA_Granularity,
			&peer_granularity))
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
	ufshci_printf(ctrlr, "UFS device spec version %u.%u.%u\n",
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

static int
ufshci_dev_enable_write_booster(struct ufshci_controller *ctrlr)
{
	struct ufshci_device *dev = &ctrlr->ufs_dev;
	int error;

	/* Enable WriteBooster */
	error = ufshci_dev_set_flag(ctrlr, UFSHCI_FLAG_F_WRITE_BOOSTER_EN);
	if (error) {
		ufshci_printf(ctrlr, "Failed to enable WriteBooster\n");
		return (error);
	}
	dev->is_wb_enabled = true;

	/* Enable WriteBooster buffer flush during hibernate */
	error = ufshci_dev_set_flag(ctrlr,
	    UFSHCI_FLAG_F_WB_BUFFER_FLUSH_DURING_HIBERNATE);
	if (error) {
		ufshci_printf(ctrlr,
		    "Failed to enable WriteBooster buffer flush during hibernate\n");
		return (error);
	}

	/* Enable WriteBooster buffer flush */
	error = ufshci_dev_set_flag(ctrlr, UFSHCI_FLAG_F_WB_BUFFER_FLUSH_EN);
	if (error) {
		ufshci_printf(ctrlr,
		    "Failed to enable WriteBooster buffer flush\n");
		return (error);
	}
	dev->is_wb_flush_enabled = true;

	return (0);
}

static int
ufshci_dev_disable_write_booster(struct ufshci_controller *ctrlr)
{
	struct ufshci_device *dev = &ctrlr->ufs_dev;
	int error;

	/* Disable WriteBooster buffer flush */
	error = ufshci_dev_clear_flag(ctrlr, UFSHCI_FLAG_F_WB_BUFFER_FLUSH_EN);
	if (error) {
		ufshci_printf(ctrlr,
		    "Failed to disable WriteBooster buffer flush\n");
		return (error);
	}
	dev->is_wb_flush_enabled = false;

	/* Disable WriteBooster buffer flush during hibernate */
	error = ufshci_dev_clear_flag(ctrlr,
	    UFSHCI_FLAG_F_WB_BUFFER_FLUSH_DURING_HIBERNATE);
	if (error) {
		ufshci_printf(ctrlr,
		    "Failed to disable WriteBooster buffer flush during hibernate\n");
		return (error);
	}

	/* Disable WriteBooster */
	error = ufshci_dev_clear_flag(ctrlr, UFSHCI_FLAG_F_WRITE_BOOSTER_EN);
	if (error) {
		ufshci_printf(ctrlr, "Failed to disable WriteBooster\n");
		return (error);
	}
	dev->is_wb_enabled = false;

	return (0);
}

static int
ufshci_dev_is_write_booster_buffer_life_time_left(
    struct ufshci_controller *ctrlr, bool *is_life_time_left)
{
	struct ufshci_device *dev = &ctrlr->ufs_dev;
	uint8_t buffer_lun;
	uint64_t life_time;
	uint32_t error;

	if (dev->wb_buffer_type == UFSHCI_DESC_WB_BUF_TYPE_LU_DEDICATED)
		buffer_lun = dev->wb_dedicated_lu;
	else
		buffer_lun = 0;

	error = ufshci_dev_read_attribute(ctrlr,
	    UFSHCI_ATTR_B_WB_BUFFER_LIFE_TIME_EST, buffer_lun, 0, &life_time);
	if (error)
		return (error);

	*is_life_time_left = (life_time != UFSHCI_ATTR_WB_LIFE_EXCEEDED);

	return (0);
}

/*
 * This function is not yet in use. It will be used when suspend/resume is
 * implemented.
 */
static __unused int
ufshci_dev_need_write_booster_buffer_flush(struct ufshci_controller *ctrlr,
    bool *need_flush)
{
	struct ufshci_device *dev = &ctrlr->ufs_dev;
	bool is_life_time_left = false;
	uint64_t available_buffer_size, current_buffer_size;
	uint8_t buffer_lun;
	uint32_t error;

	*need_flush = false;

	if (!dev->is_wb_enabled)
		return (0);

	error = ufshci_dev_is_write_booster_buffer_life_time_left(ctrlr,
	    &is_life_time_left);
	if (error)
		return (error);

	if (!is_life_time_left)
		return (ufshci_dev_disable_write_booster(ctrlr));

	if (dev->wb_buffer_type == UFSHCI_DESC_WB_BUF_TYPE_LU_DEDICATED)
		buffer_lun = dev->wb_dedicated_lu;
	else
		buffer_lun = 0;

	error = ufshci_dev_read_attribute(ctrlr,
	    UFSHCI_ATTR_B_AVAILABLE_WB_BUFFER_SIZE, buffer_lun, 0,
	    &available_buffer_size);
	if (error)
		return (error);

	switch (dev->wb_user_space_config_option) {
	case UFSHCI_DESC_WB_BUF_USER_SPACE_REDUCTION:
		*need_flush = (available_buffer_size <=
		    UFSHCI_ATTR_WB_AVAILABLE_10);
		break;
	case UFSHCI_DESC_WB_BUF_PRESERVE_USER_SPACE:
		/*
		 * In PRESERVE USER SPACE mode, flush should be performed when
		 * the current buffer is greater than 0 and the available buffer
		 * below write_booster_flush_threshold is left.
		 */
		error = ufshci_dev_read_attribute(ctrlr,
		    UFSHCI_ATTR_D_CURRENT_WB_BUFFER_SIZE, buffer_lun, 0,
		    &current_buffer_size);
		if (error)
			return (error);

		if (current_buffer_size == 0)
			return (0);

		*need_flush = (available_buffer_size <
		    dev->write_booster_flush_threshold);
		break;
	default:
		ufshci_printf(ctrlr,
		    "Invalid bWriteBoosterBufferPreserveUserSpaceEn value");
		return (EINVAL);
	}

	/*
	 * TODO: Need to handle WRITEBOOSTER_FLUSH_NEEDED exception case from
	 * wExceptionEventStatus attribute.
	 */

	return (0);
}

int
ufshci_dev_config_write_booster(struct ufshci_controller *ctrlr)
{
	struct ufshci_device *dev = &ctrlr->ufs_dev;
	uint32_t extended_ufs_feature_support;
	uint32_t alloc_units;
	struct ufshci_unit_descriptor unit_desc;
	uint8_t lun;
	bool is_life_time_left;
	uint32_t mega_byte = 1024 * 1024;
	uint32_t error = 0;

	extended_ufs_feature_support = be32toh(
	    dev->dev_desc.dExtendedUfsFeaturesSupport);
	if (!(extended_ufs_feature_support &
		UFSHCI_DESC_EXT_UFS_FEATURE_WRITE_BOOSTER)) {
		/* This device does not support Write Booster */
		return (0);
	}

	if (ufshci_dev_enable_write_booster(ctrlr))
		return (0);

	/* Get WriteBooster buffer parameters */
	dev->wb_buffer_type = dev->dev_desc.bWriteBoosterBufferType;
	dev->wb_user_space_config_option =
	    dev->dev_desc.bWriteBoosterBufferPreserveUserSpaceEn;

	/*
	 * Find the size of the write buffer.
	 * With LU-dedicated (00h), the WriteBooster buffer is assigned
	 * exclusively to one chosen LU (not one-per-LU), whereas Shared (01h)
	 * uses a single device-wide buffer shared by multiple LUs.
	 */
	if (dev->wb_buffer_type == UFSHCI_DESC_WB_BUF_TYPE_SINGLE_SHARED) {
		alloc_units = be32toh(
		    dev->dev_desc.dNumSharedWriteBoosterBufferAllocUnits);
		ufshci_printf(ctrlr,
		    "WriteBooster buffer type = Shared, alloc_units=%d\n",
		    alloc_units);
	} else if (dev->wb_buffer_type ==
	    UFSHCI_DESC_WB_BUF_TYPE_LU_DEDICATED) {
		ufshci_printf(ctrlr, "WriteBooster buffer type = Dedicated\n");
		for (lun = 0; lun < ctrlr->max_lun_count; lun++) {
			/* Find a dedicated buffer using a unit descriptor */
			if (ufshci_dev_read_unit_descriptor(ctrlr, lun,
				&unit_desc))
				continue;

			alloc_units = be32toh(
			    unit_desc.dLUNumWriteBoosterBufferAllocUnits);
			if (alloc_units) {
				dev->wb_dedicated_lu = lun;
				break;
			}
		}
	} else {
		ufshci_printf(ctrlr,
		    "Not supported WriteBooster buffer type: 0x%x\n",
		    dev->wb_buffer_type);
		goto out;
	}

	if (alloc_units == 0) {
		ufshci_printf(ctrlr, "The WriteBooster buffer size is zero\n");
		goto out;
	}

	dev->wb_buffer_size_mb = alloc_units *
	    dev->geo_desc.bAllocationUnitSize *
	    (be32toh(dev->geo_desc.dSegmentSize)) /
	    (mega_byte / UFSHCI_SECTOR_SIZE);

	/* Set to flush when 40% of the available buffer size remains */
	dev->write_booster_flush_threshold = UFSHCI_ATTR_WB_AVAILABLE_40;

	/*
	 * Check if WriteBooster Buffer lifetime is left.
	 * WriteBooster Buffer lifetime — percent of life used based on P/E
	 * cycles. If "preserve user space" is enabled, writes to normal user
	 * space also consume WB life since the area is shared.
	 */
	error = ufshci_dev_is_write_booster_buffer_life_time_left(ctrlr,
	    &is_life_time_left);
	if (error)
		goto out;

	if (!is_life_time_left) {
		ufshci_printf(ctrlr,
		    "There is no WriteBooster buffer life time left.\n");
		goto out;
	}

	ufshci_printf(ctrlr, "WriteBooster Enabled\n");
	return (0);
out:
	ufshci_dev_disable_write_booster(ctrlr);
	return (error);
}

