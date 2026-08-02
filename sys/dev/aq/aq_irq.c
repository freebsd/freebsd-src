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
 *   (3)The name of the author may not be used to endorse or promote
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
 */


#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bitstring.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/iflib.h>

#include "aq_common.h"
#include "aq_device.h"
#include "aq_ring.h"
#include "aq_dbg.h"
#include "aq_hw.h"
#include "aq_hw_llh.h"
#include "aq_fw.h"

int
aq_update_hw_stats(struct aq_dev *aq_dev)
{
	struct aq_hw *hw = &aq_dev->hw;
	struct aq_hw_stats stats;

	memset(&stats, 0, sizeof(stats));
	if (aq_hw_mpi_read_stats(hw, &stats) != 0)
		return (0);

#define AQ_SDELTA(_N_) do { \
	aq_dev->curr_stats._N_ += stats._N_ - aq_dev->last_stats._N_; \
} while (0)
	if (aq_dev->linkup) {
		AQ_SDELTA(uprc);
		AQ_SDELTA(mprc);
		AQ_SDELTA(bprc);
		AQ_SDELTA(cprc);
		AQ_SDELTA(erpt);

		AQ_SDELTA(uptc);
		AQ_SDELTA(mptc);
		AQ_SDELTA(bptc);
		AQ_SDELTA(erpr);

		AQ_SDELTA(ubrc);
		AQ_SDELTA(ubtc);
		AQ_SDELTA(mbrc);
		AQ_SDELTA(mbtc);
		AQ_SDELTA(bbrc);
		AQ_SDELTA(bbtc);

		AQ_SDELTA(ptc);
		AQ_SDELTA(prc);

		AQ_SDELTA(dpc);

		/*
		 * Per-cast octets present: derive the aggregate from them.
		 * Otherwise (B0) accumulate the firmware-reported aggregate.
		 */
		if (stats.ubrc | stats.mbrc | stats.bbrc)
			aq_dev->curr_stats.brc = aq_dev->curr_stats.ubrc +
			    aq_dev->curr_stats.mbrc + aq_dev->curr_stats.bbrc;
		else
			AQ_SDELTA(brc);

		if (stats.ubtc | stats.mbtc | stats.bbtc)
			aq_dev->curr_stats.btc = aq_dev->curr_stats.ubtc +
			    aq_dev->curr_stats.mbtc + aq_dev->curr_stats.bbtc;
		else
			AQ_SDELTA(btc);
	}
#undef AQ_SDELTA

	memcpy(&aq_dev->last_stats, &stats, sizeof(stats));

	return (0);
}


#define	AQ_THERMAL_HYSTERESIS_MC	18000	/* recover this far below the limit */
#define	AQ_THERMAL_RECOVER_MC	90000	/* fallback when the limit is unreadable */
#define	AQ_THERMAL_SETTLE_POLLS	5	/* ~5 s for the PHY reset to settle */
#define	AQ_THERMAL_RETRY_SECS	60	/* minimum spacing between recoveries */
#define	AQ_INIT_MAX_RETRIES	5	/* re-init attempts after a failed init */

/* Temperature here is post-trip; the PHY is already dropping to low power. */
static void
aq_thermal_report_shutdown(struct aq_dev *aq_dev)
{
	struct aq_hw *hw = &aq_dev->hw;
	int temp_mc, limit_mc;
	bool have_temp, have_limit;

	have_temp = hw->fw_ops->get_temp(hw, &temp_mc) == 0;
	have_limit = hw->fw_ops->get_thermal_limit(hw, &limit_mc) == 0;
	if (have_temp)
		aq_dev->thermal_temp_mc = temp_mc;
	/* The F/W also exposes a cold_temperature hysteresis point. */
	aq_dev->thermal_recover_mc = have_limit ?
	    limit_mc - AQ_THERMAL_HYSTERESIS_MC : AQ_THERMAL_RECOVER_MC;

	if (have_temp && have_limit)
		device_printf(aq_dev->dev, "PHY thermal shutdown; "
		    "limit %d C, temp %d C; holding link down until it cools\n",
		    limit_mc / 1000, temp_mc / 1000);
	else if (have_temp)
		device_printf(aq_dev->dev, "PHY thermal shutdown; "
		    "temp %d C; holding link down until it cools\n",
		    temp_mc / 1000);
	else
		device_printf(aq_dev->dev, "PHY thermal shutdown; "
		    "holding link down until it cools\n");
}

