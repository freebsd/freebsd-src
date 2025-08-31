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
ufshci_ctrlr_enable_host_ctrlr(struct ufshci_controller *ctrlr)
{
	int timeout = ticks + MSEC_2_TICKS(ctrlr->device_init_timeout_in_ms);
	sbintime_t delta_t = SBT_1US;
	uint32_t hce;

	hce = ufshci_mmio_read_4(ctrlr, hce);

	/* If UFS host controller is already enabled, disable it. */
	if (UFSHCIV(UFSHCI_HCE_REG_HCE, hce)) {
		hce &= ~UFSHCIM(UFSHCI_HCE_REG_HCE);
		ufshci_mmio_write_4(ctrlr, hce, hce);
	}

	/* Enable UFS host controller */
	hce |= UFSHCIM(UFSHCI_HCE_REG_HCE);
	ufshci_mmio_write_4(ctrlr, hce, hce);

	/*
	 * During the controller initialization, the value of the HCE bit is
	 * unstable, so we need to read the HCE value after some time after
	 * initialization is complete.
	 */
	pause_sbt("ufshci_hce", ustosbt(100), 0, C_PREL(1));

	/* Wait for the HCE flag to change */
	while (1) {
		hce = ufshci_mmio_read_4(ctrlr, hce);
		if (UFSHCIV(UFSHCI_HCE_REG_HCE, hce))
			break;
		if (timeout - ticks < 0) {
			ufshci_printf(ctrlr,
			    "host controller failed to enable "
			    "within %d ms\n",
			    ctrlr->device_init_timeout_in_ms);
			return (ENXIO);
		}

		pause_sbt("ufshci_hce", delta_t, 0, C_PREL(1));
		delta_t = min(SBT_1MS, delta_t * 3 / 2);
	}

	return (0);
}

