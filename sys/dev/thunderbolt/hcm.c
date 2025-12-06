/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Scott Long
 * All rights reserved.
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

#include "opt_thunderbolt.h"

/* Host Configuration Manager (HCM) for USB4 and later TB3 */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/taskqueue.h>
#include <sys/gsb_crc32.h>
#include <sys/endian.h>
#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#include <machine/stdarg.h>

#include <dev/thunderbolt/nhi_reg.h>
#include <dev/thunderbolt/nhi_var.h>
#include <dev/thunderbolt/tb_reg.h>
#include <dev/thunderbolt/tb_var.h>
#include <dev/thunderbolt/tb_debug.h>
#include <dev/thunderbolt/tbcfg_reg.h>
#include <dev/thunderbolt/router_var.h>
#include <dev/thunderbolt/hcm_var.h>

static void hcm_cfg_task(void *, int);

int
hcm_attach(struct nhi_softc *nsc)
{
	struct hcm_softc *hcm;

	tb_debug(nsc, DBG_HCM|DBG_EXTRA, "hcm_attach called\n");

	hcm = malloc(sizeof(struct hcm_softc), M_THUNDERBOLT, M_NOWAIT|M_ZERO);
	if (hcm == NULL) {
		tb_debug(nsc, DBG_HCM, "Cannot allocate hcm object\n");
		return (ENOMEM);
	}

	hcm->dev = nsc->dev;
	hcm->nsc = nsc;
	nsc->hcm = hcm;

	hcm->taskqueue = taskqueue_create("hcm_event", M_NOWAIT,
	    taskqueue_thread_enqueue, &hcm->taskqueue);
	if (hcm->taskqueue == NULL)
		return (ENOMEM);
	taskqueue_start_threads(&hcm->taskqueue, 1, PI_DISK, "tbhcm%d_tq",
	    device_get_unit(nsc->dev));
	TASK_INIT(&hcm->cfg_task, 0, hcm_cfg_task, hcm);

	return (0);
}

int
hcm_detach(struct nhi_softc *nsc)
{
	struct hcm_softc *hcm;

	hcm = nsc->hcm;
	if (hcm->taskqueue)
		taskqueue_free(hcm->taskqueue);

	return (0);
}

int
hcm_router_discover(struct hcm_softc *hcm)
{

	taskqueue_enqueue(hcm->taskqueue, &hcm->cfg_task);

	return (0);
}

static void
hcm_cfg_task(void *arg, int pending)
{
	struct hcm_softc *hcm;
	struct router_softc *rsc;
	struct router_cfg_cap cap;
	struct tb_cfg_router *cfg;
	struct tb_cfg_adapter *adp;
	struct tb_cfg_cap_lane *lane;
	uint32_t *buf;
	uint8_t *u;
	u_int error, i, offset;

	hcm = (struct hcm_softc *)arg;

	tb_debug(hcm, DBG_HCM|DBG_EXTRA, "hcm_cfg_task called\n");

	buf = malloc(8 * 4, M_THUNDERBOLT, M_NOWAIT|M_ZERO);
	if (buf == NULL) {
		tb_debug(hcm, DBG_HCM, "Cannot alloc memory for discovery\n");
		return;
	}

	rsc = hcm->nsc->root_rsc;
	error = tb_config_router_read(rsc, 0, 5, buf);
	if (error != 0) {
		free(buf, M_NHI);
		return;
	}

	cfg = (struct tb_cfg_router *)buf;

	cap.space = TB_CFG_CS_ROUTER;
	cap.adap = 0;
	cap.next_cap = GET_ROUTER_CS_NEXT_CAP(cfg);
	while (cap.next_cap != 0) {
		error = tb_config_next_cap(rsc, &cap);
		if (error != 0)
			break;

		if ((cap.cap_id == TB_CFG_CAP_VSEC) && (cap.vsc_len == 0)) {
			tb_debug(hcm, DBG_HCM, "Router Cap= %d, vsec= %d, "
			    "len= %d, next_cap= %d\n", cap.cap_id,
			    cap.vsc_id, cap.vsec_len, cap.next_cap);
		} else if (cap.cap_id == TB_CFG_CAP_VSC) {
			tb_debug(hcm, DBG_HCM, "Router cap= %d, vsc= %d, "
			    "len= %d, next_cap= %d\n", cap.cap_id,
			    cap.vsc_id, cap.vsc_len, cap.next_cap);
		} else
			tb_debug(hcm, DBG_HCM, "Router cap= %d, "
			    "next_cap= %d\n", cap.cap_id, cap.next_cap);
		if (cap.next_cap > TB_CFG_CAP_OFFSET_MAX)
			cap.next_cap = 0;
	}

	u = (uint8_t *)buf;
	error = tb_config_get_lc_uuid(rsc, u);
	if (error == 0) {
		tb_debug(hcm, DBG_HCM, "Router LC UUID: %02x%02x%02x%02x-"
		    "%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
		    u[0], u[1], u[2], u[3], u[4], u[5], u[6], u[7], u[8],
		    u[9], u[10], u[11], u[12], u[13], u[14], u[15]);
	} else
		tb_printf(hcm, "Error finding LC registers: %d\n", error);

	for (i = 1; i <= rsc->max_adap; i++) {
		error = tb_config_adapter_read(rsc, i, 0, 8, buf);
		if (error != 0) {
			tb_debug(hcm, DBG_HCM, "Adapter %d: no adapter\n", i);
			continue;
		}
		adp = (struct tb_cfg_adapter *)buf;
		tb_debug(hcm, DBG_HCM, "Adapter %d: %s, max_counters= 0x%08x,"
		    " adapter_num= %d\n", i,
		    tb_get_string(GET_ADP_CS_TYPE(adp), tb_adapter_type),
		    GET_ADP_CS_MAX_COUNTERS(adp), GET_ADP_CS_ADP_NUM(adp));

		if (GET_ADP_CS_TYPE(adp) != ADP_CS2_LANE)
			continue;

		error = tb_config_find_adapter_cap(rsc, i, TB_CFG_CAP_LANE,
		    &offset);
		if (error)
			continue;

		error = tb_config_adapter_read(rsc, i, offset, 3, buf);
		if (error)
			continue;

		lane = (struct tb_cfg_cap_lane *)buf;
		tb_debug(hcm, DBG_HCM, "Lane Adapter State= %s %s\n",
		    tb_get_string((lane->current_lws & CAP_LANE_STATE_MASK),
		    tb_adapter_state), (lane->targ_lwp & CAP_LANE_DISABLE) ?
		    "disabled" : "enabled");

		if ((lane->current_lws & CAP_LANE_STATE_MASK) ==
		    CAP_LANE_STATE_CL0) {
			tb_route_t newr;

			newr.hi = rsc->route.hi;
			newr.lo = rsc->route.lo | (i << rsc->depth * 8);

			tb_printf(hcm, "want to add router at 0x%08x%08x\n",
			    newr.hi, newr.lo);
			error = tb_router_attach(rsc, newr);
			tb_printf(rsc, "tb_router_attach returned %d\n", error);
		}
	}

	free(buf, M_THUNDERBOLT);
}
