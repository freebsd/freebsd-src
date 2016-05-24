/*-
 * Copyright (c) 2009-2016 Solarflare Communications Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are
 * those of the authors and should not be interpreted as representing official
 * policies, either expressed or implied, of the FreeBSD Project.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "efx.h"
#include "efx_impl.h"

#if EFSYS_OPT_MON_MCDI

#if EFSYS_OPT_MON_STATS

#define	MCDI_MON_NEXT_PAGE  (uint16_t)0xfffe
#define	MCDI_MON_INVALID_SENSOR (uint16_t)0xfffd
#define	MCDI_MON_PAGE_SIZE 0x20

/* Bitmasks of valid port(s) for each sensor */
#define	MCDI_MON_PORT_NONE	(0x00)
#define	MCDI_MON_PORT_P1	(0x01)
#define	MCDI_MON_PORT_P2	(0x02)
#define	MCDI_MON_PORT_P3	(0x04)
#define	MCDI_MON_PORT_P4	(0x08)
#define	MCDI_MON_PORT_Px	(0xFFFF)

/* Entry for MCDI sensor in sensor map */
#define	STAT(portmask, stat)	\
	{ (MCDI_MON_PORT_##portmask), (EFX_MON_STAT_##stat) }

/* Entry for sensor next page flag in sensor map */
#define	STAT_NEXT_PAGE()	\
	{ MCDI_MON_PORT_NONE, MCDI_MON_NEXT_PAGE }

/* Placeholder for gaps in the array */
#define	STAT_NO_SENSOR()	\
	{ MCDI_MON_PORT_NONE, MCDI_MON_INVALID_SENSOR }

/* Map from MC sensors to monitor statistics */
static const struct mcdi_sensor_map_s {
	uint16_t	msm_port_mask;
	uint16_t	msm_stat;
} mcdi_sensor_map[] = {
	/* Sensor page 0		MC_CMD_SENSOR_xxx */
	STAT(Px, INT_TEMP),		/* 0x00 CONTROLLER_TEMP */
	STAT(Px, EXT_TEMP),		/* 0x01 PHY_COMMON_TEMP */
	STAT(Px, INT_COOLING),		/* 0x02 CONTROLLER_COOLING */
	STAT(P1, EXT_TEMP),		/* 0x03 PHY0_TEMP */
	STAT(P1, EXT_COOLING),		/* 0x04 PHY0_COOLING */
	STAT(P2, EXT_TEMP),		/* 0x05 PHY1_TEMP */
	STAT(P2, EXT_COOLING),		/* 0x06 PHY1_COOLING */
	STAT(Px, 1V),			/* 0x07 IN_1V0 */
	STAT(Px, 1_2V),			/* 0x08 IN_1V2 */
	STAT(Px, 1_8V),			/* 0x09 IN_1V8 */
	STAT(Px, 2_5V),			/* 0x0a IN_2V5 */
	STAT(Px, 3_3V),			/* 0x0b IN_3V3 */
	STAT(Px, 12V),			/* 0x0c IN_12V0 */
	STAT(Px, 1_2VA),		/* 0x0d IN_1V2A */
	STAT(Px, VREF),			/* 0x0e IN_VREF */
	STAT(Px, VAOE),			/* 0x0f OUT_VAOE */
	STAT(Px, AOE_TEMP),		/* 0x10 AOE_TEMP */
	STAT(Px, PSU_AOE_TEMP),		/* 0x11 PSU_AOE_TEMP */
	STAT(Px, PSU_TEMP),		/* 0x12 PSU_TEMP */
	STAT(Px, FAN0),			/* 0x13 FAN_0 */
	STAT(Px, FAN1),			/* 0x14 FAN_1 */
	STAT(Px, FAN2),			/* 0x15 FAN_2 */
	STAT(Px, FAN3),			/* 0x16 FAN_3 */
	STAT(Px, FAN4),			/* 0x17 FAN_4 */
	STAT(Px, VAOE_IN),		/* 0x18 IN_VAOE */
	STAT(Px, IAOE),			/* 0x19 OUT_IAOE */
	STAT(Px, IAOE_IN),		/* 0x1a IN_IAOE */
	STAT(Px, NIC_POWER),		/* 0x1b NIC_POWER */
	STAT(Px, 0_9V),			/* 0x1c IN_0V9 */
	STAT(Px, I0_9V),		/* 0x1d IN_I0V9 */
	STAT(Px, I1_2V),		/* 0x1e IN_I1V2 */
	STAT_NEXT_PAGE(),		/* 0x1f Next page flag (not a sensor) */

	/* Sensor page 1		MC_CMD_SENSOR_xxx */
	STAT(Px, 0_9V_ADC),		/* 0x20 IN_0V9_ADC */
	STAT(Px, INT_TEMP2),		/* 0x21 CONTROLLER_2_TEMP */
	STAT(Px, VREG_TEMP),		/* 0x22 VREG_INTERNAL_TEMP */
	STAT(Px, VREG_0_9V_TEMP),	/* 0x23 VREG_0V9_TEMP */
	STAT(Px, VREG_1_2V_TEMP),	/* 0x24 VREG_1V2_TEMP */
	STAT(Px, INT_VPTAT),		/* 0x25 CTRLR. VPTAT */
	STAT(Px, INT_ADC_TEMP),		/* 0x26 CTRLR. INTERNAL_TEMP */
	STAT(Px, EXT_VPTAT),		/* 0x27 CTRLR. VPTAT_EXTADC */
	STAT(Px, EXT_ADC_TEMP),		/* 0x28 CTRLR. INTERNAL_TEMP_EXTADC */
	STAT(Px, AMBIENT_TEMP),		/* 0x29 AMBIENT_TEMP */
	STAT(Px, AIRFLOW),		/* 0x2a AIRFLOW */
	STAT(Px, VDD08D_VSS08D_CSR),	/* 0x2b VDD08D_VSS08D_CSR */
	STAT(Px, VDD08D_VSS08D_CSR_EXTADC), /* 0x2c VDD08D_VSS08D_CSR_EXTADC */
	STAT(Px, HOTPOINT_TEMP),	/* 0x2d HOTPOINT_TEMP */
	STAT(P1, PHY_POWER_SWITCH_PORT0),   /* 0x2e PHY_POWER_SWITCH_PORT0 */
	STAT(P2, PHY_POWER_SWITCH_PORT1),   /* 0x2f PHY_POWER_SWITCH_PORT1 */
	STAT(Px, MUM_VCC),		/* 0x30 MUM_VCC */
	STAT(Px, 0V9_A),		/* 0x31 0V9_A */
	STAT(Px, I0V9_A),		/* 0x32 I0V9_A */
	STAT(Px, 0V9_A_TEMP),		/* 0x33 0V9_A_TEMP */
	STAT(Px, 0V9_B),		/* 0x34 0V9_B */
	STAT(Px, I0V9_B),		/* 0x35 I0V9_B */
	STAT(Px, 0V9_B_TEMP),		/* 0x36 0V9_B_TEMP */
	STAT(Px, CCOM_AVREG_1V2_SUPPLY),  /* 0x37 CCOM_AVREG_1V2_SUPPLY */
	STAT(Px, CCOM_AVREG_1V2_SUPPLY_EXT_ADC),
					/* 0x38 CCOM_AVREG_1V2_SUPPLY_EXT_ADC */
	STAT(Px, CCOM_AVREG_1V8_SUPPLY),  /* 0x39 CCOM_AVREG_1V8_SUPPLY */
	STAT(Px, CCOM_AVREG_1V8_SUPPLY_EXT_ADC),
					/* 0x3a CCOM_AVREG_1V8_SUPPLY_EXT_ADC */
	STAT_NO_SENSOR(),		/* 0x3b (no sensor) */
	STAT_NO_SENSOR(),		/* 0x3c (no sensor) */
	STAT_NO_SENSOR(),		/* 0x3d (no sensor) */
	STAT_NO_SENSOR(),		/* 0x3e (no sensor) */
	STAT_NEXT_PAGE(),		/* 0x3f Next page flag (not a sensor) */

	/* Sensor page 2		MC_CMD_SENSOR_xxx */
	STAT(Px, CONTROLLER_MASTER_VPTAT),	   /* 0x40 MASTER_VPTAT */
	STAT(Px, CONTROLLER_MASTER_INTERNAL_TEMP), /* 0x41 MASTER_INT_TEMP */
	STAT(Px, CONTROLLER_MASTER_VPTAT_EXT_ADC), /* 0x42 MAST_VPTAT_EXT_ADC */
	STAT(Px, CONTROLLER_MASTER_INTERNAL_TEMP_EXT_ADC),
					/* 0x43 MASTER_INTERNAL_TEMP_EXT_ADC */
	STAT(Px, CONTROLLER_SLAVE_VPTAT),	  /* 0x44 SLAVE_VPTAT */
	STAT(Px, CONTROLLER_SLAVE_INTERNAL_TEMP), /* 0x45 SLAVE_INTERNAL_TEMP */
	STAT(Px, CONTROLLER_SLAVE_VPTAT_EXT_ADC), /* 0x46 SLAVE_VPTAT_EXT_ADC */
	STAT(Px, CONTROLLER_SLAVE_INTERNAL_TEMP_EXT_ADC),
					/* 0x47 SLAVE_INTERNAL_TEMP_EXT_ADC */
	STAT_NO_SENSOR(),		/* 0x48 (no sensor) */
	STAT(Px, SODIMM_VOUT),		/* 0x49 SODIMM_VOUT */
	STAT(Px, SODIMM_0_TEMP),	/* 0x4a SODIMM_0_TEMP */
	STAT(Px, SODIMM_1_TEMP),	/* 0x4b SODIMM_1_TEMP */
	STAT(Px, PHY0_VCC),		/* 0x4c PHY0_VCC */
	STAT(Px, PHY1_VCC),		/* 0x4d PHY1_VCC */
	STAT(Px, CONTROLLER_TDIODE_TEMP), /* 0x4e CONTROLLER_TDIODE_TEMP */
	STAT(Px, BOARD_FRONT_TEMP), 	/* 0x4f BOARD_FRONT_TEMP */
	STAT(Px, BOARD_BACK_TEMP), 	/* 0x50 BOARD_BACK_TEMP */
};

#define	MCDI_STATIC_SENSOR_ASSERT(_field)				\
	EFX_STATIC_ASSERT(MC_CMD_SENSOR_STATE_ ## _field		\
			    == EFX_MON_STAT_STATE_ ## _field)

static						void
mcdi_mon_decode_stats(
	__in					efx_nic_t *enp,
	__in_ecount(sensor_mask_size)		uint32_t *sensor_mask,
	__in					size_t sensor_mask_size,
	__in_opt				efsys_mem_t *esmp,
	__out_ecount_opt(sensor_mask_size)	uint32_t *stat_maskp,
	__inout_ecount_opt(EFX_MON_NSTATS)	efx_mon_stat_value_t *stat)
{
	efx_mcdi_iface_t *emip = &(enp->en_mcdi.em_emip);
	uint16_t port_mask;
	uint16_t sensor;
	size_t sensor_max;
	uint32_t stat_mask[(EFX_ARRAY_SIZE(mcdi_sensor_map) + 31) / 32];
	uint32_t idx = 0;
	uint32_t page = 0;

	/* Assert the MC_CMD_SENSOR and EFX_MON_STATE namespaces agree */
	MCDI_STATIC_SENSOR_ASSERT(OK);
	MCDI_STATIC_SENSOR_ASSERT(WARNING);
	MCDI_STATIC_SENSOR_ASSERT(FATAL);
	MCDI_STATIC_SENSOR_ASSERT(BROKEN);
	MCDI_STATIC_SENSOR_ASSERT(NO_READING);

	EFX_STATIC_ASSERT(sizeof (stat_mask[0]) * 8 ==
	    EFX_MON_MASK_ELEMENT_SIZE);
	sensor_max =
	    MIN((8 * sensor_mask_size), EFX_ARRAY_SIZE(mcdi_sensor_map));

	port_mask = 1U << emip->emi_port;

	memset(stat_mask, 0, sizeof (stat_mask));

	/*
	 * The MCDI sensor readings in the DMA buffer are a packed array of
	 * MC_CMD_SENSOR_VALUE_ENTRY structures, which only includes entries for
	 * supported sensors (bit set in sensor_mask). The sensor_mask and
	 * sensor readings do not include entries for the per-page NEXT_PAGE
	 * flag.
	 *
	 * sensor_mask may legitimately contain MCDI sensors that the driver
	 * does not understand.
	 */
	for (sensor = 0; sensor < sensor_max; ++sensor) {
		efx_mon_stat_t id = mcdi_sensor_map[sensor].msm_stat;

		if ((sensor % MCDI_MON_PAGE_SIZE) == MC_CMD_SENSOR_PAGE0_NEXT) {
			EFSYS_ASSERT3U(id, ==, MCDI_MON_NEXT_PAGE);
			page++;
			continue;
		}
		if (~(sensor_mask[page]) & (1U << sensor))
			continue;
		idx++;

		if ((port_mask & mcdi_sensor_map[sensor].msm_port_mask) == 0)
			continue;
		EFSYS_ASSERT(id < EFX_MON_NSTATS);

		/*
		 * stat_mask is a bitmask indexed by EFX_MON_* monitor statistic
		 * identifiers from efx_mon_stat_t (without NEXT_PAGE bits).
		 *
		 * If there is an entry in the MCDI sensor to monitor statistic
		 * map then the sensor reading is used for the value of the
		 * monitor statistic.
		 */
		stat_mask[id / EFX_MON_MASK_ELEMENT_SIZE] |=
		    (1U << (id % EFX_MON_MASK_ELEMENT_SIZE));

		if (stat != NULL && esmp != NULL && !EFSYS_MEM_IS_NULL(esmp)) {
			efx_dword_t dword;

			/* Get MCDI sensor reading from DMA buffer */
			EFSYS_MEM_READD(esmp, 4 * (idx - 1), &dword);

			/* Update EFX monitor stat from MCDI sensor reading */
			stat[id].emsv_value = (uint16_t)EFX_DWORD_FIELD(dword,
			    MC_CMD_SENSOR_VALUE_ENTRY_TYPEDEF_VALUE);

			stat[id].emsv_state = (uint16_t)EFX_DWORD_FIELD(dword,
			    MC_CMD_SENSOR_VALUE_ENTRY_TYPEDEF_STATE);
		}
	}

	if (stat_maskp != NULL) {
		memcpy(stat_maskp, stat_mask, sizeof (stat_mask));
	}
}

	__checkReturn			efx_rc_t
mcdi_mon_ev(
	__in				efx_nic_t *enp,
	__in				efx_qword_t *eqp,
	__out				efx_mon_stat_t *idp,
	__out				efx_mon_stat_value_t *valuep)
{
	efx_mcdi_iface_t *emip = &(enp->en_mcdi.em_emip);
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	uint16_t port_mask;
	uint16_t sensor;
	uint16_t state;
	uint16_t value;
	efx_mon_stat_t id;
	efx_rc_t rc;

	port_mask = (emip->emi_port == 1)
	    ? MCDI_MON_PORT_P1
	    : MCDI_MON_PORT_P2;

	sensor = (uint16_t)MCDI_EV_FIELD(eqp, SENSOREVT_MONITOR);
	state = (uint16_t)MCDI_EV_FIELD(eqp, SENSOREVT_STATE);
	value = (uint16_t)MCDI_EV_FIELD(eqp, SENSOREVT_VALUE);

	/* Hardware must support this MCDI sensor */
	EFSYS_ASSERT3U(sensor, <, (8 * encp->enc_mcdi_sensor_mask_size));
	EFSYS_ASSERT((sensor % MCDI_MON_PAGE_SIZE) != MC_CMD_SENSOR_PAGE0_NEXT);
	EFSYS_ASSERT(encp->enc_mcdi_sensor_maskp != NULL);
	EFSYS_ASSERT((encp->enc_mcdi_sensor_maskp[sensor / MCDI_MON_PAGE_SIZE] &
		(1U << (sensor % MCDI_MON_PAGE_SIZE))) != 0);

	/* But we don't have to understand it */
	if (sensor >= EFX_ARRAY_SIZE(mcdi_sensor_map)) {
		rc = ENOTSUP;
		goto fail1;
	}
	id = mcdi_sensor_map[sensor].msm_stat;
	if ((port_mask & mcdi_sensor_map[sensor].msm_port_mask) == 0)
		return (ENODEV);
	EFSYS_ASSERT(id < EFX_MON_NSTATS);

	*idp = id;
	valuep->emsv_value = value;
	valuep->emsv_state = state;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}


static	__checkReturn	efx_rc_t
efx_mcdi_read_sensors(
	__in		efx_nic_t *enp,
	__in		efsys_mem_t *esmp,
	__in		uint32_t size)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_READ_SENSORS_EXT_IN_LEN,
			    MC_CMD_READ_SENSORS_EXT_OUT_LEN)];
	uint32_t addr_lo, addr_hi;

	req.emr_cmd = MC_CMD_READ_SENSORS;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_READ_SENSORS_EXT_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_READ_SENSORS_EXT_OUT_LEN;

	addr_lo = (uint32_t)(EFSYS_MEM_ADDR(esmp) & 0xffffffff);
	addr_hi = (uint32_t)(EFSYS_MEM_ADDR(esmp) >> 32);

	MCDI_IN_SET_DWORD(req, READ_SENSORS_EXT_IN_DMA_ADDR_LO, addr_lo);
	MCDI_IN_SET_DWORD(req, READ_SENSORS_EXT_IN_DMA_ADDR_HI, addr_hi);
	MCDI_IN_SET_DWORD(req, READ_SENSORS_EXT_IN_LENGTH, size);

	efx_mcdi_execute(enp, &req);

	return (req.emr_rc);
}