/* Recover after cooldown: A1 needs a PHY reset then re-init, A2 re-inits alone. */
static void
aq_thermal_poll(struct aq_dev *aq_dev)
{
	struct aq_hw *hw = &aq_dev->hw;
	uint16_t fault;
	int temp_mc;

	switch (aq_dev->thermal_state) {
	case AQ_THERMAL_NORMAL:
		if (aq_dev->linkup)
			return;
		/* The F/W raises the fault a poll after it drops the link. */
		if (hw->fw_ops->get_phy_fault(hw, &fault) != 0 || fault == 0)
			return;
		/* Report each code once; do not mask a later shutdown. */
		if (fault == aq_dev->phy_fault_last)
			return;
		aq_dev->phy_fault_last = fault;
		if (fault != AQ_PHY_FAULT_THERMAL_SHUTDOWN) {
			device_printf(aq_dev->dev,
			    "PHY fault 0x%04x\n", fault);
			return;
		}
		aq_thermal_report_shutdown(aq_dev);
		aq_dev->thermal_state = AQ_THERMAL_COOLING;
		return;

	case AQ_THERMAL_COOLING:
		if (hw->fw_ops->get_temp(hw, &temp_mc) != 0 ||
		    temp_mc > aq_dev->thermal_recover_mc)
			return;
		aq_dev->thermal_temp_mc = temp_mc;
		if (hw->fw_ops->phy_reset != NULL) {
			if (hw->fw_ops->phy_reset(hw) != 0)
				return;
			aq_dev->thermal_settle = 0;
			aq_dev->thermal_state = AQ_THERMAL_SETTLING;
			return;
		}
		/* No PHY reset needed (A2): re-init below restores the link. */
		break;

	case AQ_THERMAL_SETTLING:
		if (++aq_dev->thermal_settle < AQ_THERMAL_SETTLE_POLLS)
			return;
		break;
	}

	/* Space attempts out: recovery costs a re-init and a renegotiation. */
	if ((int)(ticks - aq_dev->thermal_retry_ticks) < 0)
		return;
	aq_dev->thermal_retry_ticks = ticks + AQ_THERMAL_RETRY_SECS * hz;

	device_printf(aq_dev->dev, "PHY cooled to %d C; restoring "
	    "link\n", aq_dev->thermal_temp_mc / 1000);
	aq_dev->thermal_state = AQ_THERMAL_NORMAL;
	aq_dev->reset_pending = true;
	iflib_request_reset(aq_dev->ctx);
	iflib_admin_intr_deferred(aq_dev->ctx);
}

void
aq_if_update_admin_status(if_ctx_t ctx)
{
	struct aq_dev *aq_dev = iflib_get_softc(ctx);
	struct aq_hw *hw = &aq_dev->hw;
	uint32_t link_speed;
	bool running;


	struct aq_hw_fc_info fc_neg;
	aq_hw_get_link_state(hw, &link_speed, &fc_neg);

	/* A stopped or half-initialized interface has no link. */
	running = (if_getdrvflags(iflib_get_ifp(ctx)) & IFF_DRV_RUNNING) != 0;
	if (!running || aq_dev->init_failed)
		link_speed = 0;

	/* A retrain can change the speed without ever dropping the link. */
	if (link_speed != 0U &&
	    (!aq_dev->linkup || link_speed != aq_dev->link_speed)) {
		if (!aq_dev->linkup) {
			if (bootverbose)
				device_printf(aq_dev->dev,
				    "link UP: speed=%d\n", link_speed);
			aq_dev->phy_fault_last = 0;
		} else
			device_printf(aq_dev->dev, "link speed=%d\n",
			    link_speed);

		aq_dev->linkup = 1;
		aq_dev->link_speed = link_speed;

		/* turn on/off RX Pause in RPB */
		rpb_rx_xoff_en_per_tc_set(hw, fc_neg.fc_rx, 0);


		iflib_link_state_change(ctx, LINK_STATE_UP, IF_Mbps(link_speed));
		aq_mediastatus_update(aq_dev, link_speed, &fc_neg);

		/* update ITR settings according new link speed */
		aq_hw_interrupt_moderation_set(hw);
	} else if (link_speed == 0U && aq_dev->linkup) { /* link was UP */
		if (bootverbose)
			device_printf(aq_dev->dev, "link DOWN\n");

		aq_dev->linkup = 0;
		aq_dev->link_speed = 0;

		/* turn off RX Pause in RPB */
		rpb_rx_xoff_en_per_tc_set(hw, 0, 0);

		iflib_link_state_change(ctx, LINK_STATE_DOWN,  0);
		aq_mediastatus_update(aq_dev, link_speed, &fc_neg);
	}

	/* A stopped interface must not be re-initialized behind the operator. */
	if (!running)
		return;

	/* Re-arming while a reset is queued would re-init once too often. */
	if (aq_dev->init_failed) {
		if (aq_dev->reset_pending)
			return;
		if (aq_dev->init_retries < AQ_INIT_MAX_RETRIES) {
			aq_dev->init_retries++;
			aq_dev->reset_pending = true;
			iflib_request_reset(ctx);
		} else if (aq_dev->init_retries == AQ_INIT_MAX_RETRIES) {
			aq_dev->init_retries++;
			device_printf(aq_dev->dev, "initialization failed; "
			    "giving up after %d retries, link held down\n",
			    AQ_INIT_MAX_RETRIES);
		}
		return;
	}

	if (hw->fw_ops->get_phy_fault != NULL)
		aq_thermal_poll(aq_dev);

	aq_update_hw_stats(aq_dev);
}

/**************************************************************************/
/* interrupt service routine  (Top half)                                  */
/**************************************************************************/
int
aq_isr_rx(void *arg)
{
	struct aq_ring  *ring = arg;
	struct aq_dev   *aq_dev = ring->dev;
	struct aq_hw    *hw = &aq_dev->hw;

	itr_irq_status_clearlsw_set(hw, BIT(ring->msix));
	AQ_HW_FLUSH(hw);
	counter_u64_add(ring->stats.irq, 1);
	return (FILTER_SCHEDULE_THREAD);
}

/**************************************************************************/
/* interrupt service routine  (Top half)                                  */
/**************************************************************************/
int
aq_linkstat_isr(void *arg)
{
	struct aq_dev              *aq_dev = arg;
	struct aq_hw          *hw = &aq_dev->hw;

	itr_irq_status_clearlsw_set(hw, BIT(aq_dev->msix));
	AQ_HW_FLUSH(hw);

	iflib_admin_intr_deferred(aq_dev->ctx);

	return (FILTER_HANDLED);
}