int
ufshci_ctrlr_construct(struct ufshci_controller *ctrlr, device_t dev)
{
	uint32_t ver, cap, hcs, ie, ahit;
	uint32_t timeout_period, retry_count;
	int error;

	ctrlr->device_init_timeout_in_ms = UFSHCI_DEVICE_INIT_TIMEOUT_MS;
	ctrlr->uic_cmd_timeout_in_ms = UFSHCI_UIC_CMD_TIMEOUT_MS;
	ctrlr->dev = dev;
	ctrlr->sc_unit = device_get_unit(dev);

	snprintf(ctrlr->sc_name, sizeof(ctrlr->sc_name), "%s",
	    device_get_nameunit(dev));

	mtx_init(&ctrlr->sc_mtx, device_get_nameunit(dev), NULL,
	    MTX_DEF | MTX_RECURSE);

	mtx_init(&ctrlr->uic_cmd_lock, "ufshci ctrlr uic cmd lock", NULL,
	    MTX_DEF);

	ver = ufshci_mmio_read_4(ctrlr, ver);
	ctrlr->major_version = UFSHCIV(UFSHCI_VER_REG_MJR, ver);
	ctrlr->minor_version = UFSHCIV(UFSHCI_VER_REG_MNR, ver);
	ufshci_printf(ctrlr, "UFSHCI Version: %d.%d\n", ctrlr->major_version,
	    ctrlr->minor_version);

	/* Read Device Capabilities */
	ctrlr->cap = cap = ufshci_mmio_read_4(ctrlr, cap);
	ctrlr->is_single_db_supported = UFSHCIV(UFSHCI_CAP_REG_LSDBS, cap);
	/*
	 * TODO: This driver does not yet support multi-queue.
	 * Check the UFSHCI_CAP_REG_MCQS bit in the future to determine if
	 * multi-queue support is available.
	 */
	ctrlr->is_mcq_supported = false;
	if (!(ctrlr->is_single_db_supported == 0 || ctrlr->is_mcq_supported))
		return (ENXIO);
	/*
	 * The maximum transfer size supported by UFSHCI spec is 65535 * 256 KiB
	 * However, we limit the maximum transfer size to 1MiB(256 * 4KiB) for
	 * performance reason.
	 */
	ctrlr->page_size = PAGE_SIZE;
	ctrlr->max_xfer_size = ctrlr->page_size * UFSHCI_MAX_PRDT_ENTRY_COUNT;

	timeout_period = UFSHCI_DEFAULT_TIMEOUT_PERIOD;
	TUNABLE_INT_FETCH("hw.ufshci.timeout_period", &timeout_period);
	timeout_period = min(timeout_period, UFSHCI_MAX_TIMEOUT_PERIOD);
	timeout_period = max(timeout_period, UFSHCI_MIN_TIMEOUT_PERIOD);
	ctrlr->timeout_period = timeout_period;

	retry_count = UFSHCI_DEFAULT_RETRY_COUNT;
	TUNABLE_INT_FETCH("hw.ufshci.retry_count", &retry_count);
	ctrlr->retry_count = retry_count;

	/* Disable all interrupts */
	ufshci_mmio_write_4(ctrlr, ie, 0);

	/* Enable Host Controller */
	error = ufshci_ctrlr_enable_host_ctrlr(ctrlr);
	if (error)
		return (error);

	/* Send DME_LINKSTARTUP command to start the link startup procedure */
	error = ufshci_uic_send_dme_link_startup(ctrlr);
	if (error)
		return (error);

	/* Read the UECPA register to clear */
	ufshci_mmio_read_4(ctrlr, uecpa);

	/* Diable Auto-hibernate */
	ahit = 0;
	ufshci_mmio_write_4(ctrlr, ahit, ahit);

	/*
	 * The device_present(UFSHCI_HCS_REG_DP) bit becomes true if the host
	 * controller has successfully received a Link Startup UIC command
	 * response and the UFS device has found a physical link to the
	 * controller.
	 */
	hcs = ufshci_mmio_read_4(ctrlr, hcs);
	if (!UFSHCIV(UFSHCI_HCS_REG_DP, hcs)) {
		ufshci_printf(ctrlr, "UFS device not found\n");
		return (ENXIO);
	}

	/* Allocate and initialize UTP Task Management Request List. */
	error = ufshci_utmr_req_queue_construct(ctrlr);
	if (error)
		return (error);

	/* Allocate and initialize UTP Transfer Request List or SQ/CQ. */
	error = ufshci_utr_req_queue_construct(ctrlr);
	if (error)
		return (error);

	/* Enable additional interrupts by programming the IE register. */
	ie = ufshci_mmio_read_4(ctrlr, ie);
	ie |= UFSHCIM(UFSHCI_IE_REG_UTRCE);  /* UTR Completion */
	ie |= UFSHCIM(UFSHCI_IE_REG_UEE);    /* UIC Error */
	ie |= UFSHCIM(UFSHCI_IE_REG_UTMRCE); /* UTMR Completion */
	ie |= UFSHCIM(UFSHCI_IE_REG_DFEE);   /* Device Fatal Error */
	ie |= UFSHCIM(UFSHCI_IE_REG_UTPEE);  /* UTP Error */
	ie |= UFSHCIM(UFSHCI_IE_REG_HCFEE);  /* Host Ctrlr Fatal Error */
	ie |= UFSHCIM(UFSHCI_IE_REG_SBFEE);  /* System Bus Fatal Error */
	ie |= UFSHCIM(UFSHCI_IE_REG_CEFEE);  /* Crypto Engine Fatal Error */
	ufshci_mmio_write_4(ctrlr, ie, ie);

	/* TODO: Initialize interrupt Aggregation Control Register (UTRIACR) */

	/* TODO: Separate IO and Admin slot */
	/*
	 * max_hw_pend_io is the number of slots in the transfer_req_queue.
	 * Reduce num_entries by one to reserve an admin slot.
	 */
	ctrlr->max_hw_pend_io = ctrlr->transfer_req_queue.num_entries - 1;

	return (0);
}

void
ufshci_ctrlr_destruct(struct ufshci_controller *ctrlr, device_t dev)
{
	if (ctrlr->resource == NULL)
		goto nores;

	/* TODO: Flush In-flight IOs */

	/* Release resources */
	ufshci_utmr_req_queue_destroy(ctrlr);
	ufshci_utr_req_queue_destroy(ctrlr);

	if (ctrlr->tag)
		bus_teardown_intr(ctrlr->dev, ctrlr->res, ctrlr->tag);

	if (ctrlr->res)
		bus_release_resource(ctrlr->dev, SYS_RES_IRQ,
		    rman_get_rid(ctrlr->res), ctrlr->res);

	mtx_lock(&ctrlr->sc_mtx);

	ufshci_sim_detach(ctrlr);

	mtx_unlock(&ctrlr->sc_mtx);

	bus_release_resource(dev, SYS_RES_MEMORY, ctrlr->resource_id,
	    ctrlr->resource);
nores:
	mtx_destroy(&ctrlr->uic_cmd_lock);
	mtx_destroy(&ctrlr->sc_mtx);

	return;
}