static	__checkReturn	efx_rc_t
efx_mcdi_sensor_info_npages(
	__in		efx_nic_t *enp,
	__out		uint32_t *npagesp)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_SENSOR_INFO_EXT_IN_LEN,
			    MC_CMD_SENSOR_INFO_OUT_LENMAX)];
	int page;
	efx_rc_t rc;

	EFSYS_ASSERT(npagesp != NULL);

	page = 0;
	do {
		(void) memset(payload, 0, sizeof (payload));
		req.emr_cmd = MC_CMD_SENSOR_INFO;
		req.emr_in_buf = payload;
		req.emr_in_length = MC_CMD_SENSOR_INFO_EXT_IN_LEN;
		req.emr_out_buf = payload;
		req.emr_out_length = MC_CMD_SENSOR_INFO_OUT_LENMAX;

		MCDI_IN_SET_DWORD(req, SENSOR_INFO_EXT_IN_PAGE, page++);

		efx_mcdi_execute_quiet(enp, &req);

		if (req.emr_rc != 0) {
			rc = req.emr_rc;
			goto fail1;
		}
	} while (MCDI_OUT_DWORD(req, SENSOR_INFO_OUT_MASK) &
	    (1 << MC_CMD_SENSOR_PAGE0_NEXT));

	*npagesp = page;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

