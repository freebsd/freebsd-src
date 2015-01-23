/*-
 * Copyright 2009 Solarflare Communications Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS AND
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "efsys.h"
#include "efx.h"
#include "efx_impl.h"

#if EFSYS_OPT_MON_SIENA

	__checkReturn	int
siena_mon_reset(
	__in		efx_nic_t *enp)
{
	_NOTE(ARGUNUSED(enp))
	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_SIENA);

	return (0);
}

	__checkReturn	int
siena_mon_reconfigure(
	__in		efx_nic_t *enp)
{
	_NOTE(ARGUNUSED(enp))
	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_SIENA);

	return (0);
}

#if EFSYS_OPT_MON_STATS

#define	SIENA_MON_WRONG_PORT (uint16_t)0xffff

static __cs uint16_t __siena_mon_port0_map[] = {
	EFX_MON_STAT_INT_TEMP,		/* MC_CMD_SENSOR_CONTROLLER_TEMP */
	EFX_MON_STAT_EXT_TEMP,		/* MC_CMD_SENSOR_PHY_COMMON_TEMP */
	EFX_MON_STAT_INT_COOLING,	/* MC_CMD_SENSOR_CONTROLLER_COOLING */
	EFX_MON_STAT_EXT_TEMP,		/* MC_CMD_SENSOR_PHY0_TEMP */
	EFX_MON_STAT_EXT_COOLING,	/* MC_CMD_SENSOR_PHY0_COOLING */
	SIENA_MON_WRONG_PORT,		/* MC_CMD_SENSOR_PHY1_TEMP */
	SIENA_MON_WRONG_PORT,		/* MC_CMD_SENSOR_PHY1_COOLING */
	EFX_MON_STAT_1V,		/* MC_CMD_SENSOR_IN_1V0 */
	EFX_MON_STAT_1_2V,		/* MC_CMD_SENSOR_IN_1V2 */
	EFX_MON_STAT_1_8V,		/* MC_CMD_SENSOR_IN_1V8 */
	EFX_MON_STAT_2_5V,		/* MC_CMD_SENSOR_IN_2V5 */
	EFX_MON_STAT_3_3V,		/* MC_CMD_SENSOR_IN_3V3 */
	EFX_MON_STAT_12V,		/* MC_CMD_SENSOR_IN_12V0 */
};

static __cs uint16_t __siena_mon_port1_map[] = {
	EFX_MON_STAT_INT_TEMP,		/* MC_CMD_SENSOR_CONTROLLER_TEMP */
	EFX_MON_STAT_EXT_TEMP,		/* MC_CMD_SENSOR_PHY_COMMON_TEMP */
	EFX_MON_STAT_INT_COOLING,	/* MC_CMD_SENSOR_CONTROLLER_COOLING */
	SIENA_MON_WRONG_PORT,		/* MC_CMD_SENSOR_PHY0_TEMP */
	SIENA_MON_WRONG_PORT,		/* MC_CMD_SENSOR_PHY0_COOLING */
	EFX_MON_STAT_EXT_TEMP,		/* MC_CMD_SENSOR_PHY1_TEMP */
	EFX_MON_STAT_EXT_COOLING,	/* MC_CMD_SENSOR_PHY1_COOLING */
	EFX_MON_STAT_1V,		/* MC_CMD_SENSOR_IN_1V0 */
	EFX_MON_STAT_1_2V,		/* MC_CMD_SENSOR_IN_1V2 */
	EFX_MON_STAT_1_8V,		/* MC_CMD_SENSOR_IN_1V8 */
	EFX_MON_STAT_2_5V,		/* MC_CMD_SENSOR_IN_2V5 */
	EFX_MON_STAT_3_3V,		/* MC_CMD_SENSOR_IN_3V3 */
	EFX_MON_STAT_12V,		/* MC_CMD_SENSOR_IN_12V0 */
};

