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

#include <dev/vxge/vxgehal/vxgehal.h>

/*
 * vxge_hal_driver_config_check - Check driver configuration.
 * @config: Driver configuration information
 *
 * Check the driver configuration
 *
 * Returns: VXGE_HAL_OK - success,
 * otherwise one of the vxge_hal_status_e {} enumerated error codes.
 *
 */
vxge_hal_status_e
vxge_hal_driver_config_check(vxge_hal_driver_config_t *config)
{
	if (config->level > VXGE_TRACE)
		return (VXGE_HAL_BADCFG_LOG_LEVEL);
	return (VXGE_HAL_OK);
}

/*
 * __hal_device_wire_port_config_check - Check wire port configuration.
 * @port_config: Port configuration information
 *
 * Check wire port configuration
 *
 * Returns: VXGE_HAL_OK - success,
 * otherwise one of the vxge_hal_status_e enumerated error codes.
 *
 */
vxge_hal_status_e
__hal_device_wire_port_config_check(vxge_hal_wire_port_config_t *port_config)
{
	if (port_config->port_id > VXGE_HAL_WIRE_PORT_MAX_PORTS)
		return (VXGE_HAL_BADCFG_WIRE_PORT_PORT_ID);

	if ((port_config->media > VXGE_HAL_WIRE_PORT_MAX_MEDIA) &&
	    (port_config->media != VXGE_HAL_WIRE_PORT_MEDIA_DEFAULT))
		return (VXGE_HAL_BADCFG_WIRE_PORT_MAX_MEDIA);

	if (((port_config->mtu < VXGE_HAL_WIRE_PORT_MIN_INITIAL_MTU) ||
	    (port_config->mtu > VXGE_HAL_WIRE_PORT_MAX_INITIAL_MTU)) &&
	    (port_config->mtu != VXGE_HAL_WIRE_PORT_DEF_INITIAL_MTU))
		return (VXGE_HAL_BADCFG_WIRE_PORT_MAX_INITIAL_MTU);

	if ((port_config->autoneg_mode >
	    VXGE_HAL_WIRE_PORT_AUTONEG_MODE_RESERVED) &&
	    (port_config->autoneg_mode !=
	    VXGE_HAL_WIRE_PORT_AUTONEG_MODE_DEFAULT))
		return (VXGE_HAL_BADCFG_WIRE_PORT_AUTONEG_MODE);

	if ((port_config->autoneg_rate >
	    VXGE_HAL_WIRE_PORT_AUTONEG_RATE_10G) &&
	    (port_config->autoneg_rate !=
	    VXGE_HAL_WIRE_PORT_AUTONEG_RATE_DEFAULT))
		return (VXGE_HAL_BADCFG_WIRE_PORT_AUTONEG_RATE);

	if ((port_config->fixed_use_fsm !=
	    VXGE_HAL_WIRE_PORT_FIXED_USE_FSM_PROCESSOR) &&
	    (port_config->fixed_use_fsm !=
	    VXGE_HAL_WIRE_PORT_FIXED_USE_FSM_HW) &&
	    (port_config->fixed_use_fsm !=
	    VXGE_HAL_WIRE_PORT_FIXED_USE_FSM_DEFAULT))
		return (VXGE_HAL_BADCFG_WIRE_PORT_FIXED_USE_FSM);

	if ((port_config->antp_use_fsm !=
	    VXGE_HAL_WIRE_PORT_ANTP_USE_FSM_PROCESSOR) &&
	    (port_config->antp_use_fsm !=
	    VXGE_HAL_WIRE_PORT_ANTP_USE_FSM_HW) &&
	    (port_config->antp_use_fsm !=
	    VXGE_HAL_WIRE_PORT_ANTP_USE_FSM_DEFAULT))
		return (VXGE_HAL_BADCFG_WIRE_PORT_ANTP_USE_FSM);

	if ((port_config->anbe_use_fsm !=
	    VXGE_HAL_WIRE_PORT_ANBE_USE_FSM_PROCESSOR) &&
	    (port_config->anbe_use_fsm !=
	    VXGE_HAL_WIRE_PORT_ANBE_USE_FSM_HW) &&
	    (port_config->anbe_use_fsm !=
	    VXGE_HAL_WIRE_PORT_ANBE_USE_FSM_DEFAULT))
		return (VXGE_HAL_BADCFG_WIRE_PORT_ANBE_USE_FSM);

	if ((port_config->link_stability_period >
	    VXGE_HAL_WIRE_PORT_MAX_LINK_STABILITY_PERIOD) &&
	    (port_config->link_stability_period !=
	    VXGE_HAL_WIRE_PORT_DEF_LINK_STABILITY_PERIOD))
		return (VXGE_HAL_BADCFG_WIRE_PORT_LINK_STABILITY_PERIOD);

	if ((port_config->port_stability_period >
	    VXGE_HAL_WIRE_PORT_MAX_PORT_STABILITY_PERIOD) &&
	    (port_config->port_stability_period !=
	    VXGE_HAL_WIRE_PORT_DEF_PORT_STABILITY_PERIOD))
		return (VXGE_HAL_BADCFG_WIRE_PORT_PORT_STABILITY_PERIOD);

	if ((port_config->tmac_en != VXGE_HAL_WIRE_PORT_TMAC_ENABLE) &&
	    (port_config->tmac_en != VXGE_HAL_WIRE_PORT_TMAC_DISABLE) &&
	    (port_config->tmac_en != VXGE_HAL_WIRE_PORT_TMAC_DEFAULT))
		return (VXGE_HAL_BADCFG_WIRE_PORT_TMAC_EN);

	if ((port_config->rmac_en != VXGE_HAL_WIRE_PORT_RMAC_ENABLE) &&
	    (port_config->rmac_en != VXGE_HAL_WIRE_PORT_RMAC_DISABLE) &&
	    (port_config->rmac_en != VXGE_HAL_WIRE_PORT_RMAC_DEFAULT))
		return (VXGE_HAL_BADCFG_WIRE_PORT_RMAC_EN);

	if ((port_config->tmac_pad != VXGE_HAL_WIRE_PORT_TMAC_NO_PAD) &&
	    (port_config->tmac_pad != VXGE_HAL_WIRE_PORT_TMAC_64B_PAD) &&
	    (port_config->tmac_pad != VXGE_HAL_WIRE_PORT_TMAC_PAD_DEFAULT))
		return (VXGE_HAL_BADCFG_WIRE_PORT_TMAC_PAD);

	if ((port_config->tmac_pad_byte >
	    VXGE_HAL_WIRE_PORT_MAX_TMAC_PAD_BYTE) &&
	    (port_config->tmac_pad_byte !=
	    VXGE_HAL_WIRE_PORT_DEF_TMAC_PAD_BYTE))
		return (VXGE_HAL_BADCFG_WIRE_PORT_TMAC_PAD_BYTE);

	if ((port_config->tmac_util_period >
	    VXGE_HAL_WIRE_PORT_MAX_TMAC_UTIL_PERIOD) &&
	    (port_config->tmac_util_period !=
	    VXGE_HAL_WIRE_PORT_DEF_TMAC_UTIL_PERIOD))
		return (VXGE_HAL_BADCFG_WIRE_PORT_TMAC_UTIL_PERIOD);

	if ((port_config->rmac_strip_fcs !=
	    VXGE_HAL_WIRE_PORT_RMAC_STRIP_FCS) &&
	    (port_config->rmac_strip_fcs !=
	    VXGE_HAL_WIRE_PORT_RMAC_SEND_FCS_TO_HOST) &&
	    (port_config->rmac_strip_fcs !=
	    VXGE_HAL_WIRE_PORT_RMAC_STRIP_FCS_DEFAULT))
		return (VXGE_HAL_BADCFG_WIRE_PORT_RMAC_STRIP_FCS);

	if ((port_config->rmac_prom_en !=
	    VXGE_HAL_WIRE_PORT_RMAC_PROM_EN_ENABLE) &&
	    (port_config->rmac_prom_en !=
	    VXGE_HAL_WIRE_PORT_RMAC_PROM_EN_DISABLE) &&
	    (port_config->rmac_prom_en !=
	    VXGE_HAL_WIRE_PORT_RMAC_PROM_EN_DEFAULT))
		return (VXGE_HAL_BADCFG_WIRE_PORT_RMAC_PROM_EN);

	if ((port_config->rmac_discard_pfrm !=
	    VXGE_HAL_WIRE_PORT_RMAC_DISCARD_PFRM) &&
	    (port_config->rmac_discard_pfrm !=
	    VXGE_HAL_WIRE_PORT_RMAC_SEND_PFRM_TO_HOST) &&
	    (port_config->rmac_discard_pfrm !=
	    VXGE_HAL_WIRE_PORT_RMAC_DISCARD_PFRM_DEFAULT))
		return (VXGE_HAL_BADCFG_WIRE_PORT_RMAC_DISCARD_PFRM);

	if ((port_config->rmac_util_period >
	    VXGE_HAL_WIRE_PORT_MAX_RMAC_UTIL_PERIOD) &&
	    (port_config->rmac_util_period !=
	    VXGE_HAL_WIRE_PORT_DEF_RMAC_UTIL_PERIOD))
		return (VXGE_HAL_BADCFG_WIRE_PORT_RMAC_UTIL_PERIOD);

	if ((port_config->rmac_pause_gen_en !=
	    VXGE_HAL_WIRE_PORT_RMAC_PAUSE_GEN_EN_ENABLE) &&
	    (port_config->rmac_pause_gen_en !=
	    VXGE_HAL_WIRE_PORT_RMAC_PAUSE_GEN_EN_DISABLE) &&
	    (port_config->rmac_pause_gen_en !=
	    VXGE_HAL_WIRE_PORT_RMAC_PAUSE_GEN_EN_DEFAULT))
		return (VXGE_HAL_BADCFG_WIRE_PORT_RMAC_PAUSE_GEN_EN);

	if ((port_config->rmac_pause_rcv_en !=
	    VXGE_HAL_WIRE_PORT_RMAC_PAUSE_RCV_EN_ENABLE) &&
	    (port_config->rmac_pause_rcv_en !=
	    VXGE_HAL_WIRE_PORT_RMAC_PAUSE_RCV_EN_DISABLE) &&
	    (port_config->rmac_pause_rcv_en !=
	    VXGE_HAL_WIRE_PORT_RMAC_PAUSE_RCV_EN_DEFAULT))
		return (VXGE_HAL_BADCFG_WIRE_PORT_RMAC_PAUSE_RCV_EN);

	if (((port_config->rmac_pause_time <
	    VXGE_HAL_WIRE_PORT_MIN_RMAC_HIGH_PTIME) ||
	    (port_config->rmac_pause_time >
	    VXGE_HAL_WIRE_PORT_MAX_RMAC_HIGH_PTIME)) &&
	    (port_config->rmac_pause_time !=
	    VXGE_HAL_WIRE_PORT_DEF_RMAC_HIGH_PTIME))
		return (VXGE_HAL_BADCFG_WIRE_PORT_RMAC_HIGH_PTIME);

	if ((port_config->limiter_en !=
	    VXGE_HAL_WIRE_PORT_RMAC_PAUSE_LIMITER_ENABLE) &&
	    (port_config->limiter_en !=
	    VXGE_HAL_WIRE_PORT_RMAC_PAUSE_LIMITER_DISABLE) &&
	    (port_config->limiter_en !=
	    VXGE_HAL_WIRE_PORT_RMAC_PAUSE_LIMITER_DEFAULT))
		return (VXGE_HAL_BADCFG_WIRE_PORT_RMAC_PAUSE_LIMITER_EN);

	if ((port_config->max_limit > VXGE_HAL_WIRE_PORT_MAX_RMAC_MAX_LIMIT) &&
	    (port_config->max_limit != VXGE_HAL_WIRE_PORT_DEF_RMAC_MAX_LIMIT))
		return (VXGE_HAL_BADCFG_WIRE_PORT_RMAC_MAX_LIMIT);

	return (VXGE_HAL_OK);
}


/*
 * __hal_device_switch_port_config_check - Check switch port configuration.
 * @port_config: Port configuration information
 *
 * Check switch port configuration
 *
 * Returns: VXGE_HAL_OK - success,
 * otherwise one of the vxge_hal_status_e enumerated error codes.
 *
 */
vxge_hal_status_e
__hal_device_switch_port_config_check(
    vxge_hal_switch_port_config_t *port_config)
{
	if (((port_config->mtu < VXGE_HAL_SWITCH_PORT_MIN_INITIAL_MTU) ||
	    (port_config->mtu > VXGE_HAL_SWITCH_PORT_MAX_INITIAL_MTU)) &&
	    (port_config->mtu != VXGE_HAL_SWITCH_PORT_DEF_INITIAL_MTU))
		return (VXGE_HAL_BADCFG_SWITCH_PORT_MAX_INITIAL_MTU);

	if ((port_config->tmac_en != VXGE_HAL_SWITCH_PORT_TMAC_ENABLE) &&
	    (port_config->tmac_en != VXGE_HAL_SWITCH_PORT_TMAC_DISABLE) &&
	    (port_config->tmac_en != VXGE_HAL_SWITCH_PORT_TMAC_DEFAULT))
		return (VXGE_HAL_BADCFG_SWITCH_PORT_TMAC_EN);

	if ((port_config->rmac_en != VXGE_HAL_SWITCH_PORT_RMAC_ENABLE) &&
	    (port_config->rmac_en != VXGE_HAL_SWITCH_PORT_RMAC_DISABLE) &&
	    (port_config->rmac_en != VXGE_HAL_SWITCH_PORT_RMAC_DEFAULT))
		return (VXGE_HAL_BADCFG_SWITCH_PORT_RMAC_EN);

	if ((port_config->tmac_pad != VXGE_HAL_SWITCH_PORT_TMAC_NO_PAD) &&
	    (port_config->tmac_pad != VXGE_HAL_SWITCH_PORT_TMAC_64B_PAD) &&
	    (port_config->tmac_pad != VXGE_HAL_SWITCH_PORT_TMAC_PAD_DEFAULT))
		return (VXGE_HAL_BADCFG_SWITCH_PORT_TMAC_PAD);

	if ((port_config->tmac_pad_byte >
	    VXGE_HAL_SWITCH_PORT_MAX_TMAC_PAD_BYTE) &&
	    (port_config->tmac_pad_byte !=
	    VXGE_HAL_SWITCH_PORT_DEF_TMAC_PAD_BYTE))
		return (VXGE_HAL_BADCFG_SWITCH_PORT_TMAC_PAD_BYTE);

	if ((port_config->tmac_util_period >
	    VXGE_HAL_SWITCH_PORT_MAX_TMAC_UTIL_PERIOD) &&
	    (port_config->tmac_util_period !=
	    VXGE_HAL_SWITCH_PORT_DEF_TMAC_UTIL_PERIOD))
		return (VXGE_HAL_BADCFG_SWITCH_PORT_TMAC_UTIL_PERIOD);

	if ((port_config->rmac_strip_fcs !=
	    VXGE_HAL_SWITCH_PORT_RMAC_STRIP_FCS) &&
	    (port_config->rmac_strip_fcs !=
	    VXGE_HAL_SWITCH_PORT_RMAC_SEND_FCS_TO_HOST) &&
	    (port_config->rmac_strip_fcs !=
	    VXGE_HAL_SWITCH_PORT_RMAC_STRIP_FCS_DEFAULT))
		return (VXGE_HAL_BADCFG_SWITCH_PORT_RMAC_STRIP_FCS);

	if ((port_config->rmac_prom_en !=
	    VXGE_HAL_SWITCH_PORT_RMAC_PROM_EN_ENABLE) &&
	    (port_config->rmac_prom_en !=
	    VXGE_HAL_SWITCH_PORT_RMAC_PROM_EN_DISABLE) &&
	    (port_config->rmac_prom_en !=
	    VXGE_HAL_SWITCH_PORT_RMAC_PROM_EN_DEFAULT))
		return (VXGE_HAL_BADCFG_SWITCH_PORT_RMAC_PROM_EN);

	if ((port_config->rmac_discard_pfrm !=
	    VXGE_HAL_SWITCH_PORT_RMAC_DISCARD_PFRM) &&
	    (port_config->rmac_discard_pfrm !=
	    VXGE_HAL_SWITCH_PORT_RMAC_SEND_PFRM_TO_HOST) &&
	    (port_config->rmac_discard_pfrm !=
	    VXGE_HAL_SWITCH_PORT_RMAC_DISCARD_PFRM_DEFAULT))
		return (VXGE_HAL_BADCFG_SWITCH_PORT_RMAC_DISCARD_PFRM);

	if ((port_config->rmac_util_period >
	    VXGE_HAL_SWITCH_PORT_MAX_RMAC_UTIL_PERIOD) &&
	    (port_config->rmac_util_period !=
	    VXGE_HAL_SWITCH_PORT_DEF_RMAC_UTIL_PERIOD))
		return (VXGE_HAL_BADCFG_SWITCH_PORT_RMAC_UTIL_PERIOD);

	if ((port_config->rmac_pause_gen_en !=
	    VXGE_HAL_SWITCH_PORT_RMAC_PAUSE_GEN_EN_ENABLE) &&
	    (port_config->rmac_pause_gen_en !=
	    VXGE_HAL_SWITCH_PORT_RMAC_PAUSE_GEN_EN_DISABLE) &&
	    (port_config->rmac_pause_gen_en !=
	    VXGE_HAL_SWITCH_PORT_RMAC_PAUSE_GEN_EN_DEFAULT))
		return (VXGE_HAL_BADCFG_SWITCH_PORT_RMAC_PAUSE_GEN_EN);

	if ((port_config->rmac_pause_rcv_en !=
	    VXGE_HAL_SWITCH_PORT_RMAC_PAUSE_RCV_EN_ENABLE) &&
	    (port_config->rmac_pause_rcv_en !=
	    VXGE_HAL_SWITCH_PORT_RMAC_PAUSE_RCV_EN_DISABLE) &&
	    (port_config->rmac_pause_rcv_en !=
	    VXGE_HAL_SWITCH_PORT_RMAC_PAUSE_RCV_EN_DEFAULT))
		return (VXGE_HAL_BADCFG_SWITCH_PORT_RMAC_PAUSE_RCV_EN);

	if (((port_config->rmac_pause_time <
	    VXGE_HAL_SWITCH_PORT_MIN_RMAC_HIGH_PTIME) ||
	    (port_config->rmac_pause_time >
	    VXGE_HAL_SWITCH_PORT_MAX_RMAC_HIGH_PTIME)) &&
	    (port_config->rmac_pause_time !=
	    VXGE_HAL_SWITCH_PORT_DEF_RMAC_HIGH_PTIME))
		return (VXGE_HAL_BADCFG_SWITCH_PORT_RMAC_HIGH_PTIME);

	if ((port_config->limiter_en !=
	    VXGE_HAL_SWITCH_PORT_RMAC_PAUSE_LIMITER_ENABLE) &&
	    (port_config->limiter_en !=
	    VXGE_HAL_SWITCH_PORT_RMAC_PAUSE_LIMITER_DISABLE) &&
	    (port_config->limiter_en !=
	    VXGE_HAL_SWITCH_PORT_RMAC_PAUSE_LIMITER_DEFAULT))
		return (VXGE_HAL_BADCFG_SWITCH_PORT_RMAC_PAUSE_LIMITER_EN);

	if ((port_config->max_limit >
	    VXGE_HAL_SWITCH_PORT_MAX_RMAC_MAX_LIMIT) &&
	    (port_config->max_limit !=
	    VXGE_HAL_SWITCH_PORT_DEF_RMAC_MAX_LIMIT))
		return (VXGE_HAL_BADCFG_SWITCH_PORT_RMAC_MAX_LIMIT);

	return (VXGE_HAL_OK);
}

/*
 * __hal_device_mac_config_check - Check mac port configuration.
 * @mac_config: MAC configuration information
 *
 * Check mac port configuration
 *
 * Returns: VXGE_HAL_OK - success,
 * otherwise one of the vxge_hal_status_e enumerated error codes.
 *
 */