int
ufshci_ctrlr_reset(struct ufshci_controller *ctrlr)
{
	uint32_t ie;
	int error;

	/* Backup and disable all interrupts */
	ie = ufshci_mmio_read_4(ctrlr, ie);
	ufshci_mmio_write_4(ctrlr, ie, 0);

	/* Release resources */
	ufshci_utmr_req_queue_destroy(ctrlr);
	ufshci_utr_req_queue_destroy(ctrlr);

	/* Reset Host Controller */
	error = ufshci_ctrlr_enable_host_ctrlr(ctrlr);
	if (error)
		return (error);

	/* Send DME_LINKSTARTUP command to start the link startup procedure */
	error = ufshci_uic_send_dme_link_startup(ctrlr);
	if (error)
		return (error);

	/* Enable interrupts */
	ufshci_mmio_write_4(ctrlr, ie, ie);

	/* Allocate and initialize UTP Task Management Request List. */
	error = ufshci_utmr_req_queue_construct(ctrlr);
	if (error)
		return (error);

	/* Allocate and initialize UTP Transfer Request List or SQ/CQ. */
	error = ufshci_utr_req_queue_construct(ctrlr);
	if (error)
		return (error);

	return (0);
}

int
ufshci_ctrlr_submit_task_mgmt_request(struct ufshci_controller *ctrlr,
    struct ufshci_request *req)
{
	return (
	    ufshci_req_queue_submit_request(&ctrlr->task_mgmt_req_queue, req,
		/*is_admin*/ false));
}

int
ufshci_ctrlr_submit_admin_request(struct ufshci_controller *ctrlr,
    struct ufshci_request *req)
{
	return (ufshci_req_queue_submit_request(&ctrlr->transfer_req_queue, req,
	    /*is_admin*/ true));
}

int
ufshci_ctrlr_submit_io_request(struct ufshci_controller *ctrlr,
    struct ufshci_request *req)
{
	return (ufshci_req_queue_submit_request(&ctrlr->transfer_req_queue, req,
	    /*is_admin*/ false));
}

int
ufshci_ctrlr_send_nop(struct ufshci_controller *ctrlr)
{
	struct ufshci_completion_poll_status status;

	status.done = 0;
	ufshci_ctrlr_cmd_send_nop(ctrlr, ufshci_completion_poll_cb, &status);
	ufshci_completion_poll(&status);
	if (status.error) {
		ufshci_printf(ctrlr, "ufshci_ctrlr_send_nop failed!\n");
		return (ENXIO);
	}

	return (0);
}

static void
ufshci_ctrlr_fail(struct ufshci_controller *ctrlr, bool admin_also)
{
	printf("ufshci(4): ufshci_ctrlr_fail\n");

	ctrlr->is_failed = true;

	/* TODO: task_mgmt_req_queue should be handled as fail */

	ufshci_req_queue_fail(ctrlr,
	    &ctrlr->transfer_req_queue.hwq[UFSHCI_SDB_Q]);
}

static void
ufshci_ctrlr_start(struct ufshci_controller *ctrlr)
{
	TSENTER();

	if (ufshci_ctrlr_send_nop(ctrlr) != 0) {
		ufshci_ctrlr_fail(ctrlr, false);
		return;
	}

	/* Initialize UFS target drvice */
	if (ufshci_dev_init(ctrlr) != 0) {
		ufshci_ctrlr_fail(ctrlr, false);
		return;
	}

	/* Initialize Reference Clock */
	if (ufshci_dev_init_reference_clock(ctrlr) != 0) {
		ufshci_ctrlr_fail(ctrlr, false);
		return;
	}

	/* Initialize unipro */
	if (ufshci_dev_init_unipro(ctrlr) != 0) {
		ufshci_ctrlr_fail(ctrlr, false);
		return;
	}

	/*
	 * Initialize UIC Power Mode
	 * QEMU UFS devices do not support unipro and power mode.
	 */
	if (!(ctrlr->quirks & UFSHCI_QUIRK_IGNORE_UIC_POWER_MODE) &&
	    ufshci_dev_init_uic_power_mode(ctrlr) != 0) {
		ufshci_ctrlr_fail(ctrlr, false);
		return;
	}

	/* Initialize UFS Power Mode */
	if (ufshci_dev_init_ufs_power_mode(ctrlr) != 0) {
		ufshci_ctrlr_fail(ctrlr, false);
		return;
	}

	/* Read Controller Descriptor (Device, Geometry) */
	if (ufshci_dev_get_descriptor(ctrlr) != 0) {
		ufshci_ctrlr_fail(ctrlr, false);
		return;
	}

	if (ufshci_dev_config_write_booster(ctrlr)) {
		ufshci_ctrlr_fail(ctrlr, false);
		return;
	}

	/* TODO: Configure Background Operations */

	if (ufshci_sim_attach(ctrlr) != 0) {
		ufshci_ctrlr_fail(ctrlr, false);
		return;
	}

	TSEXIT();
}