#define	SIENA_STATIC_SENSOR_ASSERT(_field)				\
	EFX_STATIC_ASSERT(MC_CMD_SENSOR_STATE_ ## _field		\
			    == EFX_MON_STAT_STATE_ ## _field)

					void
siena_mon_decode_stats(
	__in				efx_nic_t *enp,
	__in				uint32_t dmask,
	__in_opt			efsys_mem_t *esmp,
	__out_opt			uint32_t *vmaskp,
	__out_ecount_opt(EFX_MON_NSTATS)	efx_mon_stat_value_t *value)
{
	efx_mcdi_iface_t *emip = &(enp->en_u.siena.enu_mip);
	uint16_t *sensor_map;
	uint16_t mc_sensor;
	size_t mc_sensor_max;
	uint32_t vmask = 0;

	/* Assert the MC_CMD_SENSOR and EFX_MON_STATE namespaces agree */
	SIENA_STATIC_SENSOR_ASSERT(OK);
	SIENA_STATIC_SENSOR_ASSERT(WARNING);
	SIENA_STATIC_SENSOR_ASSERT(FATAL);
	SIENA_STATIC_SENSOR_ASSERT(BROKEN);

	EFX_STATIC_ASSERT(sizeof (__siena_mon_port1_map)
			    == sizeof (__siena_mon_port0_map));
	mc_sensor_max = EFX_ARRAY_SIZE(__siena_mon_port0_map);
	sensor_map = (emip->emi_port == 1)
		? __siena_mon_port0_map
		: __siena_mon_port1_map;

	/*
	 * dmask may legitimately contain sensors not understood by the driver
	 */
	for (mc_sensor = 0; mc_sensor < mc_sensor_max; ++mc_sensor) {
		uint16_t efx_sensor = sensor_map[mc_sensor];

		if (efx_sensor == SIENA_MON_WRONG_PORT)
			continue;
		EFSYS_ASSERT(efx_sensor < EFX_MON_NSTATS);

		if (~dmask & (1 << mc_sensor))
			continue;

		vmask |= (1 << efx_sensor);
		if (value != NULL && esmp != NULL && !EFSYS_MEM_IS_NULL(esmp)) {
			efx_mon_stat_value_t *emsvp = value + efx_sensor;
			efx_dword_t dword;
			EFSYS_MEM_READD(esmp, 4 * mc_sensor, &dword);
			emsvp->emsv_value =
				(uint16_t)EFX_DWORD_FIELD(
					dword,
					MC_CMD_SENSOR_VALUE_ENTRY_TYPEDEF_VALUE);
			emsvp->emsv_state =
				(uint16_t)EFX_DWORD_FIELD(
					dword,
					MC_CMD_SENSOR_VALUE_ENTRY_TYPEDEF_STATE);
		}
	}

	if (vmaskp != NULL)
		*vmaskp = vmask;
}

	__checkReturn			int
siena_mon_ev(
	__in				efx_nic_t *enp,
	__in				efx_qword_t *eqp,
	__out				efx_mon_stat_t *idp,
	__out				efx_mon_stat_value_t *valuep)
{
	efx_mcdi_iface_t *emip = &(enp->en_u.siena.enu_mip);
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	uint16_t ev_monitor;
	uint16_t ev_state;
	uint16_t ev_value;
	uint16_t *sensor_map;
	efx_mon_stat_t id;
	int rc;

	sensor_map = (emip->emi_port == 1)
		? __siena_mon_port0_map
		: __siena_mon_port1_map;

	ev_monitor = (uint16_t)MCDI_EV_FIELD(eqp, SENSOREVT_MONITOR);
	ev_state = (uint16_t)MCDI_EV_FIELD(eqp, SENSOREVT_STATE);
	ev_value = (uint16_t)MCDI_EV_FIELD(eqp, SENSOREVT_VALUE);

	/* Hardware must support this statistic */
	EFSYS_ASSERT((1 << ev_monitor) & encp->enc_siena_mon_stat_mask);

	/* But we don't have to understand it */
	if (ev_monitor >= EFX_ARRAY_SIZE(__siena_mon_port0_map)) {
		rc = ENOTSUP;
		goto fail1;
	}

	id = sensor_map[ev_monitor];
	if (id == SIENA_MON_WRONG_PORT)
		return (ENODEV);
	EFSYS_ASSERT(id < EFX_MON_NSTATS);

	*idp = id;
	valuep->emsv_value = ev_value;
	valuep->emsv_state = ev_state;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn			int
siena_mon_stats_update(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp,
	__out_ecount(EFX_MON_NSTATS)	efx_mon_stat_value_t *values)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	uint32_t dmask = encp->enc_siena_mon_stat_mask;
	uint32_t vmask;
	uint8_t payload[MC_CMD_READ_SENSORS_IN_LEN];
	efx_mcdi_req_t req;
	int rc;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_SIENA);

	req.emr_cmd = MC_CMD_READ_SENSORS;
	req.emr_in_buf = payload;
	req.emr_in_length = sizeof (payload);
	EFX_STATIC_ASSERT(MC_CMD_READ_SENSORS_OUT_LEN == 0);
	req.emr_out_buf = NULL;
	req.emr_out_length = 0;

	MCDI_IN_SET_DWORD(req, READ_SENSORS_IN_DMA_ADDR_LO,
			    EFSYS_MEM_ADDR(esmp) & 0xffffffff);
	MCDI_IN_SET_DWORD(req, READ_SENSORS_IN_DMA_ADDR_HI,
			    EFSYS_MEM_ADDR(esmp) >> 32);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	siena_mon_decode_stats(enp, dmask, esmp, &vmask, values);
	EFSYS_ASSERT(vmask == encp->enc_mon_stat_mask);

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

#endif	/* EFSYS_OPT_MON_STATS */

#endif	/* EFSYS_OPT_MON_SIENA */
