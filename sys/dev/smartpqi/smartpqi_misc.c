/*-
 * Copyright 2016-2023 Microchip Technology, Inc. and/or its subsidiaries.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
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


#include "smartpqi_includes.h"

/*
 * Populate hostwellness time variables in bcd format from FreeBSD format.
 */
void
os_get_time(struct bmic_host_wellness_time *host_wellness_time)
{
	struct timespec ts;
	struct clocktime ct = {0};

	getnanotime(&ts);
	clock_ts_to_ct(&ts, &ct);

	/* Fill the time In BCD Format */
	host_wellness_time->hour= (uint8_t)bin2bcd(ct.hour);
	host_wellness_time->min = (uint8_t)bin2bcd(ct.min);
	host_wellness_time->sec= (uint8_t)bin2bcd(ct.sec);
	host_wellness_time->reserved = 0;
	host_wellness_time->month = (uint8_t)bin2bcd(ct.mon);
	host_wellness_time->day = (uint8_t)bin2bcd(ct.day);
	host_wellness_time->century = (uint8_t)bin2bcd(ct.year / 100);
	host_wellness_time->year = (uint8_t)bin2bcd(ct.year % 100);

}

/*
 * Update host time to f/w every 24 hours in a periodic timer.
 */

void
os_wellness_periodic(void *data)
{
	struct pqisrc_softstate *softs = (struct pqisrc_softstate *)data;
	int ret = 0;

	/* update time to FW */
	if (!pqisrc_ctrl_offline(softs)){
		if( (ret = pqisrc_write_current_time_to_host_wellness(softs)) != 0 )
			DBG_ERR("Failed to update time to FW in periodic ret = %d\n", ret);
	}

	/* reschedule ourselves */
	callout_reset(&softs->os_specific.wellness_periodic,
			PQI_HOST_WELLNESS_TIMEOUT_SEC * hz, os_wellness_periodic, softs);
}

/*
 * Routine used to stop the heart-beat timer
 */
void
os_stop_heartbeat_timer(pqisrc_softstate_t *softs)
{
	DBG_FUNC("IN\n");

	/* Kill the heart beat event */
	callout_stop(&softs->os_specific.heartbeat_timeout_id);

	DBG_FUNC("OUT\n");
}

/*
 * Routine used to start the heart-beat timer
 */
void
os_start_heartbeat_timer(void *data)
{
	struct pqisrc_softstate *softs = (struct pqisrc_softstate *)data;
	DBG_FUNC("IN\n");

	pqisrc_heartbeat_timer_handler(softs);
	if (!pqisrc_ctrl_offline(softs)) {
		callout_reset(&softs->os_specific.heartbeat_timeout_id,
				PQI_HEARTBEAT_TIMEOUT_SEC * hz,
				os_start_heartbeat_timer, softs);
	}

       DBG_FUNC("OUT\n");
}

/*
 * Mutex initialization function
 */
int
os_init_spinlock(struct pqisrc_softstate *softs, struct mtx *lock,
			char *lockname)
{
    mtx_init(lock, lockname, NULL, MTX_SPIN);
    return 0;

}

/*
 * Mutex uninitialization function
 */
void
os_uninit_spinlock(struct mtx *lock)
{
	mtx_destroy(lock);
	return;
}

/*
 * Semaphore initialization function
 */
int
os_create_semaphore(const char *name, int value, struct sema *sema)
{
	sema_init(sema, value, name);
	return PQI_STATUS_SUCCESS;
}

/*
 * Semaphore uninitialization function
 */
int
os_destroy_semaphore(struct sema *sema)
{
	sema_destroy(sema);
	return PQI_STATUS_SUCCESS;
}

/*
 * Semaphore grab function
 */
void inline
os_sema_lock(struct sema *sema)
{
	sema_post(sema);
}

/*
 * Semaphore release function
 */
void inline
os_sema_unlock(struct sema *sema)
{
	sema_wait(sema);
}

/*
 * string copy wrapper function
 */
int
os_strlcpy(char *dst, char *src, int size)
{
	return strlcpy(dst, src, size);
}

int
bsd_status_to_pqi_status(int bsd_status)
{
	if (bsd_status == BSD_SUCCESS)
		return PQI_STATUS_SUCCESS;
	else
		return PQI_STATUS_FAILURE;
}