vxge_hal_status_e
__hal_device_mac_config_check(vxge_hal_mac_config_t *mac_config)
{
	u32 i;
	vxge_hal_status_e status;

	status = __hal_device_wire_port_config_check(
	    &mac_config->wire_port_config[0]);

	if (status != VXGE_HAL_OK)
		return (status);

	status = __hal_device_wire_port_config_check(
	    &mac_config->wire_port_config[1]);

	if (status != VXGE_HAL_OK)
		return (status);

	status = __hal_device_switch_port_config_check(
	    &mac_config->switch_port_config);

	if (status != VXGE_HAL_OK)
		return (status);

	if ((mac_config->network_stability_period >
	    VXGE_HAL_MAC_MAX_NETWORK_STABILITY_PERIOD) &&
	    (mac_config->network_stability_period !=
	    VXGE_HAL_MAC_DEF_NETWORK_STABILITY_PERIOD))
		return (VXGE_HAL_BADCFG_MAC_NETWORK_STABILITY_PERIOD);

	for (i = 0; i < 16; i++) {

		if ((mac_config->mc_pause_threshold[i] >
		    VXGE_HAL_MAC_MAX_MC_PAUSE_THRESHOLD) &&
		    (mac_config->mc_pause_threshold[i] !=
		    VXGE_HAL_MAC_DEF_MC_PAUSE_THRESHOLD))
			return (VXGE_HAL_BADCFG_MAC_MC_PAUSE_THRESHOLD);

	}

	if ((mac_config->tmac_perma_stop_en !=
	    VXGE_HAL_MAC_TMAC_PERMA_STOP_ENABLE) &&
	    (mac_config->tmac_perma_stop_en !=
	    VXGE_HAL_MAC_TMAC_PERMA_STOP_DISABLE) &&
	    (mac_config->tmac_perma_stop_en !=
	    VXGE_HAL_MAC_TMAC_PERMA_STOP_DEFAULT))
		return (VXGE_HAL_BADCFG_MAC_PERMA_STOP_EN);

	if ((mac_config->tmac_tx_switch_dis !=
	    VXGE_HAL_MAC_TMAC_TX_SWITCH_ENABLE) &&
	    (mac_config->tmac_tx_switch_dis !=
	    VXGE_HAL_MAC_TMAC_TX_SWITCH_DISABLE) &&
	    (mac_config->tmac_tx_switch_dis !=
	    VXGE_HAL_MAC_TMAC_TX_SWITCH_DEFAULT))
		return (VXGE_HAL_BADCFG_MAC_TMAC_TX_SWITCH_DIS);

	if ((mac_config->tmac_lossy_switch_en !=
	    VXGE_HAL_MAC_TMAC_LOSSY_SWITCH_ENABLE) &&
	    (mac_config->tmac_lossy_switch_en !=
	    VXGE_HAL_MAC_TMAC_LOSSY_SWITCH_DISABLE) &&
	    (mac_config->tmac_lossy_switch_en !=
	    VXGE_HAL_MAC_TMAC_LOSSY_SWITCH_DEFAULT))
		return (VXGE_HAL_BADCFG_MAC_TMAC_LOSSY_SWITCH_EN);

	if ((mac_config->tmac_lossy_wire_en !=
	    VXGE_HAL_MAC_TMAC_LOSSY_WIRE_ENABLE) &&
	    (mac_config->tmac_lossy_wire_en !=
	    VXGE_HAL_MAC_TMAC_LOSSY_WIRE_DISABLE) &&
	    (mac_config->tmac_lossy_wire_en !=
	    VXGE_HAL_MAC_TMAC_LOSSY_WIRE_DEFAULT))
		return (VXGE_HAL_BADCFG_MAC_TMAC_LOSSY_WIRE_EN);

	if ((mac_config->tmac_bcast_to_wire_dis !=
	    VXGE_HAL_MAC_TMAC_BCAST_TO_WIRE_DISABLE) &&
	    (mac_config->tmac_bcast_to_wire_dis !=
	    VXGE_HAL_MAC_TMAC_BCAST_TO_WIRE_ENABLE) &&
	    (mac_config->tmac_bcast_to_wire_dis !=
	    VXGE_HAL_MAC_TMAC_BCAST_TO_WIRE_DEFAULT))
		return (VXGE_HAL_BADCFG_MAC_TMAC_BCAST_TO_WIRE_DIS);

	if ((mac_config->tmac_bcast_to_switch_dis !=
	    VXGE_HAL_MAC_TMAC_BCAST_TO_SWITCH_DISABLE) &&
	    (mac_config->tmac_bcast_to_switch_dis !=
	    VXGE_HAL_MAC_TMAC_BCAST_TO_SWITCH_ENABLE) &&
	    (mac_config->tmac_bcast_to_switch_dis !=
	    VXGE_HAL_MAC_TMAC_BCAST_TO_SWITCH_DEFAULT))
		return (VXGE_HAL_BADCFG_MAC_TMAC_BCAST_TO_SWITCH_DIS);

	if ((mac_config->tmac_host_append_fcs_en !=
	    VXGE_HAL_MAC_TMAC_HOST_APPEND_FCS_ENABLE) &&
	    (mac_config->tmac_host_append_fcs_en !=
	    VXGE_HAL_MAC_TMAC_HOST_APPEND_FCS_DISABLE) &&
	    (mac_config->tmac_host_append_fcs_en !=
	    VXGE_HAL_MAC_TMAC_HOST_APPEND_FCS_DEFAULT))
		return (VXGE_HAL_BADCFG_MAC_TMAC_HOST_APPEND_FCS_EN);

	if ((mac_config->tpa_support_snap_ab_n !=
	    VXGE_HAL_MAC_TPA_SUPPORT_SNAP_AB_N_LLC_SAP_AB) &&
	    (mac_config->tpa_support_snap_ab_n !=
	    VXGE_HAL_MAC_TPA_SUPPORT_SNAP_AB_N_LLC_SAP_AA) &&
	    (mac_config->tpa_support_snap_ab_n !=
	    VXGE_HAL_MAC_TPA_SUPPORT_SNAP_AB_N_DEFAULT))
		return (VXGE_HAL_BADCFG_MAC_TPA_SUPPORT_SNAP_AB_N);

	if ((mac_config->tpa_ecc_enable_n !=
	    VXGE_HAL_MAC_TPA_ECC_ENABLE_N_ENABLE) &&
	    (mac_config->tpa_ecc_enable_n !=
	    VXGE_HAL_MAC_TPA_ECC_ENABLE_N_DISABLE) &&
	    (mac_config->tpa_ecc_enable_n !=
	    VXGE_HAL_MAC_TPA_ECC_ENABLE_N_DEFAULT))
		return (VXGE_HAL_BADCFG_MAC_TPA_ECC_ENABLE_N);

	if ((mac_config->rpa_ignore_frame_err !=
	    VXGE_HAL_MAC_RPA_IGNORE_FRAME_ERR_ENABLE) &&
	    (mac_config->rpa_ignore_frame_err !=
	    VXGE_HAL_MAC_RPA_IGNORE_FRAME_ERR_DISABLE) &&
	    (mac_config->rpa_ignore_frame_err !=
	    VXGE_HAL_MAC_RPA_IGNORE_FRAME_ERR_DEFAULT))
		return (VXGE_HAL_BADCFG_MAC_RPA_IGNORE_FRAME_ERR);

	if ((mac_config->rpa_support_snap_ab_n !=
	    VXGE_HAL_MAC_RPA_SUPPORT_SNAP_AB_N_ENABLE) &&
	    (mac_config->rpa_support_snap_ab_n !=
	    VXGE_HAL_MAC_RPA_SUPPORT_SNAP_AB_N_DISABLE) &&
	    (mac_config->rpa_support_snap_ab_n !=
	    VXGE_HAL_MAC_RPA_SUPPORT_SNAP_AB_N_DEFAULT))
		return (VXGE_HAL_BADCFG_MAC_RPA_SNAP_AB_N);

	if ((mac_config->rpa_search_for_hao !=
	    VXGE_HAL_MAC_RPA_SEARCH_FOR_HAO_ENABLE) &&
	    (mac_config->rpa_search_for_hao !=
	    VXGE_HAL_MAC_RPA_SEARCH_FOR_HAO_DISABLE) &&
	    (mac_config->rpa_search_for_hao !=
	    VXGE_HAL_MAC_RPA_SEARCH_FOR_HAO_DEFAULT))
		return (VXGE_HAL_BADCFG_MAC_RPA_SEARCH_FOR_HAO);

	if ((mac_config->rpa_support_ipv6_mobile_hdrs !=
	    VXGE_HAL_MAC_RPA_SUPPORT_IPV6_MOBILE_HDRS_ENABLE) &&
	    (mac_config->rpa_support_ipv6_mobile_hdrs !=
	    VXGE_HAL_MAC_RPA_SUPPORT_IPV6_MOBILE_HDRS_DISABLE) &&
	    (mac_config->rpa_support_ipv6_mobile_hdrs !=
	    VXGE_HAL_MAC_RPA_SUPPORT_IPV6_MOBILE_HDRS_DEFAULT))
		return (VXGE_HAL_BADCFG_MAC_RPA_SUPPORT_IPV6_MOBILE_HDRS);

	if ((mac_config->rpa_ipv6_stop_searching !=
	    VXGE_HAL_MAC_RPA_IPV6_STOP_SEARCHING) &&
	    (mac_config->rpa_ipv6_stop_searching !=
	    VXGE_HAL_MAC_RPA_IPV6_DONT_STOP_SEARCHING) &&
	    (mac_config->rpa_ipv6_stop_searching !=
	    VXGE_HAL_MAC_RPA_IPV6_STOP_SEARCHING_DEFAULT))
		return (VXGE_HAL_BADCFG_MAC_RPA_IPV6_STOP_SEARCHING);

	if ((mac_config->rpa_no_ps_if_unknown !=
	    VXGE_HAL_MAC_RPA_NO_PS_IF_UNKNOWN_ENABLE) &&
	    (mac_config->rpa_no_ps_if_unknown !=
	    VXGE_HAL_MAC_RPA_NO_PS_IF_UNKNOWN_DISABLE) &&
	    (mac_config->rpa_no_ps_if_unknown !=
	    VXGE_HAL_MAC_RPA_NO_PS_IF_UNKNOWN_DEFAULT))
		return (VXGE_HAL_BADCFG_MAC_RPA_NO_PS_IF_UNKNOWN);

	if ((mac_config->rpa_search_for_etype !=
	    VXGE_HAL_MAC_RPA_SEARCH_FOR_ETYPE_ENABLE) &&
	    (mac_config->rpa_search_for_etype !=
	    VXGE_HAL_MAC_RPA_SEARCH_FOR_ETYPE_DISABLE) &&
	    (mac_config->rpa_search_for_etype !=
	    VXGE_HAL_MAC_RPA_SEARCH_FOR_ETYPE_DEFAULT))
		return (VXGE_HAL_BADCFG_MAC_RPA_SEARCH_FOR_ETYPE);

	if ((mac_config->rpa_repl_l4_comp_csum !=
	    VXGE_HAL_MAC_RPA_REPL_L4_COMP_CSUM_ENABLE) &&
	    (mac_config->rpa_repl_l4_comp_csum !=
	    VXGE_HAL_MAC_RPA_REPL_L4_COMP_CSUM_DISABLE) &&
	    (mac_config->rpa_repl_l4_comp_csum !=
	    VXGE_HAL_MAC_RPA_REPL_l4_COMP_CSUM_DEFAULT))
		return (VXGE_HAL_BADCFG_MAC_RPA_REPL_L4_COMP_CSUM);

	if ((mac_config->rpa_repl_l3_incl_cf !=
	    VXGE_HAL_MAC_RPA_REPL_L3_INCL_CF_ENABLE) &&
	    (mac_config->rpa_repl_l3_incl_cf !=
	    VXGE_HAL_MAC_RPA_REPL_L3_INCL_CF_DISABLE) &&
	    (mac_config->rpa_repl_l3_incl_cf !=
	    VXGE_HAL_MAC_RPA_REPL_L3_INCL_CF_DEFAULT))
		return (VXGE_HAL_BADCFG_MAC_RPA_REPL_L3_INCL_CF);

	if ((mac_config->rpa_repl_l3_comp_csum !=
	    VXGE_HAL_MAC_RPA_REPL_L3_COMP_CSUM_ENABLE) &&
	    (mac_config->rpa_repl_l3_comp_csum !=
	    VXGE_HAL_MAC_RPA_REPL_L3_COMP_CSUM_DISABLE) &&
	    (mac_config->rpa_repl_l3_comp_csum !=
	    VXGE_HAL_MAC_RPA_REPL_l3_COMP_CSUM_DEFAULT))
		return (VXGE_HAL_BADCFG_MAC_RPA_REPL_L3_COMP_CSUM);

	if ((mac_config->rpa_repl_ipv4_tcp_incl_ph !=
	    VXGE_HAL_MAC_RPA_REPL_IPV4_TCP_INCL_PH_ENABLE) &&
	    (mac_config->rpa_repl_ipv4_tcp_incl_ph !=
	    VXGE_HAL_MAC_RPA_REPL_IPV4_TCP_INCL_PH_DISABLE) &&
	    (mac_config->rpa_repl_ipv4_tcp_incl_ph !=
	    VXGE_HAL_MAC_RPA_REPL_IPV4_TCP_INCL_PH_DEFAULT))
		return (VXGE_HAL_BADCFG_MAC_RPA_REPL_IPV4_TCP_INCL_PH);

	if ((mac_config->rpa_repl_ipv6_tcp_incl_ph !=
	    VXGE_HAL_MAC_RPA_REPL_IPV6_TCP_INCL_PH_ENABLE) &&
	    (mac_config->rpa_repl_ipv6_tcp_incl_ph !=
	    VXGE_HAL_MAC_RPA_REPL_IPV6_TCP_INCL_PH_DISABLE) &&
	    (mac_config->rpa_repl_ipv6_tcp_incl_ph !=
	    VXGE_HAL_MAC_RPA_REPL_IPV6_TCP_INCL_PH_DEFAULT))
		return (VXGE_HAL_BADCFG_MAC_RPA_REPL_IPV6_TCP_INCL_PH);

	if ((mac_config->rpa_repl_ipv4_udp_incl_ph !=
	    VXGE_HAL_MAC_RPA_REPL_IPV4_UDP_INCL_PH_ENABLE) &&
	    (mac_config->rpa_repl_ipv4_udp_incl_ph !=
	    VXGE_HAL_MAC_RPA_REPL_IPV4_UDP_INCL_PH_DISABLE) &&
	    (mac_config->rpa_repl_ipv4_udp_incl_ph !=
	    VXGE_HAL_MAC_RPA_REPL_IPV4_UDP_INCL_PH_DEFAULT))
		return (VXGE_HAL_BADCFG_MAC_RPA_REPL_IPV4_UDP_INCL_PH);

	if ((mac_config->rpa_repl_ipv6_udp_incl_ph !=
	    VXGE_HAL_MAC_RPA_REPL_IPV6_UDP_INCL_PH_ENABLE) &&
	    (mac_config->rpa_repl_ipv6_udp_incl_ph !=
	    VXGE_HAL_MAC_RPA_REPL_IPV6_UDP_INCL_PH_DISABLE) &&
	    (mac_config->rpa_repl_ipv6_udp_incl_ph !=
	    VXGE_HAL_MAC_RPA_REPL_IPV6_UDP_INCL_PH_DEFAULT))
		return (VXGE_HAL_BADCFG_MAC_RPA_REPL_IPV6_UDP_INCL_PH);

	if ((mac_config->rpa_repl_l4_incl_cf !=
	    VXGE_HAL_MAC_RPA_REPL_L4_INCL_CF_ENABLE) &&
	    (mac_config->rpa_repl_l4_incl_cf !=
	    VXGE_HAL_MAC_RPA_REPL_L4_INCL_CF_DISABLE) &&
	    (mac_config->rpa_repl_l4_incl_cf !=
	    VXGE_HAL_MAC_RPA_REPL_L4_INCL_CF_DEFAULT))
		return (VXGE_HAL_BADCFG_MAC_RPA_REPL_L4_INCL_CF);

	if ((mac_config->rpa_repl_strip_vlan_tag !=
	    VXGE_HAL_MAC_RPA_REPL_STRIP_VLAN_TAG_ENABLE) &&
	    (mac_config->rpa_repl_strip_vlan_tag !=
	    VXGE_HAL_MAC_RPA_REPL_STRIP_VLAN_TAG_DISABLE) &&
	    (mac_config->rpa_repl_strip_vlan_tag !=
	    VXGE_HAL_MAC_RPA_REPL_STRIP_VLAN_TAG_DEFAULT))
		return (VXGE_HAL_BADCFG_MAC_RPA_REPL_STRIP_VLAN_TAG);

	return (VXGE_HAL_OK);
}

/*
 * __hal_device_lag_port_config_check - Check LAG port configuration.
 * @aggr_config: LAG port configuration information
 *
 * Check LAG port configuration
 *
 * Returns: VXGE_HAL_OK - success,
 * otherwise one of the vxge_hal_status_e enumerated error codes.
 *
 */