void
ufshci_ctrlr_start_config_hook(void *arg)
{
	struct ufshci_controller *ctrlr = arg;

	TSENTER();

	if (ufshci_utmr_req_queue_enable(ctrlr) == 0 &&
	    ufshci_utr_req_queue_enable(ctrlr) == 0)
		ufshci_ctrlr_start(ctrlr);
	else
		ufshci_ctrlr_fail(ctrlr, false);

	ufshci_sysctl_initialize_ctrlr(ctrlr);
	config_intrhook_disestablish(&ctrlr->config_hook);

	TSEXIT();
}

/*
 * Poll all the queues enabled on the device for completion.
 */
void
ufshci_ctrlr_poll(struct ufshci_controller *ctrlr)
{
	uint32_t is;

	is = ufshci_mmio_read_4(ctrlr, is);

	/* UIC error */
	if (is & UFSHCIM(UFSHCI_IS_REG_UE)) {
		uint32_t uecpa, uecdl, uecn, uect, uecdme;

		/* UECPA for Host UIC Error Code within PHY Adapter Layer */
		uecpa = ufshci_mmio_read_4(ctrlr, uecpa);
		if (uecpa & UFSHCIM(UFSHCI_UECPA_REG_ERR)) {
			ufshci_printf(ctrlr, "UECPA error code: 0x%x\n",
			    UFSHCIV(UFSHCI_UECPA_REG_EC, uecpa));
		}
		/* UECDL for Host UIC Error Code within Data Link Layer */
		uecdl = ufshci_mmio_read_4(ctrlr, uecdl);
		if (uecdl & UFSHCIM(UFSHCI_UECDL_REG_ERR)) {
			ufshci_printf(ctrlr, "UECDL error code: 0x%x\n",
			    UFSHCIV(UFSHCI_UECDL_REG_EC, uecdl));
		}
		/* UECN for Host UIC Error Code within Network Layer */
		uecn = ufshci_mmio_read_4(ctrlr, uecn);
		if (uecn & UFSHCIM(UFSHCI_UECN_REG_ERR)) {
			ufshci_printf(ctrlr, "UECN error code: 0x%x\n",
			    UFSHCIV(UFSHCI_UECN_REG_EC, uecn));
		}
		/* UECT for Host UIC Error Code within Transport Layer */
		uect = ufshci_mmio_read_4(ctrlr, uect);
		if (uect & UFSHCIM(UFSHCI_UECT_REG_ERR)) {
			ufshci_printf(ctrlr, "UECT error code: 0x%x\n",
			    UFSHCIV(UFSHCI_UECT_REG_EC, uect));
		}
		/* UECDME for Host UIC Error Code within DME subcomponent */
		uecdme = ufshci_mmio_read_4(ctrlr, uecdme);
		if (uecdme & UFSHCIM(UFSHCI_UECDME_REG_ERR)) {
			ufshci_printf(ctrlr, "UECDME error code: 0x%x\n",
			    UFSHCIV(UFSHCI_UECDME_REG_EC, uecdme));
		}
		ufshci_mmio_write_4(ctrlr, is, UFSHCIM(UFSHCI_IS_REG_UE));
	}
	/* Device Fatal Error Status */
	if (is & UFSHCIM(UFSHCI_IS_REG_DFES)) {
		ufshci_printf(ctrlr, "Device fatal error on ISR\n");
		ufshci_mmio_write_4(ctrlr, is, UFSHCIM(UFSHCI_IS_REG_DFES));
	}
	/* UTP Error Status */
	if (is & UFSHCIM(UFSHCI_IS_REG_UTPES)) {
		ufshci_printf(ctrlr, "UTP error on ISR\n");
		ufshci_mmio_write_4(ctrlr, is, UFSHCIM(UFSHCI_IS_REG_UTPES));
	}
	/* Host Controller Fatal Error Status */
	if (is & UFSHCIM(UFSHCI_IS_REG_HCFES)) {
		ufshci_printf(ctrlr, "Host controller fatal error on ISR\n");
		ufshci_mmio_write_4(ctrlr, is, UFSHCIM(UFSHCI_IS_REG_HCFES));
	}
	/* System Bus Fatal Error Status */
	if (is & UFSHCIM(UFSHCI_IS_REG_SBFES)) {
		ufshci_printf(ctrlr, "System bus fatal error on ISR\n");
		ufshci_mmio_write_4(ctrlr, is, UFSHCIM(UFSHCI_IS_REG_SBFES));
	}
	/* Crypto Engine Fatal Error Status */
	if (is & UFSHCIM(UFSHCI_IS_REG_CEFES)) {
		ufshci_printf(ctrlr, "Crypto engine fatal error on ISR\n");
		ufshci_mmio_write_4(ctrlr, is, UFSHCIM(UFSHCI_IS_REG_CEFES));
	}
	/* UTP Task Management Request Completion Status */
	if (is & UFSHCIM(UFSHCI_IS_REG_UTMRCS)) {
		ufshci_mmio_write_4(ctrlr, is, UFSHCIM(UFSHCI_IS_REG_UTMRCS));
		ufshci_req_queue_process_completions(
		    &ctrlr->task_mgmt_req_queue);
	}
	/* UTP Transfer Request Completion Status */
	if (is & UFSHCIM(UFSHCI_IS_REG_UTRCS)) {
		ufshci_mmio_write_4(ctrlr, is, UFSHCIM(UFSHCI_IS_REG_UTRCS));
		ufshci_req_queue_process_completions(
		    &ctrlr->transfer_req_queue);
	}
	/* MCQ CQ Event Status */
	if (is & UFSHCIM(UFSHCI_IS_REG_CQES)) {
		/* TODO: We need to process completion Queue Pairs */
		ufshci_printf(ctrlr, "MCQ completion not yet implemented\n");
		ufshci_mmio_write_4(ctrlr, is, UFSHCIM(UFSHCI_IS_REG_CQES));
	}
}