static	__checkReturn		efx_rc_t
efx_mcdi_sensor_info(
	__in			efx_nic_t *enp,
	__out_ecount(npages)	uint32_t *sensor_maskp,
	__in			size_t npages)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_SENSOR_INFO_EXT_IN_LEN,
			    MC_CMD_SENSOR_INFO_OUT_LENMAX)];
	uint32_t page;
	efx_rc_t rc;

	EFSYS_ASSERT(sensor_maskp != NULL);

	for (page = 0; page < npages; page++) {
		uint32_t mask;

		(void) memset(payload, 0, sizeof (payload));
		req.emr_cmd = MC_CMD_SENSOR_INFO;
		req.emr_in_buf = payload;
		req.emr_in_length = MC_CMD_SENSOR_INFO_EXT_IN_LEN;
		req.emr_out_buf = payload;
		req.emr_out_length = MC_CMD_SENSOR_INFO_OUT_LENMAX;

		MCDI_IN_SET_DWORD(req, SENSOR_INFO_EXT_IN_PAGE, page);

		efx_mcdi_execute(enp, &req);

		if (req.emr_rc != 0) {
			rc = req.emr_rc;
			goto fail1;
		}

		mask = MCDI_OUT_DWORD(req, SENSOR_INFO_OUT_MASK);

		if ((page != (npages - 1)) &&
		    ((mask & (1U << MC_CMD_SENSOR_PAGE0_NEXT)) == 0)) {
			rc = EINVAL;
			goto fail2;
		}
		sensor_maskp[page] = mask;
	}

	if (sensor_maskp[npages - 1] & (1U << MC_CMD_SENSOR_PAGE0_NEXT)) {
		rc = EINVAL;
		goto fail3;
	}

	return (0);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn			efx_rc_t