vxge_hal_status_e
__hal_device_lag_port_config_check(vxge_hal_lag_port_config_t *port_config)
{
	if ((port_config->port_id != VXGE_HAL_LAG_PORT_PORT_ID_0) &&
	    (port_config->port_id != VXGE_HAL_LAG_PORT_PORT_ID_1))
		return (VXGE_HAL_BADCFG_LAG_PORT_PORT_ID);

	if ((port_config->lag_en !=
	    VXGE_HAL_LAG_PORT_LAG_EN_DISABLE) &&
	    (port_config->lag_en !=
	    VXGE_HAL_LAG_PORT_LAG_EN_ENABLE) &&
	    (port_config->lag_en !=
	    VXGE_HAL_LAG_PORT_LAG_EN_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_PORT_LAG_EN);

	if ((port_config->discard_slow_proto !=
	    VXGE_HAL_LAG_PORT_DISCARD_SLOW_PROTO_DISABLE) &&
	    (port_config->discard_slow_proto !=
	    VXGE_HAL_LAG_PORT_DISCARD_SLOW_PROTO_ENABLE) &&
	    (port_config->discard_slow_proto !=
	    VXGE_HAL_LAG_PORT_DISCARD_SLOW_PROTO_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_PORT_DISCARD_SLOW_PROTO);

	if ((port_config->host_chosen_aggr !=
	    VXGE_HAL_LAG_PORT_HOST_CHOSEN_AGGR_0) &&
	    (port_config->host_chosen_aggr !=
	    VXGE_HAL_LAG_PORT_HOST_CHOSEN_AGGR_1) &&
	    (port_config->host_chosen_aggr !=
	    VXGE_HAL_LAG_PORT_HOST_CHOSEN_AGGR_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_PORT_HOST_CHOSEN_AGGR);

	if ((port_config->discard_unknown_slow_proto !=
	    VXGE_HAL_LAG_PORT_DISCARD_UNKNOWN_SLOW_PROTO_DISABLE) &&
	    (port_config->discard_unknown_slow_proto !=
	    VXGE_HAL_LAG_PORT_DISCARD_UNKNOWN_SLOW_PROTO_ENABLE) &&
	    (port_config->discard_unknown_slow_proto !=
	    VXGE_HAL_LAG_PORT_DISCARD_UNKNOWN_SLOW_PROTO_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_PORT_DISCARD_UNKNOWN_SLOW_PROTO);

	if ((port_config->actor_port_num >
	    VXGE_HAL_LAG_PORT_MAX_ACTOR_PORT_NUM) &&
	    (port_config->actor_port_num !=
	    VXGE_HAL_LAG_PORT_DEF_ACTOR_PORT_NUM))
		return (VXGE_HAL_BADCFG_LAG_PORT_ACTOR_PORT_NUM);

	if ((port_config->actor_port_priority >
	    VXGE_HAL_LAG_PORT_MAX_ACTOR_PORT_PRIORITY) &&
	    (port_config->actor_port_priority !=
	    VXGE_HAL_LAG_PORT_DEF_ACTOR_PORT_PRIORITY))
		return (VXGE_HAL_BADCFG_LAG_PORT_ACTOR_PORT_PRIORITY);

	if ((port_config->actor_key_10g >
	    VXGE_HAL_LAG_PORT_MAX_ACTOR_KEY_10G) &&
	    (port_config->actor_key_10g !=
	    VXGE_HAL_LAG_PORT_DEF_ACTOR_KEY_10G))
		return (VXGE_HAL_BADCFG_LAG_PORT_ACTOR_KEY_10G);

	if ((port_config->actor_key_1g > VXGE_HAL_LAG_PORT_MAX_ACTOR_KEY_1G) &&
	    (port_config->actor_key_1g != VXGE_HAL_LAG_PORT_DEF_ACTOR_KEY_1G))
		return (VXGE_HAL_BADCFG_LAG_PORT_ACTOR_KEY_1G);

	if ((port_config->actor_lacp_activity !=
	    VXGE_HAL_LAG_PORT_ACTOR_LACP_ACTIVITY_PASSIVE) &&
	    (port_config->actor_lacp_activity !=
	    VXGE_HAL_LAG_PORT_ACTOR_LACP_ACTIVITY_ACTIVE) &&
	    (port_config->actor_lacp_activity !=
	    VXGE_HAL_LAG_PORT_ACTOR_LACP_ACTIVITY_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_PORT_ACTOR_LACP_ACTIVITY);

	if ((port_config->actor_lacp_timeout !=
	    VXGE_HAL_LAG_PORT_ACTOR_LACP_TIMEOUT_LONG) &&
	    (port_config->actor_lacp_timeout !=
	    VXGE_HAL_LAG_PORT_ACTOR_LACP_TIMEOUT_SHORT) &&
	    (port_config->actor_lacp_timeout !=
	    VXGE_HAL_LAG_PORT_ACTOR_LACP_TIMEOUT_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_PORT_ACTOR_LACP_TIMEOUT);

	if ((port_config->actor_aggregation !=
	    VXGE_HAL_LAG_PORT_ACTOR_AGGREGATION_INDIVIDUAL) &&
	    (port_config->actor_aggregation !=
	    VXGE_HAL_LAG_PORT_ACTOR_AGGREGATION_AGGREGATEABLE) &&
	    (port_config->actor_aggregation !=
	    VXGE_HAL_LAG_PORT_ACTOR_AGGREGATION_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_PORT_ACTOR_AGGREGATION);

	if ((port_config->actor_synchronization !=
	    VXGE_HAL_LAG_PORT_ACTOR_SYNCHRONIZATION_OUT_OF_SYNC) &&
	    (port_config->actor_synchronization !=
	    VXGE_HAL_LAG_PORT_ACTOR_SYNCHRONIZATION_IN_SYNC) &&
	    (port_config->actor_synchronization !=
	    VXGE_HAL_LAG_PORT_ACTOR_SYNCHRONIZATION_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_PORT_ACTOR_SYNCHRONIZATION);

	if ((port_config->actor_collecting !=
	    VXGE_HAL_LAG_PORT_ACTOR_COLLECTING_DISABLE) &&
	    (port_config->actor_collecting !=
	    VXGE_HAL_LAG_PORT_ACTOR_COLLECTING_ENABLE) &&
	    (port_config->actor_collecting !=
	    VXGE_HAL_LAG_PORT_ACTOR_COLLECTING_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_PORT_ACTOR_COLLECTING);

	if ((port_config->actor_distributing !=
	    VXGE_HAL_LAG_PORT_ACTOR_DISTRIBUTING_DISABLE) &&
	    (port_config->actor_distributing !=
	    VXGE_HAL_LAG_PORT_ACTOR_DISTRIBUTING_ENABLE) &&
	    (port_config->actor_distributing !=
	    VXGE_HAL_LAG_PORT_ACTOR_DISTRIBUTING_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_PORT_ACTOR_DISTRIBUTING);

	if ((port_config->actor_distributing !=
	    VXGE_HAL_LAG_PORT_ACTOR_DISTRIBUTING_DISABLE) &&
	    (port_config->actor_distributing !=
	    VXGE_HAL_LAG_PORT_ACTOR_DISTRIBUTING_ENABLE) &&
	    (port_config->actor_distributing !=
	    VXGE_HAL_LAG_PORT_ACTOR_DISTRIBUTING_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_PORT_ACTOR_DISTRIBUTING);

	if ((port_config->actor_defaulted !=
	    VXGE_HAL_LAG_PORT_ACTOR_DEFAULTED) &&
	    (port_config->actor_defaulted !=
	    VXGE_HAL_LAG_PORT_ACTOR_NOT_DEFAULTED) &&
	    (port_config->actor_defaulted !=
	    VXGE_HAL_LAG_PORT_ACTOR_DEFAULTED_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_PORT_ACTOR_DEFAULTED);

	if ((port_config->actor_expired !=
	    VXGE_HAL_LAG_PORT_ACTOR_EXPIRED) &&
	    (port_config->actor_expired !=
	    VXGE_HAL_LAG_PORT_ACTOR_NOT_EXPIRED) &&
	    (port_config->actor_expired !=
	    VXGE_HAL_LAG_PORT_ACTOR_EXPIRED_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_PORT_ACTOR_EXPIRED);

	if ((port_config->partner_sys_pri >
	    VXGE_HAL_LAG_PORT_MAX_PARTNER_SYS_PRI) &&
	    (port_config->partner_sys_pri !=
	    VXGE_HAL_LAG_PORT_DEF_PARTNER_SYS_PRI))
		return (VXGE_HAL_BADCFG_LAG_PORT_PARTNER_SYS_PRI);

	if ((port_config->partner_key > VXGE_HAL_LAG_PORT_MAX_PARTNER_KEY) &&
	    (port_config->partner_key != VXGE_HAL_LAG_PORT_DEF_PARTNER_KEY))
		return (VXGE_HAL_BADCFG_LAG_PORT_PARTNER_KEY);

	if ((port_config->partner_port_num >
	    VXGE_HAL_LAG_PORT_MAX_PARTNER_PORT_NUM) &&
	    (port_config->partner_port_num !=
	    VXGE_HAL_LAG_PORT_DEF_PARTNER_PORT_NUM))
		return (VXGE_HAL_BADCFG_LAG_PORT_PARTNER_NUM);

	if ((port_config->partner_port_priority >
	    VXGE_HAL_LAG_PORT_MAX_PARTNER_PORT_PRIORITY) &&
	    (port_config->partner_port_priority !=
	    VXGE_HAL_LAG_PORT_DEF_PARTNER_PORT_PRIORITY))
		return (VXGE_HAL_BADCFG_LAG_PORT_PARTNER_PORT_PRIORITY);

	if ((port_config->partner_lacp_activity !=
	    VXGE_HAL_LAG_PORT_PARTNER_LACP_ACTIVITY_PASSIVE) &&
	    (port_config->partner_lacp_activity !=
	    VXGE_HAL_LAG_PORT_PARTNER_LACP_ACTIVITY_ACTIVE) &&
	    (port_config->partner_lacp_activity !=
	    VXGE_HAL_LAG_PORT_PARTNER_LACP_ACTIVITY_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_PORT_PARTNER_LACP_ACTIVITY);

	if ((port_config->partner_lacp_timeout !=
	    VXGE_HAL_LAG_PORT_PARTNER_LACP_TIMEOUT_LONG) &&
	    (port_config->partner_lacp_timeout !=
	    VXGE_HAL_LAG_PORT_PARTNER_LACP_TIMEOUT_SHORT) &&
	    (port_config->partner_lacp_timeout !=
	    VXGE_HAL_LAG_PORT_PARTNER_LACP_TIMEOUT_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_PORT_PARTNER_LACP_TIMEOUT);

	if ((port_config->partner_aggregation !=
	    VXGE_HAL_LAG_PORT_PARTNER_AGGREGATION_INDIVIDUAL) &&
	    (port_config->partner_aggregation !=
	    VXGE_HAL_LAG_PORT_PARTNER_AGGREGATION_AGGREGATEABLE) &&
	    (port_config->partner_aggregation !=
	    VXGE_HAL_LAG_PORT_PARTNER_AGGREGATION_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_PORT_PARTNER_AGGREGATION);

	if ((port_config->partner_synchronization !=
	    VXGE_HAL_LAG_PORT_PARTNER_SYNCHRONIZATION_OUT_OF_SYNC) &&
	    (port_config->partner_synchronization !=
	    VXGE_HAL_LAG_PORT_PARTNER_SYNCHRONIZATION_IN_SYNC) &&
	    (port_config->partner_synchronization !=
	    VXGE_HAL_LAG_PORT_PARTNER_SYNCHRONIZATION_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_PORT_PARTNER_SYNCHRONIZATION);

	if ((port_config->partner_collecting !=
	    VXGE_HAL_LAG_PORT_PARTNER_COLLECTING_DISABLE) &&
	    (port_config->partner_collecting !=
	    VXGE_HAL_LAG_PORT_PARTNER_COLLECTING_ENABLE) &&
	    (port_config->partner_collecting !=
	    VXGE_HAL_LAG_PORT_PARTNER_COLLECTING_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_PORT_PARTNER_COLLECTING);

	if ((port_config->partner_distributing !=
	    VXGE_HAL_LAG_PORT_PARTNER_DISTRIBUTING_DISABLE) &&
	    (port_config->partner_distributing !=
	    VXGE_HAL_LAG_PORT_PARTNER_DISTRIBUTING_ENABLE) &&
	    (port_config->partner_distributing !=
	    VXGE_HAL_LAG_PORT_PARTNER_DISTRIBUTING_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_PORT_PARTNER_DISTRIBUTING);

	if ((port_config->partner_distributing !=
	    VXGE_HAL_LAG_PORT_PARTNER_DISTRIBUTING_DISABLE) &&
	    (port_config->partner_distributing !=
	    VXGE_HAL_LAG_PORT_PARTNER_DISTRIBUTING_ENABLE) &&
	    (port_config->partner_distributing !=
	    VXGE_HAL_LAG_PORT_PARTNER_DISTRIBUTING_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_PORT_PARTNER_DISTRIBUTING);

	if ((port_config->partner_defaulted !=
	    VXGE_HAL_LAG_PORT_PARTNER_DEFAULTED) &&
	    (port_config->partner_defaulted !=
	    VXGE_HAL_LAG_PORT_PARTNER_NOT_DEFAULTED) &&
	    (port_config->partner_defaulted !=
	    VXGE_HAL_LAG_PORT_PARTNER_DEFAULTED_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_PORT_PARTNER_DEFAULTED);

	if ((port_config->partner_expired !=
	    VXGE_HAL_LAG_PORT_PARTNER_EXPIRED) &&
	    (port_config->partner_expired !=
	    VXGE_HAL_LAG_PORT_PARTNER_NOT_EXPIRED) &&
	    (port_config->partner_expired !=
	    VXGE_HAL_LAG_PORT_PARTNER_EXPIRED_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_PORT_PARTNER_EXPIRED);

	return (VXGE_HAL_OK);
}

/*
 * __hal_device_lag_aggr_config_check - Check aggregator configuration.
 * @aggr_config: Aggregator configuration information
 *
 * Check aggregator configuration
 *
 * Returns: VXGE_HAL_OK - success,
 * otherwise one of the vxge_hal_status_e enumerated error codes.
 *
 */
vxge_hal_status_e
__hal_device_lag_aggr_config_check(vxge_hal_lag_aggr_config_t *aggr_config)
{
	if ((aggr_config->aggr_id != VXGE_HAL_LAG_AGGR_AGGR_ID_1) &&
	    (aggr_config->aggr_id != VXGE_HAL_LAG_AGGR_AGGR_ID_2))
		return (VXGE_HAL_BADCFG_LAG_AGGR_AGGR_ID);

	if ((aggr_config->use_port_mac_addr !=
	    VXGE_HAL_LAG_AGGR_USE_PORT_MAC_ADDR_DISBALE) &&
	    (aggr_config->use_port_mac_addr !=
	    VXGE_HAL_LAG_AGGR_USE_PORT_MAC_ADDR_ENABLE) &&
	    (aggr_config->use_port_mac_addr !=
	    VXGE_HAL_LAG_AGGR_USE_PORT_MAC_ADDR_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_AGGR_USE_PORT_MAC_ADDR);

	if ((aggr_config->mac_addr_sel !=
	    VXGE_HAL_LAG_AGGR_MAC_ADDR_SEL_PORT_0) &&
	    (aggr_config->mac_addr_sel !=
	    VXGE_HAL_LAG_AGGR_MAC_ADDR_SEL_PORT_1) &&
	    (aggr_config->mac_addr_sel !=
	    VXGE_HAL_LAG_AGGR_MAC_ADDR_SEL_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_AGGR_MAC_ADDR_SEL);

	if ((aggr_config->admin_key > VXGE_HAL_LAG_AGGR_MAX_ADMIN_KEY) &&
	    (aggr_config->admin_key != VXGE_HAL_LAG_AGGR_DEF_ADMIN_KEY))
		return (VXGE_HAL_BADCFG_LAG_AGGR_ADMIN_KEY);

	return (VXGE_HAL_OK);
}

/*
 * __hal_device_lag_la_config_check
 * Check LAG link aggregation mode configuration.
 * @la_config: LAG configuration information
 *
 * Check LAG link aggregation mode configuration
 *
 * Returns: VXGE_HAL_OK - success,
 * otherwise one of the vxge_hal_status_e enumerated error codes.
 *
 */
vxge_hal_status_e
__hal_device_lag_la_config_check(vxge_hal_lag_la_config_t *la_config)
{
	if ((la_config->tx_discard != VXGE_HAL_LAG_TX_DISCARD_DISBALE) &&
	    (la_config->tx_discard != VXGE_HAL_LAG_TX_DISCARD_ENABLE) &&
	    (la_config->tx_discard != VXGE_HAL_LAG_TX_DISCARD_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_TX_DISCARD);

	if ((la_config->distrib_alg_sel !=
	    VXGE_HAL_LAG_DISTRIB_ALG_SEL_SRC_VPATH) &&
	    (la_config->distrib_alg_sel !=
	    VXGE_HAL_LAG_DISTRIB_ALG_SEL_DEST_MAC_ADDR) &&
	    (la_config->distrib_alg_sel !=
	    VXGE_HAL_LAG_DISTRIB_ALG_SEL_SRC_MAC_ADDR) &&
	    (la_config->distrib_alg_sel !=
	    VXGE_HAL_LAG_DISTRIB_ALG_SEL_BOTH_MAC_ADDR) &&
	    (la_config->distrib_alg_sel !=
	    VXGE_HAL_LAG_DISTRIB_ALG_SEL_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_DISTRIB_ALG_SEL);

	if ((la_config->distrib_remap_if_fail !=
	    VXGE_HAL_LAG_DISTRIB_REMAP_IF_FAIL_DISBALE) &&
	    (la_config->distrib_remap_if_fail !=
	    VXGE_HAL_LAG_DISTRIB_REMAP_IF_FAIL_ENABLE) &&
	    (la_config->distrib_remap_if_fail !=
	    VXGE_HAL_LAG_DISTRIB_REMAP_IF_FAIL_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_DISTRIB_REMAP_IF_FAIL);

	if ((la_config->coll_max_delay > VXGE_HAL_LAG_MAX_COLL_MAX_DELAY) &&
	    (la_config->coll_max_delay != VXGE_HAL_LAG_DEF_COLL_MAX_DELAY))
		return (VXGE_HAL_BADCFG_LAG_COLL_MAX_DELAY);

	if ((la_config->rx_discard != VXGE_HAL_LAG_RX_DISCARD_DISBALE) &&
	    (la_config->rx_discard != VXGE_HAL_LAG_RX_DISCARD_ENABLE) &&
	    (la_config->rx_discard != VXGE_HAL_LAG_RX_DISCARD_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_RX_DISCARD);

	return (VXGE_HAL_OK);
}

/*
 * __hal_device_lag_ap_config_check - Check LAG a/p mode configuration.
 * @ap_config: LAG configuration information
 *
 * Check LAG a/p mode configuration
 *
 * Returns: VXGE_HAL_OK - success,
 * otherwise one of the vxge_hal_status_e enumerated error codes.
 *
 */
vxge_hal_status_e
__hal_device_lag_ap_config_check(vxge_hal_lag_ap_config_t *ap_config)
{
	if ((ap_config->hot_standby !=
	    VXGE_HAL_LAG_HOT_STANDBY_DISBALE_PORT) &&
	    (ap_config->hot_standby !=
	    VXGE_HAL_LAG_HOT_STANDBY_KEEP_UP_PORT) &&
	    (ap_config->hot_standby !=
	    VXGE_HAL_LAG_HOT_STANDBY_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_HOT_STANDBY);

	if ((ap_config->lacp_decides != VXGE_HAL_LAG_LACP_DECIDES_DISBALE) &&
	    (ap_config->lacp_decides != VXGE_HAL_LAG_LACP_DECIDES_ENBALE) &&
	    (ap_config->lacp_decides != VXGE_HAL_LAG_LACP_DECIDES_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_LACP_DECIDES);

	if ((ap_config->pref_active_port !=
	    VXGE_HAL_LAG_PREF_ACTIVE_PORT_0) &&
	    (ap_config->pref_active_port !=
	    VXGE_HAL_LAG_PREF_ACTIVE_PORT_1) &&
	    (ap_config->pref_active_port !=
	    VXGE_HAL_LAG_PREF_ACTIVE_PORT_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_PREF_ACTIVE_PORT);

	if ((ap_config->auto_failback != VXGE_HAL_LAG_AUTO_FAILBACK_DISBALE) &&
	    (ap_config->auto_failback != VXGE_HAL_LAG_AUTO_FAILBACK_ENBALE) &&
	    (ap_config->auto_failback != VXGE_HAL_LAG_AUTO_FAILBACK_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_AUTO_FAILBACK);

	if ((ap_config->failback_en != VXGE_HAL_LAG_FAILBACK_EN_DISBALE) &&
	    (ap_config->failback_en != VXGE_HAL_LAG_FAILBACK_EN_ENBALE) &&
	    (ap_config->failback_en != VXGE_HAL_LAG_FAILBACK_EN_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_FAILBACK_EN);

	if ((ap_config->cold_failover_timeout !=
	    VXGE_HAL_LAG_MIN_COLD_FAILOVER_TIMEOUT) &&
	    (ap_config->cold_failover_timeout !=
	    VXGE_HAL_LAG_MAX_COLD_FAILOVER_TIMEOUT) &&
	    (ap_config->cold_failover_timeout !=
	    VXGE_HAL_LAG_DEF_COLD_FAILOVER_TIMEOUT))
		return (VXGE_HAL_BADCFG_LAG_COLD_FAILOVER_TIMEOUT);

	if ((ap_config->alt_admin_key !=
	    VXGE_HAL_LAG_MIN_ALT_ADMIN_KEY) &&
	    (ap_config->alt_admin_key !=
	    VXGE_HAL_LAG_MAX_ALT_ADMIN_KEY) &&
	    (ap_config->alt_admin_key !=
	    VXGE_HAL_LAG_DEF_ALT_ADMIN_KEY))
		return (VXGE_HAL_BADCFG_LAG_ALT_ADMIN_KEY);

	if ((ap_config->alt_aggr !=
	    VXGE_HAL_LAG_ALT_AGGR_0) &&
	    (ap_config->alt_aggr !=
	    VXGE_HAL_LAG_ALT_AGGR_1) &&
	    (ap_config->alt_aggr !=
	    VXGE_HAL_LAG_ALT_AGGR_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_ALT_AGGR);

	return (VXGE_HAL_OK);
}

/*
 * __hal_device_lag_sl_config_check - Check LAG Single Link mode configuration.
 * @sl_config: LAG configuration information
 *
 * Check LAG Single link mode configuration
 *
 * Returns: VXGE_HAL_OK - success,
 * otherwise one of the vxge_hal_status_e enumerated error codes.
 *
 */
vxge_hal_status_e
__hal_device_lag_sl_config_check(vxge_hal_lag_sl_config_t *sl_config)
{
	if ((sl_config->pref_indiv_port !=
	    VXGE_HAL_LAG_PREF_INDIV_PORT_0) &&
	    (sl_config->pref_indiv_port !=
	    VXGE_HAL_LAG_PREF_INDIV_PORT_1) &&
	    (sl_config->pref_indiv_port !=
	    VXGE_HAL_LAG_PREF_INDIV_PORT_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_PREF_INDIV_PORT);

	return (VXGE_HAL_OK);
}

/*
 * __hal_device_lag_lacp_config_check - Check LACP configuration.
 * @lacp_config: LAG configuration information
 *
 * Check LACP configuration
 *
 * Returns: VXGE_HAL_OK - success,
 * otherwise one of the vxge_hal_status_e enumerated error codes.
 *
 */
vxge_hal_status_e
__hal_device_lag_lacp_config_check(vxge_hal_lag_lacp_config_t *lacp_config)
{
	if ((lacp_config->lacp_en != VXGE_HAL_LAG_LACP_EN_DISBALE) &&
	    (lacp_config->lacp_en != VXGE_HAL_LAG_LACP_EN_ENABLE) &&
	    (lacp_config->lacp_en != VXGE_HAL_LAG_LACP_EN_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_LACP_EN);

	if ((lacp_config->lacp_begin != VXGE_HAL_LAG_LACP_BEGIN_NORMAL) &&
	    (lacp_config->lacp_begin != VXGE_HAL_LAG_LACP_BEGIN_RESET) &&
	    (lacp_config->lacp_begin != VXGE_HAL_LAG_LACP_BEGIN_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_LACP_BEGIN);

	if ((lacp_config->discard_lacp != VXGE_HAL_LAG_DISCARD_LACP_DISBALE) &&
	    (lacp_config->discard_lacp != VXGE_HAL_LAG_DISCARD_LACP_ENABLE) &&
	    (lacp_config->discard_lacp != VXGE_HAL_LAG_DISCARD_LACP_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_DISCARD_LACP);

	if ((lacp_config->liberal_len_chk !=
	    VXGE_HAL_LAG_LIBERAL_LEN_CHK_DISBALE) &&
	    (lacp_config->liberal_len_chk !=
	    VXGE_HAL_LAG_LIBERAL_LEN_CHK_ENABLE) &&
	    (lacp_config->liberal_len_chk !=
	    VXGE_HAL_LAG_LIBERAL_LEN_CHK_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_LIBERAL_LEN_CHK);

	if ((lacp_config->marker_gen_recv_en !=
	    VXGE_HAL_LAG_MARKER_GEN_RECV_EN_DISBALE) &&
	    (lacp_config->marker_gen_recv_en !=
	    VXGE_HAL_LAG_MARKER_GEN_RECV_EN_ENABLE) &&
	    (lacp_config->marker_gen_recv_en !=
	    VXGE_HAL_LAG_MARKER_GEN_RECV_EN_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_MARKER_GEN_RECV_EN);

	if ((lacp_config->marker_resp_en !=
	    VXGE_HAL_LAG_MARKER_RESP_EN_DISBALE) &&
	    (lacp_config->marker_resp_en !=
	    VXGE_HAL_LAG_MARKER_RESP_EN_ENABLE) &&
	    (lacp_config->marker_resp_en !=
	    VXGE_HAL_LAG_MARKER_RESP_EN_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_MARKER_RESP_EN);

	if ((lacp_config->marker_resp_timeout !=
	    VXGE_HAL_LAG_MIN_MARKER_RESP_TIMEOUT) &&
	    (lacp_config->marker_resp_timeout !=
	    VXGE_HAL_LAG_MAX_MARKER_RESP_TIMEOUT) &&
	    (lacp_config->marker_resp_timeout !=
	    VXGE_HAL_LAG_DEF_MARKER_RESP_TIMEOUT))
		return (VXGE_HAL_BADCFG_LAG_MARKER_RESP_TIMEOUT);

	if ((lacp_config->slow_proto_mrkr_min_interval !=
	    VXGE_HAL_LAG_MIN_SLOW_PROTO_MRKR_MIN_INTERVAL) &&
	    (lacp_config->slow_proto_mrkr_min_interval !=
	    VXGE_HAL_LAG_MAX_SLOW_PROTO_MRKR_MIN_INTERVAL) &&
	    (lacp_config->slow_proto_mrkr_min_interval !=
	    VXGE_HAL_LAG_DEF_SLOW_PROTO_MRKR_MIN_INTERVAL))
		return (VXGE_HAL_BADCFG_LAG_SLOW_PROTO_MRKR_MIN_INTERVAL);

	if ((lacp_config->throttle_mrkr_resp !=
	    VXGE_HAL_LAG_THROTTLE_MRKR_RESP_DISBALE) &&
	    (lacp_config->throttle_mrkr_resp !=
	    VXGE_HAL_LAG_THROTTLE_MRKR_RESP_ENABLE) &&
	    (lacp_config->throttle_mrkr_resp !=
	    VXGE_HAL_LAG_THROTTLE_MRKR_RESP_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_THROTTLE_MRKR_RESP);

	return (VXGE_HAL_OK);
}

/*
 * __hal_device_lag_config_check - Check link aggregation configuration.
 * @lag_config: LAG configuration information
 *
 * Check link aggregation configuration
 *
 * Returns: VXGE_HAL_OK - success,
 * otherwise one of the vxge_hal_status_e enumerated error codes.
 *
 */
vxge_hal_status_e
__hal_device_lag_config_check(vxge_hal_lag_config_t *lag_config)
{
	u32 i;
	vxge_hal_status_e status;

	if ((lag_config->lag_en != VXGE_HAL_LAG_LAG_EN_DISABLE) &&
	    (lag_config->lag_en != VXGE_HAL_LAG_LAG_EN_ENABLE) &&
	    (lag_config->lag_en != VXGE_HAL_LAG_LAG_EN_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_LAG_EN);

	if ((lag_config->lag_mode != VXGE_HAL_LAG_LAG_MODE_LAG) &&
	    (lag_config->lag_mode !=
	    VXGE_HAL_LAG_LAG_MODE_ACTIVE_PASSIVE_FAILOVER) &&
	    (lag_config->lag_mode != VXGE_HAL_LAG_LAG_MODE_SINGLE_LINK) &&
	    (lag_config->lag_mode != VXGE_HAL_LAG_LAG_MODE_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_LAG_MODE);

	status = __hal_device_lag_la_config_check(&lag_config->la_mode_config);
	if (status == VXGE_HAL_OK)
		return (status);

	status = __hal_device_lag_ap_config_check(&lag_config->ap_mode_config);
	if (status == VXGE_HAL_OK)
		return (status);

	status = __hal_device_lag_sl_config_check(&lag_config->sl_mode_config);
	if (status == VXGE_HAL_OK)
		return (status);

	status = __hal_device_lag_lacp_config_check(&lag_config->lacp_config);
	if (status == VXGE_HAL_OK)
		return (status);

	if ((lag_config->incr_tx_aggr_stats !=
	    VXGE_HAL_LAG_INCR_TX_AGGR_STATS_DISBALE) &&
	    (lag_config->incr_tx_aggr_stats !=
	    VXGE_HAL_LAG_INCR_TX_AGGR_STATS_ENABLE) &&
	    (lag_config->incr_tx_aggr_stats !=
	    VXGE_HAL_LAG_INCR_TX_AGGR_STATS_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_TX_AGGR_STATS);

	for (i = 0; i < VXGE_HAL_LAG_PORT_MAX_PORTS; i++) {

		if ((status = __hal_device_lag_port_config_check(
		    &lag_config->port_config[i])) != VXGE_HAL_OK)
			return (status);

	}

	for (i = 0; i < VXGE_HAL_LAG_AGGR_MAX_PORTS; i++) {

		if ((status = __hal_device_lag_aggr_config_check(
		    &lag_config->aggr_config[i])) != VXGE_HAL_OK)
			return (status);

	}

	if ((lag_config->sys_pri > VXGE_HAL_LAG_MAX_SYS_PRI) &&
	    (lag_config->sys_pri != VXGE_HAL_LAG_DEF_SYS_PRI))
		return (VXGE_HAL_BADCFG_LAG_SYS_PRI);

	if ((lag_config->use_port_mac_addr !=
	    VXGE_HAL_LAG_USE_PORT_MAC_ADDR_DISBALE) &&
	    (lag_config->use_port_mac_addr !=
	    VXGE_HAL_LAG_USE_PORT_MAC_ADDR_ENABLE) &&
	    (lag_config->use_port_mac_addr !=
	    VXGE_HAL_LAG_USE_PORT_MAC_ADDR_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_USE_PORT_MAC_ADDR);

	if ((lag_config->mac_addr_sel !=
	    VXGE_HAL_LAG_MAC_ADDR_SEL_PORT_0) &&
	    (lag_config->mac_addr_sel !=
	    VXGE_HAL_LAG_MAC_ADDR_SEL_PORT_1) &&
	    (lag_config->mac_addr_sel !=
	    VXGE_HAL_LAG_MAC_ADDR_SEL_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_MAC_ADDR_SEL);

	if ((lag_config->fast_per_time > VXGE_HAL_LAG_MAX_FAST_PER_TIME) &&
	    (lag_config->fast_per_time != VXGE_HAL_LAG_DEF_FAST_PER_TIME))
		return (VXGE_HAL_BADCFG_LAG_FAST_PER_TIME);

	if ((lag_config->slow_per_time > VXGE_HAL_LAG_MAX_SLOW_PER_TIME) &&
	    (lag_config->slow_per_time != VXGE_HAL_LAG_DEF_SLOW_PER_TIME))
		return (VXGE_HAL_BADCFG_LAG_SLOW_PER_TIME);

	if ((lag_config->short_timeout > VXGE_HAL_LAG_MAX_SHORT_TIMEOUT) &&
	    (lag_config->short_timeout != VXGE_HAL_LAG_DEF_SHORT_TIMEOUT))
		return (VXGE_HAL_BADCFG_LAG_SHORT_TIMEOUT);

	if ((lag_config->long_timeout > VXGE_HAL_LAG_MAX_LONG_TIMEOUT) &&
	    (lag_config->long_timeout != VXGE_HAL_LAG_DEF_LONG_TIMEOUT))
		return (VXGE_HAL_BADCFG_LAG_LONG_TIMEOUT);

	if ((lag_config->churn_det_time > VXGE_HAL_LAG_MAX_CHURN_DET_TIME) &&
	    (lag_config->churn_det_time != VXGE_HAL_LAG_DEF_CHURN_DET_TIME))
		return (VXGE_HAL_BADCFG_LAG_CHURN_DET_TIME);

	if ((lag_config->aggr_wait_time > VXGE_HAL_LAG_MAX_AGGR_WAIT_TIME) &&
	    (lag_config->aggr_wait_time != VXGE_HAL_LAG_DEF_AGGR_WAIT_TIME))
		return (VXGE_HAL_BADCFG_LAG_AGGR_WAIT_TIME);

	if ((lag_config->short_timer_scale !=
	    VXGE_HAL_LAG_SHORT_TIMER_SCALE_1X) &&
	    (lag_config->short_timer_scale !=
	    VXGE_HAL_LAG_SHORT_TIMER_SCALE_10X) &&
	    (lag_config->short_timer_scale !=
	    VXGE_HAL_LAG_SHORT_TIMER_SCALE_100X) &&
	    (lag_config->short_timer_scale !=
	    VXGE_HAL_LAG_SHORT_TIMER_SCALE_1000X) &&
	    (lag_config->short_timer_scale !=
	    VXGE_HAL_LAG_SHORT_TIMER_SCALE_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_SHORT_TIMER_SCALE);

	if ((lag_config->long_timer_scale !=
	    VXGE_HAL_LAG_LONG_TIMER_SCALE_1X) &&
	    (lag_config->long_timer_scale !=
	    VXGE_HAL_LAG_LONG_TIMER_SCALE_10X) &&
	    (lag_config->long_timer_scale !=
	    VXGE_HAL_LAG_LONG_TIMER_SCALE_100X) &&
	    (lag_config->long_timer_scale !=
	    VXGE_HAL_LAG_LONG_TIMER_SCALE_1000X) &&
	    (lag_config->long_timer_scale !=
	    VXGE_HAL_LAG_LONG_TIMER_SCALE_10000X) &&
	    (lag_config->long_timer_scale !=
	    VXGE_HAL_LAG_LONG_TIMER_SCALE_100000X) &&
	    (lag_config->long_timer_scale !=
	    VXGE_HAL_LAG_LONG_TIMER_SCALE_1000000X) &&
	    (lag_config->long_timer_scale !=
	    VXGE_HAL_LAG_LONG_TIMER_SCALE_DEFAULT))
		return (VXGE_HAL_BADCFG_LAG_LONG_TIMER_SCALE);

	return (VXGE_HAL_OK);
}

/*
 * __hal_vpath_qos_config_check - Check vpath QOS configuration.
 * @config: Vpath QOS configuration information
 *
 * Check the vpath QOS configuration
 *
 * Returns: VXGE_HAL_OK - success,
 * otherwise one of the vxge_hal_status_e {} enumerated error codes.
 *
 */
vxge_hal_status_e
__hal_vpath_qos_config_check(vxge_hal_vpath_qos_config_t *config)
{
	if ((config->priority > VXGE_HAL_VPATH_QOS_PRIORITY_MAX) &&
	    (config->priority != VXGE_HAL_VPATH_QOS_PRIORITY_DEFAULT))
		return (VXGE_HAL_BADCFG_VPATH_QOS_PRIORITY);

	if ((config->min_bandwidth >
	    VXGE_HAL_VPATH_QOS_MIN_BANDWIDTH_MAX) &&
	    (config->min_bandwidth !=
	    VXGE_HAL_VPATH_QOS_MIN_BANDWIDTH_DEFAULT))
		return (VXGE_HAL_BADCFG_VPATH_QOS_MIN_BANDWIDTH);

	if ((config->max_bandwidth >
	    VXGE_HAL_VPATH_QOS_MAX_BANDWIDTH_MAX) &&
	    (config->max_bandwidth !=
	    VXGE_HAL_VPATH_QOS_MAX_BANDWIDTH_DEFAULT))
		return (VXGE_HAL_BADCFG_VPATH_QOS_MAX_BANDWIDTH);

	return (VXGE_HAL_OK);
}

/*
 * __hal_mrpcim_config_check - Check mrpcim configuration.
 * @config: mrpcim configuration information
 *
 * Check the mrpcim configuration
 *
 * Returns: VXGE_HAL_OK - success,
 * otherwise one of the vxge_hal_status_e {} enumerated error codes.
 *
 */
vxge_hal_status_e
__hal_mrpcim_config_check(vxge_hal_mrpcim_config_t *config)
{
	u32 i;
	vxge_hal_status_e status = VXGE_HAL_OK;

	if ((status = __hal_device_mac_config_check(&config->mac_config)) !=
	    VXGE_HAL_OK)
		return (status);

	if ((status = __hal_device_lag_config_check(&config->lag_config)) !=
	    VXGE_HAL_OK)
		return (status);

	for (i = 0; i < VXGE_HAL_MAX_VIRTUAL_PATHS; i++) {

		if ((status = __hal_vpath_qos_config_check(
		    &config->vp_qos[i])) != VXGE_HAL_OK)
			return (status);

	}

	return (VXGE_HAL_OK);
}

/*
 * __hal_device_ring_config_check - Check ring configuration.
 * @ring_config: Device configuration information
 *
 * Check the ring configuration
 *
 * Returns: VXGE_HAL_OK - success,
 * otherwise one of the vxge_hal_status_e {} enumerated error codes.
 *
 */
vxge_hal_status_e
__hal_device_ring_config_check(vxge_hal_ring_config_t *ring_config)
{
	if ((ring_config->enable != VXGE_HAL_RING_ENABLE) &&
	    (ring_config->enable != VXGE_HAL_RING_DISABLE))
		return (VXGE_HAL_BADCFG_RING_ENABLE);

	if ((ring_config->ring_length < VXGE_HAL_MIN_RING_LENGTH) ||
	    (ring_config->ring_length > VXGE_HAL_MAX_RING_LENGTH))
		return (VXGE_HAL_BADCFG_RING_LENGTH);

	if ((ring_config->buffer_mode < VXGE_HAL_RING_RXD_BUFFER_MODE_1) ||
	    (ring_config->buffer_mode > VXGE_HAL_RING_RXD_BUFFER_MODE_5))
		return (VXGE_HAL_BADCFG_RING_RXD_BUFFER_MODE);

	if ((ring_config->scatter_mode != VXGE_HAL_RING_SCATTER_MODE_A) &&
	    (ring_config->scatter_mode != VXGE_HAL_RING_SCATTER_MODE_B) &&
	    (ring_config->scatter_mode != VXGE_HAL_RING_SCATTER_MODE_C) &&
	    (ring_config->scatter_mode !=
	    VXGE_HAL_RING_SCATTER_MODE_USE_FLASH_DEFAULT))
		return (VXGE_HAL_BADCFG_RING_SCATTER_MODE);

	if ((ring_config->post_mode != VXGE_HAL_RING_POST_MODE_LEGACY) &&
	    (ring_config->post_mode != VXGE_HAL_RING_POST_MODE_DOORBELL) &&
	    (ring_config->post_mode !=
	    VXGE_HAL_RING_POST_MODE_USE_FLASH_DEFAULT))
		return (VXGE_HAL_BADCFG_RING_POST_MODE);

	if ((ring_config->max_frm_len > VXGE_HAL_MAX_RING_MAX_FRM_LEN) &&
	    (ring_config->max_frm_len != VXGE_HAL_MAX_RING_FRM_LEN_USE_MTU))
		return (VXGE_HAL_BADCFG_RING_MAX_FRM_LEN);

	if ((ring_config->no_snoop_bits > VXGE_HAL_RING_NO_SNOOP_ALL) &&
	    (ring_config->no_snoop_bits !=
	    VXGE_HAL_RING_NO_SNOOP_USE_FLASH_DEFAULT))
		return (VXGE_HAL_BADCFG_RING_NO_SNOOP_ALL);

	if ((ring_config->rx_timer_val > VXGE_HAL_RING_MAX_RX_TIMER_VAL) &&
	    (ring_config->rx_timer_val !=
	    VXGE_HAL_RING_USE_FLASH_DEFAULT_RX_TIMER_VAL))
		return (VXGE_HAL_BADCFG_RING_TIMER_VAL);

	if ((ring_config->greedy_return !=
	    VXGE_HAL_RING_GREEDY_RETURN_ENABLE) &&
	    (ring_config->greedy_return !=
	    VXGE_HAL_RING_GREEDY_RETURN_DISABLE) &&
	    (ring_config->greedy_return !=
	    VXGE_HAL_RING_GREEDY_RETURN_USE_FLASH_DEFAULT))
		return (VXGE_HAL_BADCFG_RING_GREEDY_RETURN);

	if ((ring_config->rx_timer_ci !=
	    VXGE_HAL_RING_RX_TIMER_CI_ENABLE) &&
	    (ring_config->rx_timer_ci !=
	    VXGE_HAL_RING_RX_TIMER_CI_DISABLE) &&
	    (ring_config->rx_timer_ci !=
	    VXGE_HAL_RING_RX_TIMER_CI_USE_FLASH_DEFAULT))
		return (VXGE_HAL_BADCFG_RING_TIMER_CI);

	if (((ring_config->backoff_interval_us <
	    VXGE_HAL_MIN_BACKOFF_INTERVAL_US) ||
	    (ring_config->backoff_interval_us >
	    VXGE_HAL_MAX_BACKOFF_INTERVAL_US)) &&
	    (ring_config->backoff_interval_us !=
	    VXGE_HAL_USE_FLASH_DEFAULT_BACKOFF_INTERVAL_US))
		return (VXGE_HAL_BADCFG_RING_BACKOFF_INTERVAL_US);

	if ((ring_config->indicate_max_pkts <
	    VXGE_HAL_MIN_RING_INDICATE_MAX_PKTS) ||
	    (ring_config->indicate_max_pkts >
	    VXGE_HAL_MAX_RING_INDICATE_MAX_PKTS))
		return (VXGE_HAL_BADCFG_RING_INDICATE_MAX_PKTS);

	return (VXGE_HAL_OK);
}

/*
 * __hal_device_fifo_config_check - Check fifo configuration.
 * @fifo_config: Fifo configuration information
 *
 * Check the fifo configuration
 *
 * Returns: VXGE_HAL_OK - success,
 * otherwise one of the vxge_hal_status_e {} enumerated error codes.
 *
 */
vxge_hal_status_e
__hal_device_fifo_config_check(vxge_hal_fifo_config_t *fifo_config)
{
	if ((fifo_config->enable != VXGE_HAL_FIFO_ENABLE) &&
	    (fifo_config->enable != VXGE_HAL_FIFO_DISABLE))
		return (VXGE_HAL_BADCFG_FIFO_ENABLE);

	if ((fifo_config->fifo_length < VXGE_HAL_MIN_FIFO_LENGTH) ||
	    (fifo_config->fifo_length > VXGE_HAL_MAX_FIFO_LENGTH))
		return (VXGE_HAL_BADCFG_FIFO_LENGTH);

	if ((fifo_config->max_frags < VXGE_HAL_MIN_FIFO_FRAGS) ||
	    (fifo_config->max_frags > VXGE_HAL_MAX_FIFO_FRAGS))
		return (VXGE_HAL_BADCFG_FIFO_FRAGS);

	if (fifo_config->alignment_size > VXGE_HAL_MAX_FIFO_ALIGNMENT_SIZE)
		return (VXGE_HAL_BADCFG_FIFO_ALIGNMENT_SIZE);

	if (fifo_config->max_aligned_frags > fifo_config->max_frags)
		return (VXGE_HAL_BADCFG_FIFO_MAX_FRAGS);

	if ((fifo_config->intr != VXGE_HAL_FIFO_QUEUE_INTR_ENABLE) &&
	    (fifo_config->intr != VXGE_HAL_FIFO_QUEUE_INTR_DISABLE))
		return (VXGE_HAL_BADCFG_FIFO_QUEUE_INTR);

	if (fifo_config->no_snoop_bits > VXGE_HAL_FIFO_NO_SNOOP_ALL)
		return (VXGE_HAL_BADCFG_FIFO_NO_SNOOP_ALL);

	return (VXGE_HAL_OK);
}


/*
 * __hal_device_tim_intr_config_check - Check tim intr configuration.
 * @tim_intr_config: tim intr configuration information
 *
 * Check the tim intr configuration
 *
 * Returns: VXGE_HAL_OK - success,
 * otherwise one of the vxge_hal_status_e {} enumerated error codes.
 *
 */
vxge_hal_status_e
__hal_device_tim_intr_config_check(vxge_hal_tim_intr_config_t *tim_intr_config)
{
	if ((tim_intr_config->intr_enable != VXGE_HAL_TIM_INTR_ENABLE) &&
	    (tim_intr_config->intr_enable != VXGE_HAL_TIM_INTR_DISABLE))
		return (VXGE_HAL_BADCFG_TIM_INTR_ENABLE);

	if ((tim_intr_config->btimer_val >
	    VXGE_HAL_MAX_TIM_BTIMER_VAL) &&
	    (tim_intr_config->btimer_val !=
	    VXGE_HAL_USE_FLASH_DEFAULT_TIM_BTIMER_VAL))
		return (VXGE_HAL_BADCFG_TIM_BTIMER_VAL);

	if ((tim_intr_config->timer_ac_en !=
	    VXGE_HAL_TIM_TIMER_AC_ENABLE) &&
	    (tim_intr_config->timer_ac_en !=
	    VXGE_HAL_TIM_TIMER_AC_DISABLE) &&
	    (tim_intr_config->timer_ac_en !=
	    VXGE_HAL_TIM_TIMER_AC_USE_FLASH_DEFAULT))
		return (VXGE_HAL_BADCFG_TIM_TIMER_AC_EN);

	if ((tim_intr_config->timer_ci_en !=
	    VXGE_HAL_TIM_TIMER_CI_ENABLE) &&
	    (tim_intr_config->timer_ci_en !=
	    VXGE_HAL_TIM_TIMER_CI_DISABLE) &&
	    (tim_intr_config->timer_ci_en !=
	    VXGE_HAL_TIM_TIMER_CI_USE_FLASH_DEFAULT))
		return (VXGE_HAL_BADCFG_TIM_TIMER_CI_EN);

	if ((tim_intr_config->timer_ri_en !=
	    VXGE_HAL_TIM_TIMER_RI_ENABLE) &&
	    (tim_intr_config->timer_ri_en !=
	    VXGE_HAL_TIM_TIMER_RI_DISABLE) &&
	    (tim_intr_config->timer_ri_en !=
	    VXGE_HAL_TIM_TIMER_RI_USE_FLASH_DEFAULT))
		return (VXGE_HAL_BADCFG_TIM_TIMER_RI_EN);

	if ((tim_intr_config->rtimer_event_sf >
	    VXGE_HAL_MAX_TIM_RTIMER_EVENT_SF) &&
	    (tim_intr_config->rtimer_event_sf !=
	    VXGE_HAL_USE_FLASH_DEFAULT_TIM_RTIMER_EVENT_SF))
		return (VXGE_HAL_BADCFG_TIM_BTIMER_EVENT_SF);

	if ((tim_intr_config->rtimer_val >
	    VXGE_HAL_MAX_TIM_RTIMER_VAL) &&
	    (tim_intr_config->rtimer_val !=
	    VXGE_HAL_USE_FLASH_DEFAULT_TIM_RTIMER_VAL))
		return (VXGE_HAL_BADCFG_TIM_RTIMER_VAL);

	if ((((tim_intr_config->util_sel > 19) &&
	    (tim_intr_config->util_sel < 32)) ||
	    ((tim_intr_config->util_sel > 48) &&
	    (tim_intr_config->util_sel < 63))) &&
	    (tim_intr_config->util_sel !=
	    VXGE_HAL_TIM_UTIL_SEL_USE_FLASH_DEFAULT))
		return (VXGE_HAL_BADCFG_TIM_UTIL_SEL);

	if ((tim_intr_config->ltimer_val >
	    VXGE_HAL_MAX_TIM_LTIMER_VAL) &&
	    (tim_intr_config->ltimer_val !=
	    VXGE_HAL_USE_FLASH_DEFAULT_TIM_LTIMER_VAL))
		return (VXGE_HAL_BADCFG_TIM_LTIMER_VAL);

	if ((tim_intr_config->txfrm_cnt_en !=
	    VXGE_HAL_TXFRM_CNT_EN_ENABLE) &&
	    (tim_intr_config->txfrm_cnt_en !=
	    VXGE_HAL_TXFRM_CNT_EN_DISABLE) &&
	    (tim_intr_config->txfrm_cnt_en !=
	    VXGE_HAL_TXFRM_CNT_EN_USE_FLASH_DEFAULT))
		return (VXGE_HAL_BADCFG_TXFRM_CNT_EN);

	if ((tim_intr_config->txd_cnt_en !=
	    VXGE_HAL_TXD_CNT_EN_ENABLE) &&
	    (tim_intr_config->txd_cnt_en !=
	    VXGE_HAL_TXD_CNT_EN_DISABLE) &&
	    (tim_intr_config->txd_cnt_en !=
	    VXGE_HAL_TXD_CNT_EN_USE_FLASH_DEFAULT))
		return (VXGE_HAL_BADCFG_TXD_CNT_EN);

	if ((tim_intr_config->urange_a >
	    VXGE_HAL_MAX_TIM_URANGE_A) &&
	    (tim_intr_config->urange_a !=
	    VXGE_HAL_USE_FLASH_DEFAULT_TIM_URANGE_A))
		return (VXGE_HAL_BADCFG_TIM_URANGE_A);

	if ((tim_intr_config->uec_a >
	    VXGE_HAL_MAX_TIM_UEC_A) &&
	    (tim_intr_config->uec_a !=
	    VXGE_HAL_USE_FLASH_DEFAULT_TIM_UEC_A))
		return (VXGE_HAL_BADCFG_TIM_UEC_A);

	if ((tim_intr_config->urange_b >
	    VXGE_HAL_MAX_TIM_URANGE_B) &&
	    (tim_intr_config->urange_b !=
	    VXGE_HAL_USE_FLASH_DEFAULT_TIM_URANGE_B))
		return (VXGE_HAL_BADCFG_TIM_URANGE_B);

	if ((tim_intr_config->uec_b >
	    VXGE_HAL_MAX_TIM_UEC_B) &&
	    (tim_intr_config->uec_b !=
	    VXGE_HAL_USE_FLASH_DEFAULT_TIM_UEC_B))
		return (VXGE_HAL_BADCFG_TIM_UEC_B);

	if ((tim_intr_config->urange_c >
	    VXGE_HAL_MAX_TIM_URANGE_C) &&
	    (tim_intr_config->urange_c !=
	    VXGE_HAL_USE_FLASH_DEFAULT_TIM_URANGE_C))
		return (VXGE_HAL_BADCFG_TIM_URANGE_C);

	if ((tim_intr_config->uec_c >
	    VXGE_HAL_MAX_TIM_UEC_C) &&
	    (tim_intr_config->uec_c !=
	    VXGE_HAL_USE_FLASH_DEFAULT_TIM_UEC_C))
		return (VXGE_HAL_BADCFG_TIM_UEC_C);

	if ((tim_intr_config->uec_d >
	    VXGE_HAL_MAX_TIM_UEC_D) &&
	    (tim_intr_config->uec_d !=
	    VXGE_HAL_USE_FLASH_DEFAULT_TIM_UEC_D))
		return (VXGE_HAL_BADCFG_TIM_UEC_D);

	if (((tim_intr_config->ufca_intr_thres <
	    VXGE_HAL_MIN_UFCA_INTR_THRES) ||
	    (tim_intr_config->ufca_intr_thres >
	    VXGE_HAL_MAX_UFCA_INTR_THRES)) &&
	    (tim_intr_config->ufca_intr_thres !=
	    VXGE_HAL_USE_FLASH_DEFAULT_UFCA_INTR_THRES))
		return (VXGE_HAL_BADCFG_UFCA_INTR_THRES);

	if (((tim_intr_config->ufca_lo_lim <
	    VXGE_HAL_MIN_UFCA_LO_LIM) ||
	    (tim_intr_config->ufca_lo_lim >
	    VXGE_HAL_MAX_UFCA_LO_LIM)) &&
	    (tim_intr_config->ufca_lo_lim !=
	    VXGE_HAL_USE_FLASH_DEFAULT_UFCA_LO_LIM))
		return (VXGE_HAL_BADCFG_UFCA_LO_LIM);

	if (((tim_intr_config->ufca_hi_lim <
	    VXGE_HAL_MIN_UFCA_HI_LIM) ||
	    (tim_intr_config->ufca_hi_lim >
	    VXGE_HAL_MAX_UFCA_HI_LIM)) &&
	    (tim_intr_config->ufca_hi_lim !=
	    VXGE_HAL_USE_FLASH_DEFAULT_UFCA_HI_LIM))
		return (VXGE_HAL_BADCFG_UFCA_HI_LIM);

	if (((tim_intr_config->ufca_lbolt_period <
	    VXGE_HAL_MIN_UFCA_LBOLT_PERIOD) ||
	    (tim_intr_config->ufca_lbolt_period >
	    VXGE_HAL_MAX_UFCA_LBOLT_PERIOD)) &&
	    (tim_intr_config->ufca_lbolt_period !=
	    VXGE_HAL_USE_FLASH_DEFAULT_UFCA_LBOLT_PERIOD))
		return (VXGE_HAL_BADCFG_UFCA_LBOLT_PERIOD);

	return (VXGE_HAL_OK);
}

/*
 * __hal_device_vpath_config_check - Check vpath configuration.
 * @vp_config: Vpath configuration information
 *
 * Check the vpath configuration
 *
 * Returns: VXGE_HAL_OK - success,
 * otherwise one of the vxge_hal_status_e {} enumerated error codes.
 *
 */
vxge_hal_status_e
__hal_device_vpath_config_check(vxge_hal_vp_config_t *vp_config)
{
	vxge_hal_status_e status;

	if (vp_config->vp_id > VXGE_HAL_MAX_VIRTUAL_PATHS)
		return (VXGE_HAL_BADCFG_VPATH_ID);

	if ((vp_config->wire_port != VXGE_HAL_VPATH_USE_PORT0) &&
	    (vp_config->wire_port != VXGE_HAL_VPATH_USE_PORT1) &&
	    (vp_config->wire_port != VXGE_HAL_VPATH_USE_BOTH) &&
	    (vp_config->wire_port != VXGE_HAL_VPATH_USE_DEFAULT_PORT))
		return (VXGE_HAL_BADCFG_VPATH_WIRE_PORT);

	if ((vp_config->priority != VXGE_HAL_VPATH_PRIORITY_DEFAULT) &&
	    (((int)vp_config->priority < VXGE_HAL_VPATH_PRIORITY_MIN) ||
	    (vp_config->priority > VXGE_HAL_VPATH_PRIORITY_MAX)))
		return (VXGE_HAL_BADCFG_VPATH_PRIORITY);

	if ((vp_config->bandwidth != VXGE_HAL_VPATH_BW_LIMIT_DEFAULT) &&
	    ((vp_config->bandwidth < VXGE_HAL_VPATH_BW_LIMIT_MIN) ||
	    (vp_config->bandwidth > VXGE_HAL_VPATH_BW_LIMIT_MAX)))
		return (VXGE_HAL_BADCFG_VPATH_BANDWIDTH_LIMIT);

	if ((vp_config->no_snoop != VXGE_HAL_VPATH_NO_SNOOP_ENABLE) &&
	    (vp_config->no_snoop != VXGE_HAL_VPATH_NO_SNOOP_DISABLE) &&
	    (vp_config->no_snoop != VXGE_HAL_VPATH_NO_SNOOP_USE_FLASH_DEFAULT))
		return (VXGE_HAL_BADCFG_VPATH_NO_SNOOP);

	status = __hal_device_ring_config_check(&vp_config->ring);
	if (status != VXGE_HAL_OK)
		return (status);

	status = __hal_device_fifo_config_check(&vp_config->fifo);
	if (status != VXGE_HAL_OK)
		return (status);


	status = __hal_device_tim_intr_config_check(&vp_config->tti);
	if (status != VXGE_HAL_OK)
		return (status);

	status = __hal_device_tim_intr_config_check(&vp_config->rti);
	if (status != VXGE_HAL_OK)
		return (status);

	if ((vp_config->mtu != VXGE_HAL_VPATH_USE_FLASH_DEFAULT_INITIAL_MTU) &&
	    ((vp_config->mtu < VXGE_HAL_VPATH_MIN_INITIAL_MTU) ||
	    (vp_config->mtu > VXGE_HAL_VPATH_MAX_INITIAL_MTU)))
		return (VXGE_HAL_BADCFG_VPATH_MTU);

	if ((vp_config->tpa_lsov2_en !=
	    VXGE_HAL_VPATH_TPA_LSOV2_EN_USE_FLASH_DEFAULT) &&
	    (vp_config->tpa_lsov2_en !=
	    VXGE_HAL_VPATH_TPA_LSOV2_EN_ENABLE) &&
	    (vp_config->tpa_lsov2_en !=
	    VXGE_HAL_VPATH_TPA_LSOV2_EN_DISABLE))
		return (VXGE_HAL_BADCFG_VPATH_TPA_LSOV2_EN);

	if ((vp_config->tpa_ignore_frame_error !=
	    VXGE_HAL_VPATH_TPA_IGNORE_FRAME_ERROR_USE_FLASH_DEFAULT) &&
	    (vp_config->tpa_ignore_frame_error !=
	    VXGE_HAL_VPATH_TPA_IGNORE_FRAME_ERROR_ENABLE) &&
	    (vp_config->tpa_ignore_frame_error !=
	    VXGE_HAL_VPATH_TPA_IGNORE_FRAME_ERROR_DISABLE))
		return (VXGE_HAL_BADCFG_VPATH_TPA_IGNORE_FRAME_ERROR);

	if ((vp_config->tpa_ipv6_keep_searching !=
	    VXGE_HAL_VPATH_TPA_IPV6_KEEP_SEARCHING_USE_FLASH_DEFAULT) &&
	    (vp_config->tpa_ipv6_keep_searching !=
	    VXGE_HAL_VPATH_TPA_IPV6_KEEP_SEARCHING_ENABLE) &&
	    (vp_config->tpa_ipv6_keep_searching !=
	    VXGE_HAL_VPATH_TPA_IPV6_KEEP_SEARCHING_DISABLE))
		return (VXGE_HAL_BADCFG_VPATH_TPA_IPV6_KEEP_SEARCHING);

	if ((vp_config->tpa_l4_pshdr_present !=
	    VXGE_HAL_VPATH_TPA_L4_PSHDR_PRESENT_USE_FLASH_DEFAULT) &&
	    (vp_config->tpa_l4_pshdr_present !=
	    VXGE_HAL_VPATH_TPA_L4_PSHDR_PRESENT_ENABLE) &&
	    (vp_config->tpa_l4_pshdr_present !=
	    VXGE_HAL_VPATH_TPA_L4_PSHDR_PRESENT_DISABLE))
		return (VXGE_HAL_BADCFG_VPATH_TPA_L4_PSHDR_PRESENT);

	if ((vp_config->tpa_support_mobile_ipv6_hdrs !=
	    VXGE_HAL_VPATH_TPA_SUPPORT_MOBILE_IPV6_HDRS_USE_FLASH_DEFAULT) &&
	    (vp_config->tpa_support_mobile_ipv6_hdrs !=
	    VXGE_HAL_VPATH_TPA_SUPPORT_MOBILE_IPV6_HDRS_ENABLE) &&
	    (vp_config->tpa_support_mobile_ipv6_hdrs !=
	    VXGE_HAL_VPATH_TPA_SUPPORT_MOBILE_IPV6_HDRS_DISABLE))
		return (VXGE_HAL_BADCFG_VPATH_TPA_SUPPORT_MOBILE_IPV6_HDRS);

	if ((vp_config->rpa_ipv4_tcp_incl_ph !=
	    VXGE_HAL_VPATH_RPA_IPV4_TCP_INCL_PH_USE_FLASH_DEFAULT) &&
	    (vp_config->rpa_ipv4_tcp_incl_ph !=
	    VXGE_HAL_VPATH_RPA_IPV4_TCP_INCL_PH_ENABLE) &&
	    (vp_config->rpa_ipv4_tcp_incl_ph !=
	    VXGE_HAL_VPATH_RPA_IPV4_TCP_INCL_PH_DISABLE))
		return (VXGE_HAL_BADCFG_VPATH_RPA_IPV4_TCP_INCL_PH);

	if ((vp_config->rpa_ipv6_tcp_incl_ph !=
	    VXGE_HAL_VPATH_RPA_IPV6_TCP_INCL_PH_USE_FLASH_DEFAULT) &&
	    (vp_config->rpa_ipv6_tcp_incl_ph !=
	    VXGE_HAL_VPATH_RPA_IPV6_TCP_INCL_PH_ENABLE) &&
	    (vp_config->rpa_ipv6_tcp_incl_ph !=
	    VXGE_HAL_VPATH_RPA_IPV6_TCP_INCL_PH_DISABLE))
		return (VXGE_HAL_BADCFG_VPATH_RPA_IPV6_TCP_INCL_PH);

	if ((vp_config->rpa_ipv4_udp_incl_ph !=
	    VXGE_HAL_VPATH_RPA_IPV4_UDP_INCL_PH_USE_FLASH_DEFAULT) &&
	    (vp_config->rpa_ipv4_udp_incl_ph !=
	    VXGE_HAL_VPATH_RPA_IPV4_UDP_INCL_PH_ENABLE) &&
	    (vp_config->rpa_ipv4_udp_incl_ph !=
	    VXGE_HAL_VPATH_RPA_IPV4_UDP_INCL_PH_DISABLE))
		return (VXGE_HAL_BADCFG_VPATH_RPA_IPV4_UDP_INCL_PH);

	if ((vp_config->rpa_ipv6_udp_incl_ph !=
	    VXGE_HAL_VPATH_RPA_IPV6_UDP_INCL_PH_USE_FLASH_DEFAULT) &&
	    (vp_config->rpa_ipv6_udp_incl_ph !=
	    VXGE_HAL_VPATH_RPA_IPV6_UDP_INCL_PH_ENABLE) &&
	    (vp_config->rpa_ipv6_udp_incl_ph !=
	    VXGE_HAL_VPATH_RPA_IPV6_UDP_INCL_PH_DISABLE))
		return (VXGE_HAL_BADCFG_VPATH_RPA_IPV4_UDP_INCL_PH);

	if ((vp_config->rpa_l4_incl_cf !=
	    VXGE_HAL_VPATH_RPA_L4_INCL_CF_USE_FLASH_DEFAULT) &&
	    (vp_config->rpa_l4_incl_cf !=
	    VXGE_HAL_VPATH_RPA_L4_INCL_CF_ENABLE) &&
	    (vp_config->rpa_l4_incl_cf !=
	    VXGE_HAL_VPATH_RPA_L4_INCL_CF_DISABLE))
		return (VXGE_HAL_BADCFG_VPATH_RPA_L4_INCL_CF);

	if ((vp_config->rpa_strip_vlan_tag !=
	    VXGE_HAL_VPATH_RPA_STRIP_VLAN_TAG_USE_FLASH_DEFAULT) &&
	    (vp_config->rpa_strip_vlan_tag !=
	    VXGE_HAL_VPATH_RPA_STRIP_VLAN_TAG_ENABLE) &&
	    (vp_config->rpa_strip_vlan_tag !=
	    VXGE_HAL_VPATH_RPA_STRIP_VLAN_TAG_DISABLE))
		return (VXGE_HAL_BADCFG_VPATH_RPA_STRIP_VLAN_TAG);

	if ((vp_config->rpa_l4_comp_csum !=
	    VXGE_HAL_VPATH_RPA_L4_COMP_CSUM_USE_FLASH_DEFAULT) &&
	    (vp_config->rpa_l4_comp_csum !=
	    VXGE_HAL_VPATH_RPA_L4_COMP_CSUM_ENABLE) &&
	    (vp_config->rpa_l4_comp_csum !=
	    VXGE_HAL_VPATH_RPA_L4_COMP_CSUM_DISABLE))
		return (VXGE_HAL_BADCFG_VPATH_RPA_L4_COMP_CSUM);

	if ((vp_config->rpa_l3_incl_cf !=
	    VXGE_HAL_VPATH_RPA_L3_INCL_CF_USE_FLASH_DEFAULT) &&
	    (vp_config->rpa_l3_incl_cf !=
	    VXGE_HAL_VPATH_RPA_L3_INCL_CF_ENABLE) &&
	    (vp_config->rpa_l3_incl_cf !=
	    VXGE_HAL_VPATH_RPA_L3_INCL_CF_DISABLE))
		return (VXGE_HAL_BADCFG_VPATH_RPA_L3_INCL_CF);

	if ((vp_config->rpa_l3_comp_csum !=
	    VXGE_HAL_VPATH_RPA_L3_COMP_CSUM_USE_FLASH_DEFAULT) &&
	    (vp_config->rpa_l3_comp_csum !=
	    VXGE_HAL_VPATH_RPA_L3_COMP_CSUM_ENABLE) &&
	    (vp_config->rpa_l3_comp_csum !=
	    VXGE_HAL_VPATH_RPA_L3_COMP_CSUM_DISABLE))
		return (VXGE_HAL_BADCFG_VPATH_RPA_L3_COMP_CSUM);

	if ((vp_config->rpa_ucast_all_addr_en !=
	    VXGE_HAL_VPATH_RPA_UCAST_ALL_ADDR_USE_FLASH_DEFAULT) &&
	    (vp_config->rpa_ucast_all_addr_en !=
	    VXGE_HAL_VPATH_RPA_UCAST_ALL_ADDR_ENABLE) &&
	    (vp_config->rpa_ucast_all_addr_en !=
	    VXGE_HAL_VPATH_RPA_UCAST_ALL_ADDR_DISABLE))
		return (VXGE_HAL_BADCFG_VPATH_RPA_UCAST_ALL_ADDR_EN);

	if ((vp_config->rpa_mcast_all_addr_en !=
	    VXGE_HAL_VPATH_RPA_MCAST_ALL_ADDR_USE_FLASH_DEFAULT) &&
	    (vp_config->rpa_mcast_all_addr_en !=
	    VXGE_HAL_VPATH_RPA_MCAST_ALL_ADDR_ENABLE) &&
	    (vp_config->rpa_mcast_all_addr_en !=
	    VXGE_HAL_VPATH_RPA_MCAST_ALL_ADDR_DISABLE))
		return (VXGE_HAL_BADCFG_VPATH_RPA_MCAST_ALL_ADDR_EN);

	if ((vp_config->rpa_bcast_en !=
	    VXGE_HAL_VPATH_RPA_BCAST_USE_FLASH_DEFAULT) &&
	    (vp_config->rpa_bcast_en !=
	    VXGE_HAL_VPATH_RPA_BCAST_ENABLE) &&
	    (vp_config->rpa_bcast_en !=
	    VXGE_HAL_VPATH_RPA_BCAST_DISABLE))
		return (VXGE_HAL_BADCFG_VPATH_RPA_CAST_EN);

	if ((vp_config->rpa_all_vid_en !=
	    VXGE_HAL_VPATH_RPA_ALL_VID_USE_FLASH_DEFAULT) &&
	    (vp_config->rpa_all_vid_en !=
	    VXGE_HAL_VPATH_RPA_ALL_VID_ENABLE) &&
	    (vp_config->rpa_all_vid_en !=
	    VXGE_HAL_VPATH_RPA_ALL_VID_DISABLE))
		return (VXGE_HAL_BADCFG_VPATH_RPA_ALL_VID_EN);

	if ((vp_config->vp_queue_l2_flow !=
	    VXGE_HAL_VPATH_VP_Q_L2_FLOW_ENABLE) &&
	    (vp_config->vp_queue_l2_flow !=
	    VXGE_HAL_VPATH_VP_Q_L2_FLOW_DISABLE) &&
	    (vp_config->vp_queue_l2_flow !=
	    VXGE_HAL_VPATH_VP_Q_L2_FLOW_USE_FLASH_DEFAULT))
		return (VXGE_HAL_BADCFG_VPATH_VP_Q_L2_FLOW);

	return (VXGE_HAL_OK);
}

/*
 * __hal_device_config_check - Check device configuration.
 * @new_config: Device configuration information
 *
 * Check the device configuration
 *
 * Returns: VXGE_HAL_OK - success,
 * otherwise one of the vxge_hal_status_e {} enumerated error codes.
 *
 */
vxge_hal_status_e
__hal_device_config_check(vxge_hal_device_config_t *new_config)
{
	u32 i;
	vxge_hal_status_e status;

	if (new_config->dma_blockpool_incr <
	    VXGE_HAL_INCR_DMA_BLOCK_POOL_SIZE)
		return (VXGE_HAL_BADCFG_BLOCKPOOL_INCR);

	if (new_config->dma_blockpool_max <
	    VXGE_HAL_MAX_DMA_BLOCK_POOL_SIZE)
		return (VXGE_HAL_BADCFG_BLOCKPOOL_MAX);

	if ((status = __hal_mrpcim_config_check(&new_config->mrpcim_config)) !=
	    VXGE_HAL_OK)
		return (status);

	if (new_config->isr_polling_cnt > VXGE_HAL_MAX_ISR_POLLING_CNT)
		return (VXGE_HAL_BADCFG_ISR_POLLING_CNT);

	if ((new_config->max_payload_size >
	    VXGE_HAL_MAX_PAYLOAD_SIZE_4096) &&
	    (new_config->max_payload_size !=
	    VXGE_HAL_USE_BIOS_DEFAULT_PAYLOAD_SIZE))
		return (VXGE_HAL_BADCFG_MAX_PAYLOAD_SIZE);

	if ((new_config->mmrb_count > VXGE_HAL_MMRB_COUNT_4096) &&
	    (new_config->mmrb_count != VXGE_HAL_USE_BIOS_DEFAULT_MMRB_COUNT))
		return (VXGE_HAL_BADCFG_MAX_PAYLOAD_SIZE);

	if (((new_config->stats_refresh_time_sec <
	    VXGE_HAL_MIN_STATS_REFRESH_TIME) ||
	    (new_config->stats_refresh_time_sec >
	    VXGE_HAL_MAX_STATS_REFRESH_TIME)) &&
	    (new_config->stats_refresh_time_sec !=
	    VXGE_HAL_STATS_REFRESH_DISABLE))
		return (VXGE_HAL_BADCFG_STATS_REFRESH_TIME);

	if ((new_config->intr_mode != VXGE_HAL_INTR_MODE_IRQLINE) &&
	    (new_config->intr_mode != VXGE_HAL_INTR_MODE_MSIX) &&
	    (new_config->intr_mode != VXGE_HAL_INTR_MODE_MSIX_ONE_SHOT) &&
	    (new_config->intr_mode != VXGE_HAL_INTR_MODE_EMULATED_INTA) &&
	    (new_config->intr_mode != VXGE_HAL_INTR_MODE_DEF))
		return (VXGE_HAL_BADCFG_INTR_MODE);

	if ((new_config->dump_on_unknown !=
	    VXGE_HAL_DUMP_ON_UNKNOWN_DISABLE) &&
	    (new_config->dump_on_unknown !=
	    VXGE_HAL_DUMP_ON_UNKNOWN_ENABLE))
		return (VXGE_HAL_BADCFG_DUMP_ON_UNKNOWN);

	if ((new_config->dump_on_serr != VXGE_HAL_DUMP_ON_SERR_DISABLE) &&
	    (new_config->dump_on_serr != VXGE_HAL_DUMP_ON_SERR_ENABLE))
		return (VXGE_HAL_BADCFG_DUMP_ON_SERR);

	if ((new_config->dump_on_critical !=
	    VXGE_HAL_DUMP_ON_CRITICAL_DISABLE) &&
	    (new_config->dump_on_critical !=
	    VXGE_HAL_DUMP_ON_CRITICAL_ENABLE))
		return (VXGE_HAL_BADCFG_DUMP_ON_CRITICAL);

	if ((new_config->dump_on_eccerr != VXGE_HAL_DUMP_ON_ECCERR_DISABLE) &&
	    (new_config->dump_on_eccerr != VXGE_HAL_DUMP_ON_ECCERR_ENABLE))
		return (VXGE_HAL_BADCFG_DUMP_ON_ECCERR);

	if ((new_config->rth_en != VXGE_HAL_RTH_DISABLE) &&
	    (new_config->rth_en != VXGE_HAL_RTH_ENABLE))
		return (VXGE_HAL_BADCFG_RTH_EN);

	if ((new_config->rth_it_type != VXGE_HAL_RTH_IT_TYPE_SOLO_IT) &&
	    (new_config->rth_it_type != VXGE_HAL_RTH_IT_TYPE_MULTI_IT))
		return (VXGE_HAL_BADCFG_RTH_IT_TYPE);

	if ((new_config->rts_mac_en != VXGE_HAL_RTS_MAC_DISABLE) &&
	    (new_config->rts_mac_en != VXGE_HAL_RTS_MAC_ENABLE))
		return (VXGE_HAL_BADCFG_RTS_MAC_EN);

	if ((new_config->rts_qos_en != VXGE_HAL_RTS_QOS_DISABLE) &&
	    (new_config->rts_qos_en != VXGE_HAL_RTS_QOS_ENABLE))
		return (VXGE_HAL_BADCFG_RTS_QOS_EN);

	if ((new_config->rts_port_en != VXGE_HAL_RTS_PORT_DISABLE) &&
	    (new_config->rts_port_en != VXGE_HAL_RTS_PORT_ENABLE))
		return (VXGE_HAL_BADCFG_RTS_PORT_EN);

	for (i = 0; i < VXGE_HAL_MAX_VIRTUAL_PATHS; i++) {
		if ((status = __hal_device_vpath_config_check(
		    &new_config->vp_config[i])) != VXGE_HAL_OK)
			return (status);
	}

	if ((new_config->max_cqe_groups < VXGE_HAL_MIN_MAX_CQE_GROUPS) ||
	    (new_config->max_cqe_groups > VXGE_HAL_MAX_MAX_CQE_GROUPS))
		return (VXGE_HAL_BADCFG_MAX_CQE_GROUPS);

	if ((new_config->max_num_wqe_od_groups <
	    VXGE_HAL_MIN_MAX_NUM_OD_GROUPS) ||
	    (new_config->max_num_wqe_od_groups >
	    VXGE_HAL_MAX_MAX_NUM_OD_GROUPS))
		return (VXGE_HAL_BADCFG_MAX_NUM_OD_GROUPS);

	if ((new_config->no_wqe_threshold < VXGE_HAL_MIN_NO_WQE_THRESHOLD) ||
	    (new_config->no_wqe_threshold > VXGE_HAL_MAX_NO_WQE_THRESHOLD))
		return (VXGE_HAL_BADCFG_NO_WQE_THRESHOLD);

	if ((new_config->refill_threshold_high <
	    VXGE_HAL_MIN_REFILL_THRESHOLD_HIGH) ||
	    (new_config->refill_threshold_high >
	    VXGE_HAL_MAX_REFILL_THRESHOLD_HIGH))
		return (VXGE_HAL_BADCFG_REFILL_THRESHOLD_HIGH);

	if ((new_config->refill_threshold_low <
	    VXGE_HAL_MIN_REFILL_THRESHOLD_LOW) ||
	    (new_config->refill_threshold_low >
	    VXGE_HAL_MAX_REFILL_THRESHOLD_LOW))
		return (VXGE_HAL_BADCFG_REFILL_THRESHOLD_LOW);

	if ((new_config->ack_blk_limit < VXGE_HAL_MIN_ACK_BLOCK_LIMIT) ||
	    (new_config->ack_blk_limit > VXGE_HAL_MAX_ACK_BLOCK_LIMIT))
		return (VXGE_HAL_BADCFG_ACK_BLOCK_LIMIT);

	if ((new_config->stats_read_method !=
	    VXGE_HAL_STATS_READ_METHOD_DMA) &&
	    (new_config->stats_read_method !=
	    VXGE_HAL_STATS_READ_METHOD_PIO))
		return (VXGE_HAL_BADCFG_STATS_READ_METHOD);

	if ((new_config->poll_or_doorbell !=
	    VXGE_HAL_POLL_OR_DOORBELL_POLL) &&
	    (new_config->poll_or_doorbell !=
	    VXGE_HAL_POLL_OR_DOORBELL_DOORBELL))
		return (VXGE_HAL_BADCFG_POLL_OR_DOOR_BELL);

	if ((new_config->device_poll_millis <
	    VXGE_HAL_MIN_DEVICE_POLL_MILLIS) ||
	    (new_config->device_poll_millis >
	    VXGE_HAL_MAX_DEVICE_POLL_MILLIS))
		return (VXGE_HAL_BADCFG_DEVICE_POLL_MILLIS);


	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_device_config_default_get - Initialize device config with defaults.
 * @device_config: Configuration structure to be initialized,
 *	    For the X3100 configuration "knobs" please
 *	    refer to vxge_hal_device_config_t and X3100
 *	    User Guide.
 *
 * Initialize X3100 device config with default values.
 *
 * See also: vxge_hal_device_initialize(), vxge_hal_device_terminate(),
 * vxge_hal_status_e {} vxge_hal_device_attr_t {}.
 */
vxge_hal_status_e
vxge_hal_device_config_default_get(
    vxge_hal_device_config_t *device_config)
{
	u32 i;
	vxge_hal_mac_config_t *mac_config;
	vxge_hal_wire_port_config_t *wire_port_config;
	vxge_hal_switch_port_config_t *switch_port_config;

	vxge_hal_trace_log_driver("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_driver("device_config = 0x"VXGE_OS_STXFMT,
	    (ptr_t) device_config);

	device_config->dma_blockpool_min = VXGE_HAL_MIN_DMA_BLOCK_POOL_SIZE;
	device_config->dma_blockpool_initial =
	    VXGE_HAL_INITIAL_DMA_BLOCK_POOL_SIZE;
	device_config->dma_blockpool_incr = VXGE_HAL_INCR_DMA_BLOCK_POOL_SIZE;
	device_config->dma_blockpool_max = VXGE_HAL_MAX_DMA_BLOCK_POOL_SIZE;

	mac_config = &device_config->mrpcim_config.mac_config;

	for (i = 0; i < VXGE_HAL_MAC_MAX_WIRE_PORTS; i++) {

		wire_port_config = &mac_config->wire_port_config[i];

		wire_port_config->port_id = i;

		wire_port_config->media = VXGE_HAL_WIRE_PORT_MEDIA_DEFAULT;

		wire_port_config->mtu = VXGE_HAL_WIRE_PORT_DEF_INITIAL_MTU;

		wire_port_config->autoneg_mode =
		    VXGE_HAL_WIRE_PORT_AUTONEG_MODE_DEFAULT;

		wire_port_config->autoneg_rate =
		    VXGE_HAL_WIRE_PORT_AUTONEG_RATE_DEFAULT;

		wire_port_config->fixed_use_fsm =
		    VXGE_HAL_WIRE_PORT_FIXED_USE_FSM_DEFAULT;

		wire_port_config->antp_use_fsm =
		    VXGE_HAL_WIRE_PORT_ANTP_USE_FSM_DEFAULT;

		wire_port_config->anbe_use_fsm =
		    VXGE_HAL_WIRE_PORT_ANBE_USE_FSM_DEFAULT;

		wire_port_config->link_stability_period =
		    VXGE_HAL_WIRE_PORT_DEF_LINK_STABILITY_PERIOD;

		wire_port_config->port_stability_period =
		    VXGE_HAL_WIRE_PORT_DEF_PORT_STABILITY_PERIOD;

		wire_port_config->tmac_en =
		    VXGE_HAL_WIRE_PORT_TMAC_DEFAULT;

		wire_port_config->rmac_en =
		    VXGE_HAL_WIRE_PORT_RMAC_DEFAULT;

		wire_port_config->tmac_pad =
		    VXGE_HAL_WIRE_PORT_TMAC_PAD_DEFAULT;

		wire_port_config->tmac_pad_byte =
		    VXGE_HAL_WIRE_PORT_DEF_TMAC_PAD_BYTE;

		wire_port_config->tmac_util_period =
		    VXGE_HAL_WIRE_PORT_DEF_TMAC_UTIL_PERIOD;

		wire_port_config->rmac_strip_fcs =
		    VXGE_HAL_WIRE_PORT_RMAC_STRIP_FCS_DEFAULT;

		wire_port_config->rmac_prom_en =
		    VXGE_HAL_WIRE_PORT_RMAC_PROM_EN_DEFAULT;

		wire_port_config->rmac_discard_pfrm =
		    VXGE_HAL_WIRE_PORT_RMAC_DISCARD_PFRM_DEFAULT;

		wire_port_config->rmac_util_period =
		    VXGE_HAL_WIRE_PORT_DEF_RMAC_UTIL_PERIOD;

		wire_port_config->rmac_pause_gen_en =
		    VXGE_HAL_WIRE_PORT_RMAC_PAUSE_GEN_EN_DEFAULT;

		wire_port_config->rmac_pause_rcv_en =
		    VXGE_HAL_WIRE_PORT_RMAC_PAUSE_RCV_EN_DEFAULT;

		wire_port_config->rmac_pause_time =
		    VXGE_HAL_WIRE_PORT_DEF_RMAC_HIGH_PTIME;

		wire_port_config->limiter_en =
		    VXGE_HAL_WIRE_PORT_RMAC_PAUSE_LIMITER_DEFAULT;

		wire_port_config->max_limit =
		    VXGE_HAL_WIRE_PORT_DEF_RMAC_MAX_LIMIT;
	}

	switch_port_config = &mac_config->switch_port_config;

	switch_port_config->mtu =
	    VXGE_HAL_SWITCH_PORT_DEF_INITIAL_MTU;

	switch_port_config->tmac_en =
	    VXGE_HAL_SWITCH_PORT_TMAC_DEFAULT;

	switch_port_config->rmac_en =
	    VXGE_HAL_SWITCH_PORT_RMAC_DEFAULT;

	switch_port_config->tmac_pad =
	    VXGE_HAL_SWITCH_PORT_TMAC_PAD_DEFAULT;

	switch_port_config->tmac_pad_byte =
	    VXGE_HAL_SWITCH_PORT_DEF_TMAC_PAD_BYTE;

	switch_port_config->tmac_util_period =
	    VXGE_HAL_SWITCH_PORT_DEF_TMAC_UTIL_PERIOD;

	switch_port_config->rmac_strip_fcs =
	    VXGE_HAL_SWITCH_PORT_RMAC_STRIP_FCS_DEFAULT;

	switch_port_config->rmac_prom_en =
	    VXGE_HAL_SWITCH_PORT_RMAC_PROM_EN_DEFAULT;

	switch_port_config->rmac_discard_pfrm =
	    VXGE_HAL_SWITCH_PORT_RMAC_DISCARD_PFRM_DEFAULT;

	switch_port_config->rmac_util_period =
	    VXGE_HAL_SWITCH_PORT_DEF_RMAC_UTIL_PERIOD;

	switch_port_config->rmac_pause_gen_en =
	    VXGE_HAL_SWITCH_PORT_RMAC_PAUSE_GEN_EN_DEFAULT;

	switch_port_config->rmac_pause_rcv_en =
	    VXGE_HAL_SWITCH_PORT_RMAC_PAUSE_RCV_EN_DEFAULT;

	switch_port_config->rmac_pause_time =
	    VXGE_HAL_SWITCH_PORT_DEF_RMAC_HIGH_PTIME;

	switch_port_config->limiter_en =
	    VXGE_HAL_SWITCH_PORT_RMAC_PAUSE_LIMITER_DEFAULT;

	switch_port_config->max_limit =
	    VXGE_HAL_SWITCH_PORT_DEF_RMAC_MAX_LIMIT;

	mac_config->network_stability_period =
	    VXGE_HAL_MAC_DEF_NETWORK_STABILITY_PERIOD;

	for (i = 0; i < 16; i++) {

		mac_config->mc_pause_threshold[i] =
		    VXGE_HAL_MAC_DEF_MC_PAUSE_THRESHOLD;

	}

	mac_config->tmac_perma_stop_en =
	    VXGE_HAL_MAC_TMAC_PERMA_STOP_DEFAULT;

	mac_config->tmac_tx_switch_dis =
	    VXGE_HAL_MAC_TMAC_TX_SWITCH_DEFAULT;

	mac_config->tmac_lossy_switch_en =
	    VXGE_HAL_MAC_TMAC_LOSSY_SWITCH_DEFAULT;

	mac_config->tmac_lossy_wire_en =
	    VXGE_HAL_MAC_TMAC_LOSSY_WIRE_DEFAULT;

	mac_config->tmac_bcast_to_wire_dis =
	    VXGE_HAL_MAC_TMAC_BCAST_TO_WIRE_DEFAULT;

	mac_config->tmac_bcast_to_switch_dis =
	    VXGE_HAL_MAC_TMAC_BCAST_TO_SWITCH_DEFAULT;

	mac_config->tmac_host_append_fcs_en =
	    VXGE_HAL_MAC_TMAC_HOST_APPEND_FCS_DEFAULT;

	mac_config->tpa_support_snap_ab_n =
	    VXGE_HAL_MAC_TPA_SUPPORT_SNAP_AB_N_DEFAULT;

	mac_config->tpa_ecc_enable_n =
	    VXGE_HAL_MAC_TPA_ECC_ENABLE_N_DEFAULT;

	mac_config->rpa_ignore_frame_err =
	    VXGE_HAL_MAC_RPA_IGNORE_FRAME_ERR_DEFAULT;

	mac_config->rpa_support_snap_ab_n =
	    VXGE_HAL_MAC_RPA_SUPPORT_SNAP_AB_N_DEFAULT;

	mac_config->rpa_search_for_hao =
	    VXGE_HAL_MAC_RPA_SEARCH_FOR_HAO_DEFAULT;

	mac_config->rpa_support_ipv6_mobile_hdrs =
	    VXGE_HAL_MAC_RPA_SUPPORT_IPV6_MOBILE_HDRS_DEFAULT;

	mac_config->rpa_ipv6_stop_searching =
	    VXGE_HAL_MAC_RPA_IPV6_STOP_SEARCHING_DEFAULT;

	mac_config->rpa_no_ps_if_unknown =
	    VXGE_HAL_MAC_RPA_NO_PS_IF_UNKNOWN_DEFAULT;

	mac_config->rpa_search_for_etype =
	    VXGE_HAL_MAC_RPA_SEARCH_FOR_ETYPE_DEFAULT;

	mac_config->rpa_repl_l4_comp_csum =
	    VXGE_HAL_MAC_RPA_REPL_l4_COMP_CSUM_DEFAULT;

	mac_config->rpa_repl_l3_incl_cf =
	    VXGE_HAL_MAC_RPA_REPL_L3_INCL_CF_DEFAULT;

	mac_config->rpa_repl_l3_comp_csum =
	    VXGE_HAL_MAC_RPA_REPL_l3_COMP_CSUM_DEFAULT;

	mac_config->rpa_repl_ipv4_tcp_incl_ph =
	    VXGE_HAL_MAC_RPA_REPL_IPV4_TCP_INCL_PH_DEFAULT;

	mac_config->rpa_repl_ipv6_tcp_incl_ph =
	    VXGE_HAL_MAC_RPA_REPL_IPV6_TCP_INCL_PH_DEFAULT;

	mac_config->rpa_repl_ipv4_udp_incl_ph =
	    VXGE_HAL_MAC_RPA_REPL_IPV4_UDP_INCL_PH_DEFAULT;

	mac_config->rpa_repl_ipv6_udp_incl_ph =
	    VXGE_HAL_MAC_RPA_REPL_IPV6_UDP_INCL_PH_DEFAULT;

	mac_config->rpa_repl_l4_incl_cf =
	    VXGE_HAL_MAC_RPA_REPL_L4_INCL_CF_DEFAULT;

	mac_config->rpa_repl_strip_vlan_tag =
	    VXGE_HAL_MAC_RPA_REPL_STRIP_VLAN_TAG_DEFAULT;

	device_config->mrpcim_config.lag_config.lag_en =
	    VXGE_HAL_LAG_LAG_EN_DEFAULT;

	device_config->mrpcim_config.lag_config.lag_mode =
	    VXGE_HAL_LAG_LAG_MODE_DEFAULT;

	device_config->mrpcim_config.lag_config.la_mode_config.tx_discard =
	    VXGE_HAL_LAG_TX_DISCARD_DEFAULT;

	device_config->mrpcim_config.lag_config.la_mode_config.distrib_alg_sel =
	    VXGE_HAL_LAG_DISTRIB_ALG_SEL_DEFAULT;

	device_config->mrpcim_config.lag_config.la_mode_config.distrib_dest =
	    VXGE_HAL_LAG_DISTRIB_DEST_DEFAULT;

	device_config->
	    mrpcim_config.lag_config.la_mode_config.distrib_remap_if_fail =
	    VXGE_HAL_LAG_DISTRIB_REMAP_IF_FAIL_DEFAULT;

	device_config->mrpcim_config.lag_config.la_mode_config.coll_max_delay =
	    VXGE_HAL_LAG_DEF_COLL_MAX_DELAY;

	device_config->mrpcim_config.lag_config.la_mode_config.rx_discard =
	    VXGE_HAL_LAG_RX_DISCARD_DEFAULT;

	device_config->mrpcim_config.lag_config.ap_mode_config.hot_standby =
	    VXGE_HAL_LAG_HOT_STANDBY_DEFAULT;

	device_config->mrpcim_config.lag_config.ap_mode_config.lacp_decides =
	    VXGE_HAL_LAG_LACP_DECIDES_DEFAULT;

	device_config->
	    mrpcim_config.lag_config.ap_mode_config.pref_active_port =
	    VXGE_HAL_LAG_PREF_ACTIVE_PORT_DEFAULT;

	device_config->mrpcim_config.lag_config.ap_mode_config.auto_failback =
	    VXGE_HAL_LAG_AUTO_FAILBACK_DEFAULT;

	device_config->mrpcim_config.lag_config.ap_mode_config.failback_en =
	    VXGE_HAL_LAG_FAILBACK_EN_DEFAULT;

	device_config->
	    mrpcim_config.lag_config.ap_mode_config.cold_failover_timeout =
	    VXGE_HAL_LAG_DEF_COLD_FAILOVER_TIMEOUT;

	device_config->mrpcim_config.lag_config.ap_mode_config.alt_admin_key =
	    VXGE_HAL_LAG_DEF_ALT_ADMIN_KEY;

	device_config->mrpcim_config.lag_config.ap_mode_config.alt_aggr =
	    VXGE_HAL_LAG_ALT_AGGR_DEFAULT;

	device_config->mrpcim_config.lag_config.sl_mode_config.pref_indiv_port =
	    VXGE_HAL_LAG_PREF_INDIV_PORT_DEFAULT;

	device_config->mrpcim_config.lag_config.lacp_config.lacp_en =
	    VXGE_HAL_LAG_LACP_EN_DEFAULT;

	device_config->mrpcim_config.lag_config.lacp_config.lacp_begin =
	    VXGE_HAL_LAG_LACP_BEGIN_DEFAULT;

	device_config->mrpcim_config.lag_config.lacp_config.discard_lacp =
	    VXGE_HAL_LAG_DISCARD_LACP_DEFAULT;

	device_config->mrpcim_config.lag_config.lacp_config.liberal_len_chk =
	    VXGE_HAL_LAG_LIBERAL_LEN_CHK_DEFAULT;

	device_config->mrpcim_config.lag_config.lacp_config.marker_gen_recv_en =
	    VXGE_HAL_LAG_MARKER_GEN_RECV_EN_DEFAULT;

	device_config->mrpcim_config.lag_config.lacp_config.marker_resp_en =
	    VXGE_HAL_LAG_MARKER_RESP_EN_DEFAULT;

	device_config->
	    mrpcim_config.lag_config.lacp_config.marker_resp_timeout =
	    VXGE_HAL_LAG_DEF_MARKER_RESP_TIMEOUT;

	device_config->
	    mrpcim_config.lag_config.lacp_config.slow_proto_mrkr_min_interval =
	    VXGE_HAL_LAG_DEF_SLOW_PROTO_MRKR_MIN_INTERVAL;

	device_config->mrpcim_config.lag_config.lacp_config.throttle_mrkr_resp =
	    VXGE_HAL_LAG_THROTTLE_MRKR_RESP_DEFAULT;

	device_config->mrpcim_config.lag_config.incr_tx_aggr_stats =
	    VXGE_HAL_LAG_INCR_TX_AGGR_STATS_DEFAULT;

	for (i = 0; i < VXGE_HAL_LAG_PORT_MAX_PORTS; i++) {

		vxge_hal_lag_port_config_t *port_config =
		&device_config->mrpcim_config.lag_config.port_config[i];

		port_config->port_id = i;

		port_config->lag_en = VXGE_HAL_LAG_PORT_LAG_EN_DEFAULT;

		port_config->discard_slow_proto =
		    VXGE_HAL_LAG_PORT_DISCARD_SLOW_PROTO_DEFAULT;

		port_config->host_chosen_aggr =
		    VXGE_HAL_LAG_PORT_HOST_CHOSEN_AGGR_DEFAULT;

		port_config->host_chosen_aggr =
		    VXGE_HAL_LAG_PORT_HOST_CHOSEN_AGGR_DEFAULT;

		port_config->discard_unknown_slow_proto =
		    VXGE_HAL_LAG_PORT_DISCARD_UNKNOWN_SLOW_PROTO_DEFAULT;

		port_config->actor_port_num =
		    VXGE_HAL_LAG_PORT_DEF_ACTOR_PORT_NUM;

		port_config->actor_port_priority =
		    VXGE_HAL_LAG_PORT_DEF_ACTOR_PORT_PRIORITY;

		port_config->actor_key_10g =
		    VXGE_HAL_LAG_PORT_DEF_ACTOR_KEY_10G;

		port_config->actor_key_1g =
		    VXGE_HAL_LAG_PORT_DEF_ACTOR_KEY_1G;

		port_config->actor_lacp_activity =
		    VXGE_HAL_LAG_PORT_ACTOR_LACP_ACTIVITY_DEFAULT;

		port_config->actor_lacp_timeout =
		    VXGE_HAL_LAG_PORT_ACTOR_LACP_TIMEOUT_DEFAULT;

		port_config->actor_aggregation =
		    VXGE_HAL_LAG_PORT_ACTOR_AGGREGATION_DEFAULT;

		port_config->actor_synchronization =
		    VXGE_HAL_LAG_PORT_ACTOR_SYNCHRONIZATION_DEFAULT;

		port_config->actor_collecting =
		    VXGE_HAL_LAG_PORT_ACTOR_COLLECTING_DEFAULT;

		port_config->actor_distributing =
		    VXGE_HAL_LAG_PORT_ACTOR_DISTRIBUTING_DEFAULT;

		port_config->actor_distributing =
		    VXGE_HAL_LAG_PORT_ACTOR_DISTRIBUTING_DEFAULT;

		port_config->actor_defaulted =
		    VXGE_HAL_LAG_PORT_ACTOR_DEFAULTED_DEFAULT;

		port_config->actor_expired =
		    VXGE_HAL_LAG_PORT_ACTOR_EXPIRED_DEFAULT;

		port_config->partner_sys_pri =
		    VXGE_HAL_LAG_PORT_DEF_PARTNER_SYS_PRI;

		port_config->partner_key =
		    VXGE_HAL_LAG_PORT_DEF_PARTNER_KEY;

		port_config->partner_port_num =
		    VXGE_HAL_LAG_PORT_DEF_PARTNER_PORT_NUM;

		port_config->partner_port_priority =
		    VXGE_HAL_LAG_PORT_DEF_PARTNER_PORT_PRIORITY;

		port_config->partner_lacp_activity =
		    VXGE_HAL_LAG_PORT_PARTNER_LACP_ACTIVITY_DEFAULT;

		port_config->partner_lacp_timeout =
		    VXGE_HAL_LAG_PORT_PARTNER_LACP_TIMEOUT_DEFAULT;

		port_config->partner_aggregation =
		    VXGE_HAL_LAG_PORT_PARTNER_AGGREGATION_DEFAULT;

		port_config->partner_synchronization =
		    VXGE_HAL_LAG_PORT_PARTNER_SYNCHRONIZATION_DEFAULT;

		port_config->partner_collecting =
		    VXGE_HAL_LAG_PORT_PARTNER_COLLECTING_DEFAULT;

		port_config->partner_distributing =
		    VXGE_HAL_LAG_PORT_PARTNER_DISTRIBUTING_DEFAULT;

		port_config->partner_distributing =
		    VXGE_HAL_LAG_PORT_PARTNER_DISTRIBUTING_DEFAULT;

		port_config->partner_defaulted =
		    VXGE_HAL_LAG_PORT_PARTNER_DEFAULTED_DEFAULT;

		port_config->partner_expired =
		    VXGE_HAL_LAG_PORT_PARTNER_EXPIRED_DEFAULT;

	}

	for (i = 0; i < VXGE_HAL_LAG_AGGR_MAX_PORTS; i++) {

		device_config->
		    mrpcim_config.lag_config.aggr_config[i].aggr_id = i + 1;

		device_config->
		    mrpcim_config.lag_config.aggr_config[i].use_port_mac_addr =
		    VXGE_HAL_LAG_AGGR_USE_PORT_MAC_ADDR_DEFAULT;

		device_config->
		    mrpcim_config.lag_config.aggr_config[i].mac_addr_sel =
		    VXGE_HAL_LAG_AGGR_MAC_ADDR_SEL_DEFAULT;

		device_config->
		    mrpcim_config.lag_config.aggr_config[i].admin_key =
		    VXGE_HAL_LAG_AGGR_DEF_ADMIN_KEY;

	}

	device_config->mrpcim_config.lag_config.sys_pri =
	    VXGE_HAL_LAG_DEF_SYS_PRI;

	device_config->mrpcim_config.lag_config.use_port_mac_addr =
	    VXGE_HAL_LAG_USE_PORT_MAC_ADDR_DEFAULT;

	device_config->mrpcim_config.lag_config.mac_addr_sel =
	    VXGE_HAL_LAG_MAC_ADDR_SEL_DEFAULT;

	device_config->mrpcim_config.lag_config.fast_per_time =
	    VXGE_HAL_LAG_DEF_FAST_PER_TIME;

	device_config->mrpcim_config.lag_config.slow_per_time =
	    VXGE_HAL_LAG_DEF_SLOW_PER_TIME;

	device_config->mrpcim_config.lag_config.short_timeout =
	    VXGE_HAL_LAG_DEF_SHORT_TIMEOUT;

	device_config->mrpcim_config.lag_config.long_timeout =
	    VXGE_HAL_LAG_DEF_LONG_TIMEOUT;

	device_config->mrpcim_config.lag_config.churn_det_time =
	    VXGE_HAL_LAG_DEF_CHURN_DET_TIME;

	device_config->mrpcim_config.lag_config.aggr_wait_time =
	    VXGE_HAL_LAG_DEF_AGGR_WAIT_TIME;

	device_config->mrpcim_config.lag_config.short_timer_scale =
	    VXGE_HAL_LAG_SHORT_TIMER_SCALE_DEFAULT;

	device_config->mrpcim_config.lag_config.long_timer_scale =
	    VXGE_HAL_LAG_LONG_TIMER_SCALE_DEFAULT;

	for (i = 0; i < VXGE_HAL_MAX_VIRTUAL_PATHS; i++) {

		device_config->mrpcim_config.vp_qos[i].priority =
		    VXGE_HAL_VPATH_QOS_PRIORITY_DEFAULT;

		device_config->mrpcim_config.vp_qos[i].min_bandwidth =
		    VXGE_HAL_VPATH_QOS_MIN_BANDWIDTH_DEFAULT;

		device_config->mrpcim_config.vp_qos[i].max_bandwidth =
		    VXGE_HAL_VPATH_QOS_MAX_BANDWIDTH_DEFAULT;

	}

	device_config->isr_polling_cnt = VXGE_HAL_DEF_ISR_POLLING_CNT;

	device_config->max_payload_size =
	    VXGE_HAL_USE_BIOS_DEFAULT_PAYLOAD_SIZE;

	device_config->mmrb_count = VXGE_HAL_USE_BIOS_DEFAULT_MMRB_COUNT;

	device_config->stats_refresh_time_sec =
	    VXGE_HAL_USE_FLASH_DEFAULT_STATS_REFRESH_TIME;

	device_config->intr_mode = VXGE_HAL_INTR_MODE_DEF;

	device_config->dump_on_unknown = VXGE_HAL_DUMP_ON_UNKNOWN_DEFAULT;

	device_config->dump_on_serr = VXGE_HAL_DUMP_ON_SERR_DEFAULT;

	device_config->dump_on_critical = VXGE_HAL_DUMP_ON_CRITICAL_DEFAULT;

	device_config->dump_on_eccerr = VXGE_HAL_DUMP_ON_ECCERR_DEFAULT;

	device_config->rth_en = VXGE_HAL_RTH_DEFAULT;

	device_config->rth_it_type = VXGE_HAL_RTH_IT_TYPE_DEFAULT;

	device_config->device_poll_millis = VXGE_HAL_DEF_DEVICE_POLL_MILLIS;

	device_config->rts_mac_en = VXGE_HAL_RTS_MAC_DEFAULT;

	device_config->rts_qos_en = VXGE_HAL_RTS_QOS_DEFAULT;

	device_config->rts_port_en = VXGE_HAL_RTS_PORT_DEFAULT;

	for (i = 0; i < VXGE_HAL_MAX_VIRTUAL_PATHS; i++) {

		device_config->vp_config[i].vp_id = i;

		device_config->vp_config[i].wire_port =
		    VXGE_HAL_VPATH_USE_DEFAULT_PORT;

		device_config->vp_config[i].priority =
		    VXGE_HAL_VPATH_PRIORITY_DEFAULT;

		device_config->vp_config[i].bandwidth =
		    VXGE_HAL_VPATH_BW_LIMIT_DEFAULT;

		device_config->vp_config[i].no_snoop =
		    VXGE_HAL_VPATH_NO_SNOOP_USE_FLASH_DEFAULT;

		device_config->vp_config[i].ring.enable =
		    VXGE_HAL_RING_DEFAULT;

		device_config->vp_config[i].ring.ring_length =
		    VXGE_HAL_DEF_RING_LENGTH;

		device_config->vp_config[i].ring.buffer_mode =
		    VXGE_HAL_RING_RXD_BUFFER_MODE_DEFAULT;

		device_config->vp_config[i].ring.scatter_mode =
		    VXGE_HAL_RING_SCATTER_MODE_USE_FLASH_DEFAULT;

		device_config->vp_config[i].ring.post_mode =
		    VXGE_HAL_RING_POST_MODE_USE_FLASH_DEFAULT;

		device_config->vp_config[i].ring.max_frm_len =
		    VXGE_HAL_MAX_RING_FRM_LEN_USE_MTU;

		device_config->vp_config[i].ring.no_snoop_bits =
		    VXGE_HAL_RING_NO_SNOOP_USE_FLASH_DEFAULT;

		device_config->vp_config[i].ring.rx_timer_val =
		    VXGE_HAL_RING_USE_FLASH_DEFAULT_RX_TIMER_VAL;

		device_config->vp_config[i].ring.greedy_return =
		    VXGE_HAL_RING_GREEDY_RETURN_USE_FLASH_DEFAULT;

		device_config->vp_config[i].ring.rx_timer_ci =
		    VXGE_HAL_RING_RX_TIMER_CI_USE_FLASH_DEFAULT;

		device_config->vp_config[i].ring.backoff_interval_us =
		    VXGE_HAL_USE_FLASH_DEFAULT_BACKOFF_INTERVAL_US;

		device_config->vp_config[i].ring.indicate_max_pkts =
		    VXGE_HAL_DEF_RING_INDICATE_MAX_PKTS;


		device_config->vp_config[i].fifo.enable =
		    VXGE_HAL_FIFO_DEFAULT;

		device_config->vp_config[i].fifo.fifo_length =
		    VXGE_HAL_DEF_FIFO_LENGTH;

		device_config->vp_config[i].fifo.max_frags =
		    VXGE_HAL_DEF_FIFO_FRAGS;

		device_config->vp_config[i].fifo.alignment_size =
		    VXGE_HAL_DEF_FIFO_ALIGNMENT_SIZE;

		device_config->vp_config[i].fifo.max_aligned_frags = 0;

		device_config->vp_config[i].fifo.intr =
		    VXGE_HAL_FIFO_QUEUE_INTR_DEFAULT;

		device_config->vp_config[i].fifo.no_snoop_bits =
		    VXGE_HAL_FIFO_NO_SNOOP_DEFAULT;


		device_config->vp_config[i].tti.intr_enable =
		    VXGE_HAL_TIM_INTR_DEFAULT;

		device_config->vp_config[i].tti.btimer_val =
		    VXGE_HAL_USE_FLASH_DEFAULT_TIM_BTIMER_VAL;

		device_config->vp_config[i].tti.timer_ac_en =
		    VXGE_HAL_TIM_TIMER_AC_USE_FLASH_DEFAULT;

		device_config->vp_config[i].tti.timer_ci_en =
		    VXGE_HAL_TIM_TIMER_CI_USE_FLASH_DEFAULT;

		device_config->vp_config[i].tti.timer_ri_en =
		    VXGE_HAL_TIM_TIMER_RI_USE_FLASH_DEFAULT;

		device_config->vp_config[i].tti.rtimer_event_sf =
		    VXGE_HAL_USE_FLASH_DEFAULT_TIM_RTIMER_EVENT_SF;

		device_config->vp_config[i].tti.rtimer_val =
		    VXGE_HAL_USE_FLASH_DEFAULT_TIM_RTIMER_VAL;

		device_config->vp_config[i].tti.util_sel =
		    VXGE_HAL_TIM_UTIL_SEL_USE_FLASH_DEFAULT;

		device_config->vp_config[i].tti.ltimer_val =
		    VXGE_HAL_USE_FLASH_DEFAULT_TIM_LTIMER_VAL;

		device_config->vp_config[i].tti.txfrm_cnt_en =
		    VXGE_HAL_TXFRM_CNT_EN_USE_FLASH_DEFAULT;

		device_config->vp_config[i].tti.txd_cnt_en =
		    VXGE_HAL_TXD_CNT_EN_USE_FLASH_DEFAULT;

		device_config->vp_config[i].tti.urange_a =
		    VXGE_HAL_USE_FLASH_DEFAULT_TIM_URANGE_A;

		device_config->vp_config[i].tti.uec_a =
		    VXGE_HAL_USE_FLASH_DEFAULT_TIM_UEC_A;

		device_config->vp_config[i].tti.urange_b =
		    VXGE_HAL_USE_FLASH_DEFAULT_TIM_URANGE_B;

		device_config->vp_config[i].tti.uec_b =
		    VXGE_HAL_USE_FLASH_DEFAULT_TIM_UEC_B;

		device_config->vp_config[i].tti.urange_c =
		    VXGE_HAL_USE_FLASH_DEFAULT_TIM_URANGE_C;

		device_config->vp_config[i].tti.uec_c =
		    VXGE_HAL_USE_FLASH_DEFAULT_TIM_UEC_C;

		device_config->vp_config[i].tti.uec_d =
		    VXGE_HAL_USE_FLASH_DEFAULT_TIM_UEC_D;

		device_config->vp_config[i].tti.ufca_intr_thres =
		    VXGE_HAL_USE_FLASH_DEFAULT_UFCA_INTR_THRES;

		device_config->vp_config[i].tti.ufca_lo_lim =
		    VXGE_HAL_USE_FLASH_DEFAULT_UFCA_LO_LIM;

		device_config->vp_config[i].tti.ufca_hi_lim =
		    VXGE_HAL_USE_FLASH_DEFAULT_UFCA_HI_LIM;

		device_config->vp_config[i].tti.ufca_lbolt_period =
		    VXGE_HAL_USE_FLASH_DEFAULT_UFCA_LBOLT_PERIOD;

		device_config->vp_config[i].rti.intr_enable =
		    VXGE_HAL_TIM_INTR_DEFAULT;

		device_config->vp_config[i].rti.btimer_val =
		    VXGE_HAL_USE_FLASH_DEFAULT_TIM_BTIMER_VAL;

		device_config->vp_config[i].rti.timer_ac_en =
		    VXGE_HAL_TIM_TIMER_AC_USE_FLASH_DEFAULT;

		device_config->vp_config[i].rti.timer_ci_en =
		    VXGE_HAL_TIM_TIMER_CI_USE_FLASH_DEFAULT;

		device_config->vp_config[i].rti.timer_ri_en =
		    VXGE_HAL_TIM_TIMER_RI_USE_FLASH_DEFAULT;

		device_config->vp_config[i].rti.rtimer_event_sf =
		    VXGE_HAL_USE_FLASH_DEFAULT_TIM_RTIMER_EVENT_SF;

		device_config->vp_config[i].rti.rtimer_val =
		    VXGE_HAL_USE_FLASH_DEFAULT_TIM_RTIMER_VAL;

		device_config->vp_config[i].rti.util_sel =
		    VXGE_HAL_TIM_UTIL_SEL_USE_FLASH_DEFAULT;

		device_config->vp_config[i].rti.ltimer_val =
		    VXGE_HAL_USE_FLASH_DEFAULT_TIM_LTIMER_VAL;

		device_config->vp_config[i].rti.txfrm_cnt_en =
		    VXGE_HAL_TXFRM_CNT_EN_USE_FLASH_DEFAULT;

		device_config->vp_config[i].rti.txd_cnt_en =
		    VXGE_HAL_TXD_CNT_EN_USE_FLASH_DEFAULT;

		device_config->vp_config[i].rti.urange_a =
		    VXGE_HAL_USE_FLASH_DEFAULT_TIM_URANGE_A;

		device_config->vp_config[i].rti.uec_a =
		    VXGE_HAL_USE_FLASH_DEFAULT_TIM_UEC_A;

		device_config->vp_config[i].rti.urange_b =
		    VXGE_HAL_USE_FLASH_DEFAULT_TIM_URANGE_B;

		device_config->vp_config[i].rti.uec_b =
		    VXGE_HAL_USE_FLASH_DEFAULT_TIM_UEC_B;

		device_config->vp_config[i].rti.urange_c =
		    VXGE_HAL_USE_FLASH_DEFAULT_TIM_URANGE_C;

		device_config->vp_config[i].rti.uec_c =
		    VXGE_HAL_USE_FLASH_DEFAULT_TIM_UEC_C;

		device_config->vp_config[i].rti.uec_d =
		    VXGE_HAL_USE_FLASH_DEFAULT_TIM_UEC_D;

		device_config->vp_config[i].rti.ufca_intr_thres =
		    VXGE_HAL_USE_FLASH_DEFAULT_UFCA_INTR_THRES;

		device_config->vp_config[i].rti.ufca_lo_lim =
		    VXGE_HAL_USE_FLASH_DEFAULT_UFCA_LO_LIM;

		device_config->vp_config[i].rti.ufca_hi_lim =
		    VXGE_HAL_USE_FLASH_DEFAULT_UFCA_HI_LIM;

		device_config->vp_config[i].rti.ufca_lbolt_period =
		    VXGE_HAL_USE_FLASH_DEFAULT_UFCA_LBOLT_PERIOD;

		device_config->vp_config[i].mtu =
		    VXGE_HAL_VPATH_USE_FLASH_DEFAULT_INITIAL_MTU;

		device_config->vp_config[i].tpa_lsov2_en =
		    VXGE_HAL_VPATH_TPA_LSOV2_EN_USE_FLASH_DEFAULT;

		device_config->vp_config[i].tpa_ignore_frame_error =
		    VXGE_HAL_VPATH_TPA_IGNORE_FRAME_ERROR_USE_FLASH_DEFAULT;

		device_config->vp_config[i].tpa_ipv6_keep_searching =
		    VXGE_HAL_VPATH_TPA_IPV6_KEEP_SEARCHING_USE_FLASH_DEFAULT;

		device_config->vp_config[i].tpa_l4_pshdr_present =
		    VXGE_HAL_VPATH_TPA_L4_PSHDR_PRESENT_USE_FLASH_DEFAULT;

		device_config->vp_config[i].tpa_support_mobile_ipv6_hdrs =
		    VXGE_HAL_VPATH_TPA_SUPPORT_MOBILE_IPV6_HDRS_DEFAULT;

		device_config->vp_config[i].rpa_ipv4_tcp_incl_ph =
		    VXGE_HAL_VPATH_RPA_IPV4_TCP_INCL_PH_USE_FLASH_DEFAULT;

		device_config->vp_config[i].rpa_ipv6_tcp_incl_ph =
		    VXGE_HAL_VPATH_RPA_IPV6_TCP_INCL_PH_USE_FLASH_DEFAULT;

		device_config->vp_config[i].rpa_ipv4_udp_incl_ph =
		    VXGE_HAL_VPATH_RPA_IPV4_UDP_INCL_PH_USE_FLASH_DEFAULT;

		device_config->vp_config[i].rpa_ipv6_udp_incl_ph =
		    VXGE_HAL_VPATH_RPA_IPV6_UDP_INCL_PH_USE_FLASH_DEFAULT;

		device_config->vp_config[i].rpa_l4_incl_cf =
		    VXGE_HAL_VPATH_RPA_L4_INCL_CF_USE_FLASH_DEFAULT;

		device_config->vp_config[i].rpa_strip_vlan_tag =
		    VXGE_HAL_VPATH_RPA_STRIP_VLAN_TAG_USE_FLASH_DEFAULT;

		device_config->vp_config[i].rpa_l4_comp_csum =
		    VXGE_HAL_VPATH_RPA_L4_COMP_CSUM_USE_FLASH_DEFAULT;

		device_config->vp_config[i].rpa_l3_incl_cf =
		    VXGE_HAL_VPATH_RPA_L3_INCL_CF_USE_FLASH_DEFAULT;

		device_config->vp_config[i].rpa_l3_comp_csum =
		    VXGE_HAL_VPATH_RPA_L3_COMP_CSUM_USE_FLASH_DEFAULT;

		device_config->vp_config[i].rpa_ucast_all_addr_en =
		    VXGE_HAL_VPATH_RPA_UCAST_ALL_ADDR_USE_FLASH_DEFAULT;

		device_config->vp_config[i].rpa_mcast_all_addr_en =
		    VXGE_HAL_VPATH_RPA_MCAST_ALL_ADDR_USE_FLASH_DEFAULT;

		device_config->vp_config[i].rpa_bcast_en =
		    VXGE_HAL_VPATH_RPA_BCAST_USE_FLASH_DEFAULT;

		device_config->vp_config[i].rpa_all_vid_en =
		    VXGE_HAL_VPATH_RPA_ALL_VID_USE_FLASH_DEFAULT;

		device_config->vp_config[i].vp_queue_l2_flow =
		    VXGE_HAL_VPATH_VP_Q_L2_FLOW_USE_FLASH_DEFAULT;

	}

	device_config->max_cqe_groups = VXGE_HAL_DEF_MAX_CQE_GROUPS;

	device_config->max_num_wqe_od_groups = VXGE_HAL_DEF_MAX_NUM_OD_GROUPS;

	device_config->no_wqe_threshold = VXGE_HAL_DEF_NO_WQE_THRESHOLD;

	device_config->refill_threshold_high =
	    VXGE_HAL_DEF_REFILL_THRESHOLD_HIGH;

	device_config->refill_threshold_low = VXGE_HAL_DEF_REFILL_THRESHOLD_LOW;

	device_config->ack_blk_limit = VXGE_HAL_DEF_ACK_BLOCK_LIMIT;

	device_config->poll_or_doorbell = VXGE_HAL_POLL_OR_DOORBELL_DEFAULT;

	device_config->stats_read_method = VXGE_HAL_STATS_READ_METHOD_DEFAULT;

	device_config->debug_level = VXGE_DEBUG_LEVEL_DEF;

	device_config->debug_mask = VXGE_DEBUG_MODULE_MASK_DEF;

#if defined(VXGE_TRACE_INTO_CIRCULAR_ARR)
	device_config->tracebuf_size = VXGE_HAL_DEF_CIRCULAR_ARR;
#endif
	vxge_hal_trace_log_driver("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);
}

void
vxge_hw_vpath_set_zero_rx_frm_len(vxge_hal_device_h devh, u32 vp_id)
{
	u64 val64;
	__hal_device_t *hldev = (__hal_device_t *) devh;
	__hal_virtualpath_t *vpath;

	vxge_assert(devh != NULL);

	vpath = (__hal_virtualpath_t *) &hldev->virtual_paths[vp_id];

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("devh = 0x"VXGE_OS_STXFMT", vp_id = %d",
	    (ptr_t) devh, vp_id);

	val64 = vxge_os_pio_mem_read64(vpath->hldev->header.pdev,
	    vpath->hldev->header.regh0,
	    &vpath->vp_reg->rxmac_vcfg0);

	val64 &= ~VXGE_HAL_RXMAC_VCFG0_RTS_MAX_FRM_LEN(0x3fff);

	vxge_os_pio_mem_write64(vpath->hldev->header.pdev,
	    vpath->hldev->header.regh0,
	    val64,
	    &vpath->vp_reg->rxmac_vcfg0);

	vxge_hal_trace_log_ring("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);

	vxge_os_pio_mem_read64(vpath->hldev->header.pdev,
	    vpath->hldev->header.regh0,
	    &vpath->vp_reg->rxmac_vcfg0);
}

/*
 * vxge_hw_vpath_wait_receive_idle - Wait for Rx to become idle
 *
 * Bug: Receive path stuck during small frames blast test after numerous vpath
 * reset cycle
 *
 * Fix: Driver work-around is to ensure that the vpath queue in the FB(frame
 * buffer) is empty before reset is asserted. In order to do this driver needs
 * to stop RxMAC from sending frames to the queue, e.g., by configuring the
 * max frame length for the vpath to 0 or some small value. Driver then polls
 * WRDMA registers to check that the ring controller for the vpath is not
 * processing frames for a period of time(while having enough RxDs to do so).
 *
 * Poll 2 registers in the WRDMA, namely the FRM_IN_PROGRESS_CNT_VPn register
 * and the PRC_RXD_DOORBELL_VPn register. There is no per-vpath register in
 * the frame buffer that indicates if the vpath queue is empty, so determine
 * the empty state with 2 conditions:
 * 1. There are no frames currently being processed in the WRDMA for
 * the vpath, and
 * 2. The ring controller for the vpath is not being starved of RxDs
 * (otherwise it will not be able to process frames even though the FB vpath
 * queue is not empty).
 *
 * For the second condition, compare the read value of PRC_RXD_DOORBELL_VPn
 * register against the RXD_SPAT value for the vpath.
 * The ring controller will not attempt to fetch RxDs until it has at least
 * RXD_SPAT qwords in the doorbell. A factor of 2 is used just to be safe.
 * Additionally, it is also possible that the ring controller is not
 * processing frames because of arbitration. The chance of this is very small,
 * and we try to reduce it even further by checking that the 2 conditions above
 * hold in 3 successive polls. This bug does not occur when frames from the
 * reset vpath are not selected back-to-back due to arbitration.
 * @hldev: HW device handle.
 * @vp_id: Vpath ID.
 * Returns: void
 */
void
vxge_hw_vpath_wait_receive_idle(vxge_hal_device_h devh, u32 vp_id,
    u32 *count, u32 *total_count)
{
	u64 val64;
	u32 new_qw_count, rxd_spat;
	__hal_device_t *hldev = (__hal_device_t *) devh;
	__hal_virtualpath_t *vpath;

	vpath = &hldev->virtual_paths[vp_id];

	vxge_assert(vpath != NULL);

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	if (vpath->vp_config->ring.enable == VXGE_HAL_RING_DISABLE) {
		vxge_hal_trace_log_vpath("<== %s:%s:%d ",
		    __FILE__, __func__, __LINE__);
		return;
	}

	do {
		vxge_os_mdelay(10);

		val64 = vxge_os_pio_mem_read64(
		    hldev->header.pdev,
		    hldev->header.regh0,
		    &vpath->vp_reg->prc_rxd_doorbell);

		new_qw_count =
		    (u32) VXGE_HAL_PRC_RXD_DOORBELL_GET_NEW_QW_CNT(val64);

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &vpath->vp_reg->prc_cfg6);

		rxd_spat = (u32) VXGE_HAL_PRC_CFG6_GET_RXD_SPAT(val64) + 1;

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &vpath->vp_reg->frm_in_progress_cnt);

		/*
		 * Check if there is enough RxDs with HW AND
		 * it is not processing any frames.
		 */

		if ((new_qw_count <= 2 * rxd_spat) || (val64 > 0))
			*count = 0;
		else
			(*count)++;
		(*total_count)++;

	} while ((*count < VXGE_HW_MIN_SUCCESSIVE_IDLE_COUNT) &&
	    (*total_count < VXGE_HW_MAX_POLLING_COUNT));

	vxge_hal_trace_log_vpath("<== %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_assert(*total_count < VXGE_HW_MAX_POLLING_COUNT);
}