/*
 * Poll the single-vector interrupt case: num_io_queues will be 1 and
 * there's only a single vector. While we're polling, we mask further
 * interrupts in the controller.
 */
void
ufshci_ctrlr_shared_handler(void *arg)
{
	struct ufshci_controller *ctrlr = arg;

	ufshci_ctrlr_poll(ctrlr);
}

void
ufshci_reg_dump(struct ufshci_controller *ctrlr)
{
	ufshci_printf(ctrlr, "========= UFSHCI Register Dump =========\n");

	UFSHCI_DUMP_REG(ctrlr, cap);
	UFSHCI_DUMP_REG(ctrlr, mcqcap);
	UFSHCI_DUMP_REG(ctrlr, ver);
	UFSHCI_DUMP_REG(ctrlr, ext_cap);
	UFSHCI_DUMP_REG(ctrlr, hcpid);
	UFSHCI_DUMP_REG(ctrlr, hcmid);
	UFSHCI_DUMP_REG(ctrlr, ahit);
	UFSHCI_DUMP_REG(ctrlr, is);
	UFSHCI_DUMP_REG(ctrlr, ie);
	UFSHCI_DUMP_REG(ctrlr, hcsext);
	UFSHCI_DUMP_REG(ctrlr, hcs);
	UFSHCI_DUMP_REG(ctrlr, hce);
	UFSHCI_DUMP_REG(ctrlr, uecpa);
	UFSHCI_DUMP_REG(ctrlr, uecdl);
	UFSHCI_DUMP_REG(ctrlr, uecn);
	UFSHCI_DUMP_REG(ctrlr, uect);
	UFSHCI_DUMP_REG(ctrlr, uecdme);

	ufshci_printf(ctrlr, "========================================\n");
}