mcdi_mon_stats_update(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp,
	__inout_ecount(EFX_MON_NSTATS)	efx_mon_stat_value_t *values)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	uint32_t size = encp->enc_mon_stat_dma_buf_size;
	efx_rc_t rc;

	if ((rc = efx_mcdi_read_sensors(enp, esmp, size)) != 0)
		goto fail1;

	EFSYS_DMA_SYNC_FOR_KERNEL(esmp, 0, size);

	mcdi_mon_decode_stats(enp,
	    encp->enc_mcdi_sensor_maskp,
	    encp->enc_mcdi_sensor_mask_size,
	    esmp, NULL, values);

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
mcdi_mon_cfg_build(
	__in		efx_nic_t *enp)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	uint32_t npages;
	efx_rc_t rc;

	switch (enp->en_family) {
#if EFSYS_OPT_SIENA
	case EFX_FAMILY_SIENA:
		encp->enc_mon_type = EFX_MON_SFC90X0;
		break;
#endif
#if EFSYS_OPT_HUNTINGTON
	case EFX_FAMILY_HUNTINGTON:
		encp->enc_mon_type = EFX_MON_SFC91X0;
		break;
#endif
#if EFSYS_OPT_MEDFORD
	case EFX_FAMILY_MEDFORD:
		encp->enc_mon_type = EFX_MON_SFC92X0;
		break;
#endif
	default:
		rc = EINVAL;
		goto fail1;
	}

	/* Get mc sensor mask size */
	npages = 0;
	if ((rc = efx_mcdi_sensor_info_npages(enp, &npages)) != 0)
		goto fail2;

	encp->enc_mon_stat_dma_buf_size	= npages * EFX_MON_STATS_PAGE_SIZE;
	encp->enc_mcdi_sensor_mask_size = npages * sizeof (uint32_t);

	/* Allocate mc sensor mask */
	EFSYS_KMEM_ALLOC(enp->en_esip,
	    encp->enc_mcdi_sensor_mask_size,
	    encp->enc_mcdi_sensor_maskp);

	if (encp->enc_mcdi_sensor_maskp == NULL) {
		rc = ENOMEM;
		goto fail3;
	}

	/* Read mc sensor mask */
	if ((rc = efx_mcdi_sensor_info(enp,
		    encp->enc_mcdi_sensor_maskp,
		    npages)) != 0)
		goto fail4;

	/* Build monitor statistics mask */
	mcdi_mon_decode_stats(enp,
	    encp->enc_mcdi_sensor_maskp,
	    encp->enc_mcdi_sensor_mask_size,
	    NULL, encp->enc_mon_stat_mask, NULL);

	return (0);

fail4:
	EFSYS_PROBE(fail4);
	EFSYS_KMEM_FREE(enp->en_esip,
	    encp->enc_mcdi_sensor_mask_size,
	    encp->enc_mcdi_sensor_maskp);

fail3:
	EFSYS_PROBE(fail3);

fail2:
	EFSYS_PROBE(fail2);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

			void
mcdi_mon_cfg_free(
	__in		efx_nic_t *enp)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);

	if (encp->enc_mcdi_sensor_maskp != NULL) {
		EFSYS_KMEM_FREE(enp->en_esip,
		    encp->enc_mcdi_sensor_mask_size,
		    encp->enc_mcdi_sensor_maskp);
	}
}


#endif	/* EFSYS_OPT_MON_STATS */

#endif	/* EFSYS_OPT_MON_MCDI */