/* Return true : If the feature is disabled from device hints.
 * Return false : If the feature is enabled from device hints.
 * Return default: The feature status is not deciding from hints.
 * */
boolean_t
check_device_hint_status(struct pqisrc_softstate *softs, unsigned int feature_bit)
{
	DBG_FUNC("IN\n");

	switch(feature_bit) {
		case PQI_FIRMWARE_FEATURE_RAID_1_WRITE_BYPASS:
			if (!softs->hint.aio_raid1_write_status)
				return true;
			break;
		case PQI_FIRMWARE_FEATURE_RAID_5_WRITE_BYPASS:
			if (!softs->hint.aio_raid5_write_status)
				return true;
			break;
		case PQI_FIRMWARE_FEATURE_RAID_6_WRITE_BYPASS:
			if (!softs->hint.aio_raid6_write_status)
				return true;
			break;
		case PQI_FIRMWARE_FEATURE_UNIQUE_SATA_WWN:
			if (!softs->hint.sata_unique_wwn_status)
				return true;
			break;
		default:
			return false;
	}

	DBG_FUNC("OUT\n");

	return false;
}

static void
bsd_set_hint_adapter_queue_depth(struct pqisrc_softstate *softs)
{
	uint32_t queue_depth = softs->pqi_cap.max_outstanding_io;

	DBG_FUNC("IN\n");

	if ((!softs->hint.queue_depth) || (softs->hint.queue_depth >
			 softs->pqi_cap.max_outstanding_io)) {
		/* Nothing to do here. Supported queue depth
		 * is already set by controller/driver */
	}
	else if (softs->hint.queue_depth < PQISRC_MIN_OUTSTANDING_REQ) {
		/* Nothing to do here. Supported queue depth
		 * is already set by controller/driver */
	}
	else {
		/* Set Device.Hint queue depth here */
		softs->pqi_cap.max_outstanding_io =
			softs->hint.queue_depth;
	}

	DBG_NOTE("Adapter queue depth before hint set = %u, Queue depth after hint set = %u\n",
			queue_depth, softs->pqi_cap.max_outstanding_io);

	DBG_FUNC("OUT\n");
}

static void
bsd_set_hint_scatter_gather_config(struct pqisrc_softstate *softs)
{
	uint32_t pqi_sg_segments = softs->pqi_cap.max_sg_elem;

	DBG_FUNC("IN\n");

	/* At least > 16 sg's required to wotk hint correctly.
	 * Default the sg count set by driver/controller. */

	if ((!softs->hint.sg_segments) || (softs->hint.sg_segments >
			 softs->pqi_cap.max_sg_elem)) {
		/* Nothing to do here. Supported sg count
		 * is already set by controller/driver. */
	}
	else if (softs->hint.sg_segments < BSD_MIN_SG_SEGMENTS)
	{
		/* Nothing to do here. Supported sg count
		 * is already set by controller/driver. */
	}
	else {
		/* Set Device.Hint sg count here */
		softs->pqi_cap.max_sg_elem = softs->hint.sg_segments;
	}

	DBG_NOTE("SG segments before hint set = %u, SG segments after hint set = %u\n",
			pqi_sg_segments, softs->pqi_cap.max_sg_elem);

	DBG_FUNC("OUT\n");
}

void
bsd_set_hint_adapter_cap(struct pqisrc_softstate *softs)
{
	DBG_FUNC("IN\n");

	bsd_set_hint_adapter_queue_depth(softs);
	bsd_set_hint_scatter_gather_config(softs);

	DBG_FUNC("OUT\n");
}

void
bsd_set_hint_adapter_cpu_config(struct pqisrc_softstate *softs)
{
	DBG_FUNC("IN\n");

	/* online cpu count decides the no.of queues the driver can create,
	 * and msi interrupt count as well.
	 * If the cpu count is "zero" set by hint file then the driver
	 * can have "one" queue and "one" legacy interrupt. (It shares event queue for
	 * operational IB queue).
	 * Check for os_get_intr_config function for interrupt assignment.*/

	if (softs->hint.cpu_count > softs->num_cpus_online) {
		/* Nothing to do here. Supported cpu count
		 * already fetched from hardware */
	}
	else {
		/* Set Device.Hint cpu count here */
		softs->num_cpus_online = softs->hint.cpu_count;
	}

	DBG_FUNC("OUT\n");
}
