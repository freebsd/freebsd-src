/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Kyle Crenshaw
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * FUSB302 USB Type-C controller with USB-PD source policy and DP Alt Mode
 * VDM state machine.
 *
 * Implements the DFP (source) path: CC detection → source caps negotiation →
 * Discover_Identity → Discover_SVIDs → Discover_Modes → Enter_Mode →
 * DP_Status → DP_Config.  On success the result is exported via
 * fusb302_get_dp_altmode_state() for rk_cdn_dp.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/callout.h>
#include <sys/kernel.h>
#include <sys/intr.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>
#include <machine/atomic.h>

#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/openfirm.h>

#include <dev/regulator/regulator.h>

#include <arm64/rockchip/rk3399_typec_altmode_var.h>

#include <dev/iicbus/usb/usbc/usbc_tcpc.h>
#include <dev/iicbus/usb/usbc/usbc_pd_policy.h>

#include "iicbus_if.h"
#include "fusb302_var.h"

/* -----------------------------------------------------------------------
 * Register map
 * ----------------------------------------------------------------------- */
#define	FUSB_REG_DEVICE_ID	0x01
#define	FUSB_REG_SWITCHES0	0x02
#define	FUSB_REG_SWITCHES1	0x03
#define	FUSB_REG_MEASURE	0x04
#define	FUSB_REG_SLICE		0x05	/* BMC slicer threshold; bits 5:0 SDAC,
					 * LSB ~42mV.  Default 0x60 = SDAC=0x20
					 * (~840mV) + 1-bit hysteresis. */
#define	FUSB_REG_CONTROL0	0x06
#define	FUSB_REG_CONTROL1	0x07
#define	FUSB_REG_CONTROL2	0x08
#define	FUSB_REG_CONTROL3	0x09
#define	FUSB_REG_MASK1		0x0A
#define	FUSB_REG_POWER		0x0B
#define	FUSB_REG_RESET		0x0C
#define	FUSB_REG_MASKA		0x0E
#define	FUSB_REG_MASKB		0x0F
#define	FUSB_REG_CONTROL4	0x10
#define	FUSB_REG_STATUS0A	0x3C
#define	FUSB_REG_STATUS1A	0x3D
#define	FUSB_REG_INTERRUPTA	0x3E
#define	FUSB_REG_INTERRUPTB	0x3F
#define	FUSB_REG_STATUS0	0x40
#define	FUSB_REG_STATUS1	0x41
#define	FUSB_REG_INTERRUPT	0x42
#define	FUSB_REG_FIFO		0x43

/* SWITCHES0 bits */
#define	FUSB_SW0_PDWN1		0x01
#define	FUSB_SW0_PDWN2		0x02
#define	FUSB_SW0_MEAS_CC1	0x04
#define	FUSB_SW0_MEAS_CC2	0x08
#define	FUSB_SW0_VCONN_CC1	0x10
#define	FUSB_SW0_VCONN_CC2	0x20
#define	FUSB_SW0_PU_EN1		0x40
#define	FUSB_SW0_PU_EN2		0x80

/* SWITCHES1 bits */
#define	FUSB_SW1_TXCC1		0x01
#define	FUSB_SW1_TXCC2		0x02
#define	FUSB_SW1_AUTO_CRC	0x04
#define	FUSB_SW1_DATAROLE	0x10
#define	FUSB_SW1_SPECREV	0x60	/* bits 6:5 */
#define	FUSB_SW1_POWERROLE	0x80

/* CONTROL0 bits */
/*
 * CONTROL0 bit layout per FUSB302B datasheet Table 22:
 *   bit 7    reserved
 *   bit 6    TX_FLUSH (W/C)
 *   bit 5    INT_MASK
 *   bit 4    reserved
 *   bits 3:2 HOST_CUR[1:0] (00=off, 01=USB-default 80uA, 10=1.5A, 11=3A)
 *   bit 1    AUTO_PRE
 *   bit 0    TX_START
 *
 * Earlier this file had FUSB_CTL0_HOST_CUR_DEF = 0x02 with comment
 * "bits[2:1]=01" -- that was wrong (bit 1 is AUTO_PRE!) and meant
 * the chip ran with HOST_CUR=00 (no Rp current at all) and
 * AUTO_PRE=1 (chip auto-starts TX 170ms after every CRC_CHK).
 * Result: when we presented "Rp" the partner saw nothing electrically
 * and never reciprocated.  Linux's CONTROL0_HOST_CUR_USB = (1 << 2)
 * = 0x04 sets USB-default 80uA Rp, which is what we want at attach.
 */
#define	FUSB_CTL0_HOST_CUR_USB	0x04	/* bits[3:2]=01 = USB-default 80uA Rp */
#define	FUSB_CTL0_HOST_CUR_1A5	0x08	/* bits[3:2]=10 = 1.5A 180uA Rp    */
#define	FUSB_CTL0_HOST_CUR_3A	0x0c	/* bits[3:2]=11 = 3.0A 330uA Rp    */
/*
 * Reference vendor BSP `tcpm_init` calls `tcpm_select_rp_value(TYPEC_RP_USB)` →
 * sets HOST_CUR=01 (USB-default 80uA Rp) at chip init.  An earlier
 * comment claimed a reference regmap_reg_write trace showed 1A5 (180uA),
 * but that trace cannot be reproduced on current reference builds (cdn-dp
 * probe fails — see project_cdn_dp_mainline_6_18_broken.md), and the vendor
 * BSP source-of-truth is unambiguous: USB-default at init, escalated only
 * when transitioning to ATTACHED_SRC if needed.  Path-C empirical test
 * 2026-05-09: flipping back to _USB to see if silent-PD partner ACKs.
 */
#define	FUSB_CTL0_HOST_CUR_DEF	FUSB_CTL0_HOST_CUR_USB
#define	FUSB_CTL0_INT_MASK	0x20

/* CONTROL1 bits */
#define	FUSB_CTL1_RX_FLUSH	0x04

/* CONTROL2 bits */
#define	FUSB_CTL2_TOG_RD_ONLY	0x20
#define	FUSB_CTL2_MODE_MASK	0x06
#define	FUSB_CTL2_MODE_DRP	0x02
#define	FUSB_CTL2_MODE_UFP	0x04
#define	FUSB_CTL2_MODE_DFP	0x06
#define	FUSB_CTL2_TOGGLE	0x01

/* TOGSS field values (STATUS1A bits 5:3) */
#define	FUSB_TOGSS_NOTHING	0
#define	FUSB_TOGSS_SRC_CC1	1
#define	FUSB_TOGSS_SRC_CC2	2
#define	FUSB_TOGSS_SNK_CC1	5
#define	FUSB_TOGSS_SNK_CC2	6
#define	FUSB_TOGSS_AUDIO	7

/* Toggle role preference (hw.fusb302.role_pref tunable) */
#define	FUSB_ROLE_DRP		0
#define	FUSB_ROLE_SRC		1
#define	FUSB_ROLE_SNK		2

/* CONTROL3 bits */
#define	FUSB_CTL3_AUTO_RETRY	0x01
#define	FUSB_CTL3_N_RETRIES	0x06	/* bits 2:1 — value 3 = 3 retries */
#define	FUSB_CTL3_SEND_HARDRST	0x40

/* CONTROL4 bits */
#define	FUSB_CTL4_TOG_USRC_EXIT	0x01

/* POWER bits */
#define	FUSB_POWER_ALL		0x0f

/* RESET bits */
#define	FUSB_RESET_PD_RESET	0x02
#define	FUSB_RESET_SW_RES	0x01

/* STATUS0 bits */
#define	FUSB_ST0_VBUSOK		0x80
#define	FUSB_ST0_COMP		0x20
#define	FUSB_ST0_BC_LVL_MASK	0x03

/* STATUS1 bits */
#define	FUSB_ST1_RX_EMPTY	0x20

/* MEASURE thresholds for TYPEC_RP_USB attach probing. */
#define	FUSB_MEASURE_MDAC_USB_HIGH	0x26
#define	FUSB_MEASURE_MDAC_USB_LOW	0x05

/* STATUS1A togss field */
#define	FUSB_ST1A_TOGSS_MASK	0x38
#define	FUSB_ST1A_TOGSS_SHIFT	3

/* INTERRUPTA bits */
#define	FUSB_INTRA_HARDRST	0x01
#define	FUSB_INTRA_TXSENT	0x04
#define	FUSB_INTRA_HARDSENT	0x08
#define	FUSB_INTRA_RETRYFAIL	0x10
#define	FUSB_INTRA_TOGDONE	0x40

/* INTERRUPTB bits */
#define	FUSB_INTRB_GCRCSENT	0x01

/* Interrupt mask register initial values for PD operation */
#define	FUSB_MASK1_ATTACHED	0x55	/* active session: also unmask BC_LVL change
					   (reference build's working value while DP altmode active) */
#define	FUSB_MASK1_PD		0x75	/* unmask COLLISION, ALERT, VBUSOK
					   (matches Linux i2c-rk3x init) */
#define	FUSB_MASKA_PD		0xa2	/* unmask TOGDONE,TXSENT,HARDSENT,RETRYFAIL,HARDRST */
#define	FUSB_MASKB_PD		0xfe	/* unmask GCRCSENT */

/* TX FIFO tokens */
#define	FUSB_TKN_TXON		0xa1
#define	FUSB_TKN_SYNC1		0x12
#define	FUSB_TKN_SYNC2		0x13
#define	FUSB_TKN_PACKSYM	0x80
#define	FUSB_TKN_JAMCRC		0xff
#define	FUSB_TKN_EOP		0x14
#define	FUSB_TKN_TXOFF		0xfe

/* PD control message types */
#define	PD_CMT_GOODCRC		1
#define	PD_CMT_ACCEPT		3
#define	PD_CMT_REJECT		4
#define	PD_CMT_PS_RDY		6
#define	PD_CMT_GETSINKCAP	8
#define	PD_CMT_WAIT		12
#define	PD_CMT_SOFTRESET	13

/* PD data message types */
#define	PD_DMT_SRCCAP		1
#define	PD_DMT_REQUEST		2
#define	PD_DMT_SINKCAP		4
#define	PD_DMT_VDM		15

/* VDM command IDs */
#define	VDM_DISC_ID		0x01
#define	VDM_DISC_SVIDS		0x02
#define	VDM_DISC_MODES		0x03
#define	VDM_ENTER_MODE		0x04
#define	VDM_DP_STATUS		0x10
#define	VDM_DP_CONFIG		0x11

/* VDM command types */
#define	VDM_TYPE_INIT		0
#define	VDM_TYPE_ACK		1
#define	VDM_TYPE_NACK		2

/* DP pin mode bits */
#define	DP_PIN_A		0x01
#define	DP_PIN_B		0x02
#define	DP_PIN_C		0x04
#define	DP_PIN_D		0x08
#define	DP_PIN_E		0x10
#define	DP_PIN_F		0x20
#define	DP_PIN_MF_MASK		(DP_PIN_B | DP_PIN_D | DP_PIN_F)
#define	DP_PIN_BR2_MASK		(DP_PIN_A | DP_PIN_B)
#define	DP_PIN_DP_MASK		(DP_PIN_C | DP_PIN_D | DP_PIN_E | DP_PIN_F)

/* PD header field extraction */
#define	PD_HDR_CNT(h)		(((h) >> 12) & 7)
#define	PD_HDR_TYPE(h)		((h) & 0xf)
#define	PD_HDR_ID(h)		(((h) >> 9) & 7)
#define	PD_IS_CTRL(h, t)	(PD_HDR_CNT(h) == 0 && PD_HDR_TYPE(h) == (t))
#define	PD_IS_DATA(h, t)	(PD_HDR_CNT(h) != 0 && PD_HDR_TYPE(h) == (t))

/* VDM header field extraction */
#define	VDM_GET_STRUCT(h)	(((h) >> 15) & 1)
#define	VDM_GET_CMD_TYPE(h)	(((h) >> 6) & 3)
#define	VDM_GET_CMD(h)		((h) & 0x1f)

/* DP status fields */
#define	DP_STATUS_HPD(s)	(((s) >> 7) & 1)
#define	DP_STATUS_MF_PREF(s)	(((s) >> 4) & 1)

/* DP capability pin field (receptacle vs plug) */
#define	PD_DP_PIN_CAPS(x)	((((x) >> 6) & 1) ? (((x) >> 16) & 0x3f) \
				 : (((x) >> 8) & 0x3f))
#define	PD_DP_SIGNAL_GEN2(x)	(((x) >> 3) & 1)

/* TX state values */
#define	FUSB_TX_IDLE	0
#define	FUSB_TX_BUSY	1
#define	FUSB_TX_FAILED	2
#define	FUSB_TX_SUCCESS	3

/* Event bits (stored in pending_events). */
#define	FUSB_EVT_CC		0x01
#define	FUSB_EVT_RX		0x02
#define	FUSB_EVT_TX		0x04
#define	FUSB_EVT_REC_RESET	0x08
#define	FUSB_EVT_CONTINUE	0x20
#define	FUSB_EVT_TIMER_STATE	0x80

/* Sender response timeout (ms) */
#define	T_SENDER_RESPONSE	30
#define	T_SRC_TRANSITION	30
#define	T_TYPEC_SINK_WAIT_CAP	500
#define	T_PS_TRANSITION		550
#define	T_NO_RESPONSE		5500

/* Max hard-reset retries before giving up on PD and falling back to
 * Type-C default 5V (passive) operation. */
#define	N_SNK_HARDRESET_RETRY	2
#define	T_TYPEC_SEND_SRCCAP	250

/* Source capability PDO: 5V fixed, 900mA, USB comm capable */
#define	FUSB_SRC_PDO_5V900MA	((1u << 26) | (100u << 10) | 90u)

/* -----------------------------------------------------------------------
 * Connection-state enum (DFP/source + UFP/sink paths)
 * ----------------------------------------------------------------------- */
enum fusb302_conn_state {
	FUSB_ST_DISABLED = 0,
	FUSB_ST_ERROR_RECOVERY,
	FUSB_ST_UNATTACHED,
	FUSB_ST_ATTACHED_SRC,
	FUSB_ST_SRC_STARTUP,
	FUSB_ST_SRC_SEND_CAPS,
	FUSB_ST_SRC_DISCOVERY,
	FUSB_ST_SRC_NEGOTIATE_CAP,
	FUSB_ST_SRC_CAP_RESPONSE,
	FUSB_ST_SRC_TRANSITION_SUPPLY,
	FUSB_ST_SRC_TRANSITION_DEFAULT,
	FUSB_ST_SRC_READY,
	FUSB_ST_SRC_GET_SINK_CAPS,
	FUSB_ST_SRC_SEND_HARDRST,
	FUSB_ST_SRC_SEND_SOFTRST,
	FUSB_ST_SRC_SOFTRST,
	FUSB_ST_ATTACHED_SNK,
	FUSB_ST_SNK_STARTUP,
	FUSB_ST_SNK_DISCOVERY,
	FUSB_ST_SNK_EVALUATE_CAPS,
	FUSB_ST_SNK_SELECT_CAP,
	FUSB_ST_SNK_TRANSITION_SINK,
	FUSB_ST_SNK_READY,
	FUSB_ST_SNK_TRANSITION_DEFAULT,
	FUSB_ST_SNK_SEND_HARDRST,
	FUSB_ST_SNK_SEND_SOFTRST,
	FUSB_ST_ROLE_DISCOVERY_SRC,
};

/* VDM state (signed: -1 = error) */
enum fusb302_vdm_state {
	VDM_DISC_ID_ST = 0,
	VDM_DISC_SVID_ST,
	VDM_DISC_MODES_ST,
	VDM_ENTER_MODE_ST,
	VDM_DP_STATUS_ST,
	VDM_DP_CONFIG_ST,
	VDM_NOTIFY_ST,
	VDM_READY_ST,
	VDM_ERR_ST = -1,
};

/* -----------------------------------------------------------------------
 * Software context
 * ----------------------------------------------------------------------- */

/*
 * DPRINTF — chatty trace output gated on dev.fusb302.0.debug.
 *   debug = 0  (default): only true errors / one-shot attach milestones
 *                         go to dmesg
 *   debug >= 1:           verbose trace (alerts, FIFO TX/RX, VDM walk,
 *                         PD message dumps)
 */
#define	FUSB302_DPRINTF(sc, ...)					\
	do {								\
		if ((sc)->debug > 0)					\
			device_printf((sc)->dev, __VA_ARGS__);		\
	} while (0)

struct fusb302_softc {
	device_t		dev;
	int			debug;		/* sysctl-controlled, gates DPRINTF */
	/*
	 * Sleepable rwlock so the irq path can hold it across i2c
	 * transactions (which sleep in rk_i2c_transfer).  The previous
	 * MTX_DEF mutex hit propagate_priority panics whenever a second
	 * thread contended on it while the holder was sleeping in i2c.
	 */
	struct sx		sx;
	uint8_t			addr;
	int			irq_rid;
	struct resource		*irq_res;
	void			*irq_cookie;
	struct task		irq_task;
	regulator_t		vbus_supply;
	bool			vbus_enabled;
	int			passive_src;	/* present Rp on CC, skip ALL regulator_enable paths */
	bool			initialized;

	/* CC detection legacy state */
	bool			state_valid;
	uint8_t			device_id;
	uint8_t			power;
	uint8_t			control2;
	uint8_t			status0;
	uint8_t			status1;
	uint8_t			status0a;
	uint8_t			status1a;
	enum fusb302_typec_orientation	orientation;
	enum fusb302_typec_role		role;
	bool			attached;

	/* PD / VDM state machine */
	enum fusb302_conn_state	conn_state;
	int			sub_state;
	uint32_t		work_continue;
	volatile uint32_t	pending_events;

	/* PD messaging */
	uint16_t		send_head;
	uint16_t		rec_head;
	uint32_t		send_load[7];
	uint32_t		rec_load[7];
	int			msg_id;
	int			tx_state;

	/* CC polarity: 0 = CC1, 1 = CC2 */
	int			cc_polarity;
	bool			vconn_enabled;
	bool			is_pd_support;

	/* Toggle role preference (FUSB_ROLE_*) and current attach role */
	int			role_pref;
	bool			attached_as_sink;

	/*
	 * Role-discovery arbitration: when DRP-toggle settles us as SNK
	 * with a partner that doesn't fulfill SRC role (no
	 * Source_Capabilities even after hard-resets), drop terminations
	 * and re-toggle to roll the dice again.
	 *   role_discovery_complete: latches when we give up so we
	 *                            don't infinite-loop.
	 *   role_discovery_tries:    how many TOGSS rerolls we've used.
	 */
	bool			role_discovery_complete;
	int			role_discovery_tries;

	/* hw.fusb302.skip_pd: skip PD negotiation when attached as SNK and
	 * jump straight to passive 4-lane DP defaults. For passive USB-C→DP
	 * displays/dongles that don't implement a PD client. */
	int			skip_pd;

	/*
	 * PD specification revision to advertise in the message header.
	 * 1 = PD 2.0, 2 = PD 3.0+ (the chip's SPECREV field is 2 bits).
	 * Some older sinks reject PD 3.0 headers and only respond to PD
	 * 2.0; flipping this lets us A/B-test against an unresponsive
	 * partner without rebuilding the kernel.  Default 2 (PD 3.0)
	 * matches Linux 4.4 BSP and current spec usage.
	 */
	int			pd_spec_rev;

	/*
	 * BMC slicer DAC value (SLICE register bits 5:0).  Sets the
	 * threshold the chip's BMC receiver uses to slice incoming CC
	 * voltage edges into ones and zeros.  Default 0x20 (~840 mV)
	 * matches the chip reset value.  Lower values = more sensitive
	 * to weaker incoming pulses; useful when ACTIVITY fires on the
	 * wire but no CRC_CHK ever validates (cable signal-integrity
	 * margin issue).  Tunable via hw.fusb302.slice_sdac and the
	 * dev.fusb302.0.slice_sdac sysctl.
	 */
	int			slice_sdac;

	/* VDM */
	enum fusb302_vdm_state	vdm_state;
	int			vdm_send_state;
	uint16_t		vdm_svid[12];
	int			vdm_svid_num;
	uint32_t		vdm_id;
	uint8_t			val_tmp;

	/* Notify / role */
	bool			notify_is_cc;
	bool			notify_is_pd;
	bool			notify_is_enter_mode;
	int			notify_power_role;	/* 1 = source */
	int			notify_data_role;	/* 1 = DFP */
	uint32_t		notify_dp_caps;
	uint32_t		notify_dp_status;
	int			notify_pin_support;
	int			notify_pin_def;

	/* Partner sink caps */
	uint32_t		partner_cap[7];

	/* Retry/retry counters */
	int			caps_counter;
	int			hardrst_count;
	bool			softrst_tried;	/* SNK: soft reset attempted */
	int			pos_power;

	/* Timers */
	struct callout		timer_state;

	/* DP Alt Mode result exported to rk_cdn_dp */
	struct rk3399_typec_dp_altmode_status	dp_altmode;

	/*
	 * TCPC abstraction + USB-PD policy state machine.  Lives alongside
	 * the embedded SM during the Phase 3 transition: the irq_task drives
	 * the embedded SM under sc->mtx and accumulates an event mask that
	 * gets delivered to sc->policy after sc->mtx is dropped (the policy
	 * takes its own lock and would deadlock if entered with sc->mtx held,
	 * since policy state handlers call back into TCPC ops that re-acquire
	 * sc->mtx).
	 */
	struct usbc_tcpc		tcpc;
	struct usbc_pd_policy		*policy;
};

static struct ofw_compat_data compat_data[] = {
	{ "fcs,fusb302",	1 },
	{ NULL,			0 }
};
IICBUS_FDT_PNP_INFO(compat_data);

/* -----------------------------------------------------------------------
 * Forward declarations
 * ----------------------------------------------------------------------- */
static int	fusb302_probe(device_t dev);
static int	fusb302_attach(device_t dev);
static int	fusb302_detach(device_t dev);
static void	fusb302_irq_task(void *context, int pending);
static void	fusb302_irq_ithread(void *context);
static int	fusb302_intr(void *context);
static void	fusb302_timer_state_cb(void *arg);
static int	fusb302_sysctl_slice_sdac(SYSCTL_HANDLER_ARGS);
static int	fusb302_sysctl_reattach(SYSCTL_HANDLER_ARGS);
/* fusb302_get_typec_status / fusb302_get_dp_altmode_state /
 * fusb302_tcpc_ops are declared in fusb302_var.h. */
static bool	fusb302_is_rockpro64(device_t dev);
static int	fusb302_try_ofw_irq(struct fusb302_softc *sc);
static void	fusb302_add_sysctls(struct fusb302_softc *sc);
static void	fusb302_notify_dp_locked(struct fusb302_softc *sc);
static int	fusb302_probe_cc_pull_up_locked(struct fusb302_softc *sc,
		    int polarity);

/* -----------------------------------------------------------------------
 * Low-level I2C helpers
 * ----------------------------------------------------------------------- */
static int
fusb302_read_reg(struct fusb302_softc *sc, uint8_t reg, uint8_t *val)
{
	return (iic2errno(iicdev_readfrom(sc->dev, reg, val, 1, IIC_WAIT)));
}

static int
fusb302_write_reg(struct fusb302_softc *sc, uint8_t reg, uint8_t val)
{
	return (iic2errno(iicdev_writeto(sc->dev, reg, &val, 1, IIC_WAIT)));
}

/* Read-modify-write a register: new = (old & ~mask) | (val & mask) */
static int
fusb302_update_reg(struct fusb302_softc *sc, uint8_t reg, uint8_t mask,
    uint8_t val)
{
	uint8_t cur;
	int error;

	error = fusb302_read_reg(sc, reg, &cur);
	if (error != 0)
		return (error);
	cur = (cur & ~mask) | (val & mask);
	return (fusb302_write_reg(sc, reg, cur));
}

static bool
fusb302_is_rockpro64(device_t dev)
{
	phandle_t root;

	root = OF_finddevice("/");
	if (root <= 0)
		return (false);

	return (ofw_bus_node_is_compatible(root, "pine64,rockpro64") ||
	    ofw_bus_node_is_compatible(root, "pine64,rockpro64-v2.0") ||
	    ofw_bus_node_is_compatible(root, "pine64,rockpro64-v2.1"));
}

/*
 * tcpm-style get_cc_pull_up: with toggle stopped, source-measure a
 * specific CC pin and classify it as open/Rd/Ra. We only need to detect Rd
 * here to bootstrap a hot-plugged DFP/source attach after kldload.
 */
static int
fusb302_probe_cc_pull_up_locked(struct fusb302_softc *sc, int polarity)
{
	uint8_t store, status0;
	uint8_t sw0, measure;
	int retry, comp_high;
	int ret;

	ret = 0;
	if (fusb302_read_reg(sc, FUSB_REG_SWITCHES0, &store) != 0)
		return (0);

	/*
	 * Clear PDWN bits too — if the chip was in sink role when probe
	 * runs, leaving PDWN set while applying PU forces CC to GND
	 * (PDWN dominates) and produces a false "no Rd" result.
	 */
	sw0 = store & ~(FUSB_SW0_MEAS_CC1 | FUSB_SW0_MEAS_CC2 |
	    FUSB_SW0_PU_EN1 | FUSB_SW0_PU_EN2 |
	    FUSB_SW0_PDWN1 | FUSB_SW0_PDWN2);
	if (polarity == 0)
		sw0 |= FUSB_SW0_MEAS_CC1 | FUSB_SW0_PU_EN1;
	else
		sw0 |= FUSB_SW0_MEAS_CC2 | FUSB_SW0_PU_EN2;
	if (fusb302_write_reg(sc, FUSB_REG_SWITCHES0, sw0) != 0)
		return (0);

	measure = FUSB_MEASURE_MDAC_USB_HIGH;
	(void)fusb302_write_reg(sc, FUSB_REG_MEASURE, measure);
	DELAY(300);

	comp_high = 0;
	for (retry = 0; retry < 3; retry++) {
		(void)fusb302_write_reg(sc, FUSB_REG_MEASURE, measure);
		DELAY(300);
		if (fusb302_read_reg(sc, FUSB_REG_STATUS0, &status0) != 0)
			break;
		if ((status0 & FUSB_ST0_COMP) != 0)
			comp_high++;
	}

	if (comp_high != 3) {
		measure = FUSB_MEASURE_MDAC_USB_LOW;
		(void)fusb302_write_reg(sc, FUSB_REG_MEASURE, measure);
		DELAY(300);
		if (fusb302_read_reg(sc, FUSB_REG_STATUS0, &status0) == 0 &&
		    (status0 & FUSB_ST0_COMP) != 0)
			ret = 1;
	}

	(void)fusb302_write_reg(sc, FUSB_REG_SWITCHES0, store);
	(void)fusb302_write_reg(sc, FUSB_REG_MEASURE,
	    FUSB_MEASURE_MDAC_USB_HIGH);
	return (ret);
}

/*
 * Read one PD message from the RX FIFO into sc->rec_head / sc->rec_load.
 * Automatically skips GoodCRC packets (hardware auto-reply artefacts).
 * Caller must hold sc->mtx.
 */
static int
fusb302_fifo_read_locked(struct fusb302_softc *sc)
{
	uint8_t buf[32];
	int len, tries, error;

	tries = 8;
	do {
		error = iicdev_readfrom(sc->dev, FUSB_REG_FIFO, buf, 3,
		    IIC_WAIT);
		if (error != 0)
			return (iic2errno(error));

		sc->rec_head = (uint16_t)buf[1] | ((uint16_t)buf[2] << 8);
		len = PD_HDR_CNT(sc->rec_head) << 2;

		error = iicdev_readfrom(sc->dev, FUSB_REG_FIFO, buf,
		    len + 4, IIC_WAIT);
		if (error != 0)
			return (iic2errno(error));

		if (!PD_IS_CTRL(sc->rec_head, PD_CMT_GOODCRC)) {
			memcpy(sc->rec_load, buf, len);
			return (0);
		}
	} while (--tries > 0);

	return (EIO);
}

/*
 * Write a PD message from sc->send_head / sc->send_load to the TX FIFO.
 * Caller must hold sc->mtx.
 */
static int
fusb302_fifo_write_locked(struct fusb302_softc *sc)
{
	uint8_t senddata[40];
	int pos, error;
	uint8_t len;

	pos = 0;
	senddata[pos++] = FUSB_TKN_SYNC1;
	senddata[pos++] = FUSB_TKN_SYNC1;
	senddata[pos++] = FUSB_TKN_SYNC1;
	senddata[pos++] = FUSB_TKN_SYNC2;

	len = (uint8_t)(PD_HDR_CNT(sc->send_head) << 2);
	senddata[pos++] = FUSB_TKN_PACKSYM | ((len + 2) & 0x1f);
	senddata[pos++] = (uint8_t)(sc->send_head & 0xff);
	senddata[pos++] = (uint8_t)(sc->send_head >> 8);

	memcpy(&senddata[pos], sc->send_load, len);
	pos += len;

	senddata[pos++] = FUSB_TKN_JAMCRC;
	senddata[pos++] = FUSB_TKN_EOP;
	senddata[pos++] = FUSB_TKN_TXOFF;
	senddata[pos++] = FUSB_TKN_TXON;

	{
		char hex[3 * 40 + 1];
		int i, n;

		n = 0;
		for (i = 0; i < pos && i < 40; i++)
			n += snprintf(hex + n, sizeof(hex) - n,
			    "%s%02x", i ? " " : "", senddata[i]);
		if (bootverbose)
			FUSB302_DPRINTF(sc,
			    "fifo TX [%d B] hdr=0x%04x ndo=%d: %s\n",
			    pos, sc->send_head, PD_HDR_CNT(sc->send_head),
			    hex);
	}

	error = iicdev_writeto(sc->dev, FUSB_REG_FIFO, senddata, pos,
	    IIC_WAIT);
	return (iic2errno(error));
}

static void
fusb302_flush_rx_fifo_locked(struct fusb302_softc *sc)
{
	(void)fusb302_write_reg(sc, FUSB_REG_CONTROL1, FUSB_CTL1_RX_FLUSH);
}

/* -----------------------------------------------------------------------
 * Hardware configuration
 * ----------------------------------------------------------------------- */

/*
 * Configure SWITCHES0/1 for PD communication on the active CC pin.
 * cc_polarity: 0 = CC1, 1 = CC2.  Always called as DFP (source role).
 */
static void
fusb302_set_polarity_locked(struct fusb302_softc *sc, int polarity)
{
	uint8_t sw0, sw1;

	sc->cc_polarity = polarity;

	sw0 = 0;
	if (sc->vconn_enabled) {
		/* VCONN on the opposite CC pin */
		sw0 |= polarity ? FUSB_SW0_VCONN_CC1 : FUSB_SW0_VCONN_CC2;
	}
	if (sc->attached_as_sink) {
		/*
		 * UFP/sink: keep Rd on both CC (PDWN1|PDWN2), measure on
		 * the active CC pin only. No Rp.
		 */
		if (polarity == 0)
			sw0 |= FUSB_SW0_MEAS_CC1;
		else
			sw0 |= FUSB_SW0_MEAS_CC2;
		sw0 |= FUSB_SW0_PDWN1 | FUSB_SW0_PDWN2;
	} else {
		/* DFP/source: pull-up + measure on active CC, drop pull-downs */
		if (polarity == 0)
			sw0 |= FUSB_SW0_MEAS_CC1 | FUSB_SW0_PU_EN1;
		else
			sw0 |= FUSB_SW0_MEAS_CC2 | FUSB_SW0_PU_EN2;
	}

	(void)fusb302_update_reg(sc, FUSB_REG_SWITCHES0,
	    FUSB_SW0_PDWN1 | FUSB_SW0_PDWN2 |
	    FUSB_SW0_VCONN_CC1 | FUSB_SW0_VCONN_CC2 |
	    FUSB_SW0_MEAS_CC1 | FUSB_SW0_MEAS_CC2 |
	    FUSB_SW0_PU_EN1 | FUSB_SW0_PU_EN2, sw0);

	sw1 = polarity ? FUSB_SW1_TXCC2 : FUSB_SW1_TXCC1;
	(void)fusb302_update_reg(sc, FUSB_REG_SWITCHES1,
	    FUSB_SW1_TXCC1 | FUSB_SW1_TXCC2, sw1);
}

/* Enable or disable PD reception on the active CC pin. */
static void
fusb302_enable_rx_locked(struct fusb302_softc *sc, bool enable)
{
	uint8_t mask, val;

	mask = FUSB_SW0_MEAS_CC1 | FUSB_SW0_MEAS_CC2;
	val = 0;
	if (enable) {
		val = sc->cc_polarity ? FUSB_SW0_MEAS_CC2 : FUSB_SW0_MEAS_CC1;
		fusb302_flush_rx_fifo_locked(sc);
	}
	(void)fusb302_update_reg(sc, FUSB_REG_SWITCHES0, mask, val);
	(void)fusb302_update_reg(sc, FUSB_REG_SWITCHES1, FUSB_SW1_AUTO_CRC,
	    enable ? FUSB_SW1_AUTO_CRC : 0);
}

/* Write power/data role into SWITCHES1 so the firmware stamps them in TX. */
static void
fusb302_set_msg_header_locked(struct fusb302_softc *sc)
{
	uint8_t val;

	val = (uint8_t)((sc->notify_power_role << 7) |
	    (sc->notify_data_role << 4));
	(void)fusb302_update_reg(sc, FUSB_REG_SWITCHES1,
	    FUSB_SW1_POWERROLE | FUSB_SW1_DATAROLE, val);
	/*
	 * PD specification revision -- caller-controlled via sc->pd_spec_rev
	 * (1 = PD 2.0, 2 = PD 3.0).  Default at attach is 2 (PD 3.0); a
	 * sysctl can flip this when an unresponsive sink only acks PD 2.0.
	 */
	(void)fusb302_update_reg(sc, FUSB_REG_SWITCHES1,
	    FUSB_SW1_SPECREV,
	    (uint8_t)((sc->pd_spec_rev & 0x3) << 5));
}

/* Issue a PD layer reset (does not affect CC toggle). */
static void
fusb302_pd_reset_locked(struct fusb302_softc *sc)
{
	(void)fusb302_write_reg(sc, FUSB_REG_RESET, FUSB_RESET_PD_RESET);
}

/* -----------------------------------------------------------------------
 * State machine helpers
 * ----------------------------------------------------------------------- */
static void
fusb302_set_state_locked(struct fusb302_softc *sc,
    enum fusb302_conn_state state)
{
	sc->conn_state = state;
	sc->sub_state = 0;
	sc->val_tmp = 0;
	sc->work_continue |= FUSB_EVT_CONTINUE;
}

static void
fusb302_reset_pd_params_locked(struct fusb302_softc *sc)
{
	sc->caps_counter = 0;
	sc->msg_id = 0;
	sc->vdm_state = VDM_DISC_ID_ST;
	sc->vdm_send_state = 0;
	sc->vdm_svid_num = 0;
	sc->vdm_id = 0;
	sc->val_tmp = 0;
	sc->pos_power = 0;
	memset(sc->vdm_svid, 0, sizeof(sc->vdm_svid));
}

/* Start or restart a callout timer; ms = 0 stops it. */
static void
fusb302_start_state_timer(struct fusb302_softc *sc, int ms)
{
	callout_reset_sbt(&sc->timer_state, SBT_1MS * ms, 0,
	    fusb302_timer_state_cb, sc, 0);
}


/* -----------------------------------------------------------------------
 * PD message building
 * ----------------------------------------------------------------------- */

/*
 * Build a control or data message in sc->send_head / sc->send_load.
 * data_cnt: number of 32-bit data objects.
 */
static void
fusb302_build_header_locked(struct fusb302_softc *sc, int type, int data_cnt)
{
	sc->send_head = (uint16_t)(
	    ((sc->msg_id & 0x7) << 9) |
	    ((sc->notify_power_role & 0x1) << 8) |
	    (0 << 6) |				/* PD spec rev 0 = PD 1.0 */
	    ((sc->notify_data_role & 0x1) << 5) |
	    ((data_cnt & 0x7) << 12) |
	    (type & 0xf));
}

/* Build a source-capabilities data message (single 5V PDO). */
static void
fusb302_set_mesg_srccap_locked(struct fusb302_softc *sc)
{
	fusb302_build_header_locked(sc, PD_DMT_SRCCAP, 1);
	sc->send_load[0] = FUSB_SRC_PDO_5V900MA;
}

/*
 * Build a Sink Request Data Object for the partner's selected fixed PDO.
 * sc->pos_power holds the 1-based PDO index chosen by snk_evaluate_caps.
 */
static uint32_t
fusb302_build_rdo_locked(struct fusb302_softc *sc)
{
	uint32_t pdo, op_cur, max_cur, rdo;
	int pos;

	pos = sc->pos_power;
	if (pos < 1 || pos > 7)
		pos = 1;
	pdo = sc->partner_cap[pos - 1];
	max_cur = pdo & 0x3ff;		/* 10mA units */
	if (max_cur == 0)
		max_cur = 90;		/* 900mA fallback */
	op_cur = max_cur;

	rdo = ((uint32_t)pos << 28) |
	    (1u << 25) |		/* USB Communications Capable */
	    (1u << 24) |		/* No USB Suspend */
	    ((op_cur & 0x3ff) << 10) |
	    (max_cur & 0x3ff);
	return (rdo);
}

/* Build a sink Request data message for the selected PDO. */
static void
fusb302_set_mesg_request_locked(struct fusb302_softc *sc)
{
	fusb302_build_header_locked(sc, PD_DMT_REQUEST, 1);
	sc->send_load[0] = fusb302_build_rdo_locked(sc);
}

/* Build a control message (no data objects). */
static void
fusb302_set_mesg_ctrl_locked(struct fusb302_softc *sc, int cmd)
{
	fusb302_build_header_locked(sc, cmd, 0);
}

/* Build a VDM data message. */
static void
fusb302_set_vdm_mesg_locked(struct fusb302_softc *sc, int cmd, int type,
    int mode)
{
	fusb302_build_header_locked(sc, PD_DMT_VDM, 0);
	/* header: data count filled per-command below */

	sc->send_load[0] = (1u << 15) |	/* structured VDM */
	    (0u << 13) |			/* SVID reserved bits */
	    ((type & 3) << 6) |
	    (cmd & 0x1f);

	switch (cmd) {
	case VDM_DISC_ID:
	case VDM_DISC_SVIDS:
		sc->send_load[0] |= (0xff00u << 16);
		fusb302_build_header_locked(sc, PD_DMT_VDM, 1);
		break;
	case VDM_DISC_MODES:
		sc->send_load[0] |=
		    ((uint32_t)sc->vdm_svid[sc->val_tmp >> 1] << 16);
		fusb302_build_header_locked(sc, PD_DMT_VDM, 1);
		break;
	case VDM_ENTER_MODE:
		fusb302_build_header_locked(sc, PD_DMT_VDM, 1);
		sc->send_load[0] |= ((mode & 7) << 8) | (0xff01u << 16);
		break;
	case VDM_DP_STATUS:
		fusb302_build_header_locked(sc, PD_DMT_VDM, 2);
		sc->send_load[0] |= (1u << 8) | (0xff01u << 16);
		sc->send_load[1] = 5;		/* DFP connected, HPD high */
		break;
	case VDM_DP_CONFIG:
		fusb302_build_header_locked(sc, PD_DMT_VDM, 2);
		sc->send_load[0] |= (1u << 8) | (0xff01u << 16);
		/* pin_assignment_def set by process_vdm_msg when modes arrived */
		sc->send_load[1] =
		    ((uint32_t)sc->notify_pin_def << 8) | (1 << 2) | 2;
		FUSB302_DPRINTF(sc, "VDM DP config: send_load[1]=0x%08x "
		    "pin_def=0x%x\n", sc->send_load[1], sc->notify_pin_def);
		break;
	default:
		break;
	}
}

/* Select pin assignment from DP caps + status. */
static int
fusb302_dp_pin_assignment(uint32_t caps, uint32_t status)
{
	uint32_t pin_caps;

	pin_caps = PD_DP_PIN_CAPS(caps);
	if (!DP_STATUS_MF_PREF(status))
		pin_caps &= ~DP_PIN_MF_MASK;
	if (PD_DP_SIGNAL_GEN2(caps))
		pin_caps &= ~DP_PIN_DP_MASK;
	else
		pin_caps &= ~DP_PIN_BR2_MASK;
	if (pin_caps & (DP_PIN_C | DP_PIN_D))
		pin_caps &= ~(DP_PIN_E | DP_PIN_F);
	if (pin_caps == 0)
		return (0);
	return (1 << (31 - __builtin_clz(pin_caps)));
}

/* -----------------------------------------------------------------------
 * TX state machine for USB-PD policy data sends.
 * ----------------------------------------------------------------------- */
static int
fusb302_policy_send_data_locked(struct fusb302_softc *sc)
{
	int error;

	switch (sc->tx_state) {
	case FUSB_TX_IDLE:
		error = fusb302_fifo_write_locked(sc);
		if (error != 0) {
			device_printf(sc->dev, "FIFO write error: %d\n", error);
			sc->tx_state = FUSB_TX_FAILED;
		} else {
			sc->tx_state = FUSB_TX_BUSY;
		}
		break;
	default:
		/* wait for hardware TXSENT or RETRYFAIL alert */
		break;
	}
	return (sc->tx_state);
}

/* -----------------------------------------------------------------------
 * VDM receive processing
 * ----------------------------------------------------------------------- */
static void
fusb302_process_vdm_msg_locked(struct fusb302_softc *sc)
{
	uint32_t hdr;
	int i;
	uint32_t tmp;

	hdr = sc->rec_load[0];
	if (!VDM_GET_STRUCT(hdr)) {
		device_printf(sc->dev, "VDM: unstructured, ignored\n");
		return;
	}

	switch (VDM_GET_CMD_TYPE(hdr)) {
	case VDM_TYPE_INIT:
		if (VDM_GET_CMD(hdr) == 0x06 /* ATTENTION */) {
			sc->notify_dp_status = sc->rec_load[1] & 0xff;
			FUSB302_DPRINTF(sc, "VDM attention dp_status=0x%x\n",
			    sc->notify_dp_status);
			/*
			 * Refresh exported DP altmode state. VDM Attention
			 * is how the partner tells us about HPD transitions
			 * after Enter_Mode/DP_Configure — without this call
			 * the cdn_dp consumer sees the pre-HPD snapshot
			 * (dp_ready=0) forever.
			 */
			fusb302_notify_dp_locked(sc);
		}
		break;

	case VDM_TYPE_ACK:
		switch (VDM_GET_CMD(hdr)) {
		case VDM_DISC_ID:
			sc->vdm_id = sc->rec_load[1];
			break;
		case VDM_DISC_SVIDS:
			for (i = 0; i < 6; i++) {
				tmp = (sc->rec_load[i + 1] >> 16) & 0xffff;
				if (tmp)
					sc->vdm_svid[sc->vdm_svid_num++] = tmp;
				else
					break;
				tmp = sc->rec_load[i + 1] & 0xffff;
				if (tmp)
					sc->vdm_svid[sc->vdm_svid_num++] = tmp;
				else
					break;
			}
			break;
		case VDM_DISC_MODES:
			if (PD_HDR_CNT(sc->rec_head) > 1) {
				tmp = sc->rec_load[1];
				if (((tmp >> 8) & 0x3f) || ((tmp >> 16) & 0x3f)) {
					sc->notify_dp_caps = tmp;
					sc->notify_pin_def = 0;
					sc->notify_pin_support = PD_DP_PIN_CAPS(tmp);
					FUSB302_DPRINTF(sc,
					    "VDM DP caps=0x%08x pin_support=0x%x\n",
					    tmp, sc->notify_pin_support);
				}
				sc->val_tmp |= 1;
			}
			break;
		case VDM_ENTER_MODE:
			sc->val_tmp = 1;
			break;
		case VDM_DP_STATUS:
			sc->notify_dp_status = sc->rec_load[1] & 0xff;
			FUSB302_DPRINTF(sc, "VDM DP status=0x%08x\n",
			    sc->rec_load[1]);
			sc->val_tmp = 1;
			break;
		case VDM_DP_CONFIG:
			sc->val_tmp = 1;
			sc->notify_is_enter_mode = true;
			FUSB302_DPRINTF(sc,
			    "VDM DP config OK, pin_assignment=0x%x\n",
			    sc->notify_pin_def);
			break;
		default:
			break;
		}
		break;

	case VDM_TYPE_NACK:
		FUSB302_DPRINTF(sc, "VDM NACK for cmd=0x%x\n",
		    VDM_GET_CMD(hdr));
		sc->vdm_state = VDM_ERR_ST;
		break;
	}
}

/* -----------------------------------------------------------------------
 * VDM send sub-functions.
 * Each returns: 0 = success, -EINPROGRESS = still waiting, <0 = error
 * ----------------------------------------------------------------------- */

/* Auto-VDM step+state encoding macro. */
#define	VDM_HANDLE(fn, sc, evt, cond)			\
do {							\
	(cond) = (fn)((sc), (evt));			\
	if ((cond) == 0) {				\
		(sc)->vdm_state++;			\
		(sc)->work_continue |= FUSB_EVT_CONTINUE;\
	} else if ((cond) != -EINPROGRESS) {		\
		(sc)->vdm_state = VDM_ERR_ST;		\
	}						\
} while (0)

static int
fusb302_vdm_send_discid_locked(struct fusb302_softc *sc, uint32_t evt)
{
	int tmp;

	switch (sc->vdm_send_state) {
	case 0:
		fusb302_set_vdm_mesg_locked(sc, VDM_DISC_ID, VDM_TYPE_INIT, 0);
		sc->vdm_id = 0;
		sc->tx_state = FUSB_TX_IDLE;
		sc->vdm_send_state++;
		/* FALLTHROUGH */
	case 1:
		tmp = fusb302_policy_send_data_locked(sc);
		if (tmp == FUSB_TX_SUCCESS) {
			sc->vdm_send_state++;
			fusb302_start_state_timer(sc, T_SENDER_RESPONSE);
		} else if (tmp == FUSB_TX_FAILED) {
			FUSB302_DPRINTF(sc, "VDM DISC_ID TX failed\n");
			return (-EIO);
		}
		if (sc->vdm_send_state != 2)
			break;
		/* FALLTHROUGH */
	default:
		if (sc->vdm_id) {
			sc->vdm_send_state = 0;
			return (0);
		} else if (evt & FUSB_EVT_TIMER_STATE) {
			FUSB302_DPRINTF(sc, "VDM DISC_ID timeout\n");
			sc->work_continue |= FUSB_EVT_CONTINUE;
			return (-ETIMEDOUT);
		}
		break;
	}
	return (-EINPROGRESS);
}

static int
fusb302_vdm_send_discsvid_locked(struct fusb302_softc *sc, uint32_t evt)
{
	int tmp;

	switch (sc->vdm_send_state) {
	case 0:
		fusb302_set_vdm_mesg_locked(sc, VDM_DISC_SVIDS, VDM_TYPE_INIT,
		    0);
		sc->vdm_svid_num = 0;
		memset(sc->vdm_svid, 0, sizeof(sc->vdm_svid));
		sc->tx_state = FUSB_TX_IDLE;
		sc->vdm_send_state++;
		/* FALLTHROUGH */
	case 1:
		tmp = fusb302_policy_send_data_locked(sc);
		if (tmp == FUSB_TX_SUCCESS) {
			sc->vdm_send_state++;
			fusb302_start_state_timer(sc, T_SENDER_RESPONSE);
		} else if (tmp == FUSB_TX_FAILED) {
			FUSB302_DPRINTF(sc, "VDM DISC_SVIDS TX failed\n");
			return (-EIO);
		}
		if (sc->vdm_send_state != 2)
			break;
		/* FALLTHROUGH */
	default:
		if (sc->vdm_svid_num) {
			sc->vdm_send_state = 0;
			return (0);
		} else if (evt & FUSB_EVT_TIMER_STATE) {
			FUSB302_DPRINTF(sc, "VDM DISC_SVIDS timeout\n");
			sc->work_continue |= FUSB_EVT_CONTINUE;
			return (-ETIMEDOUT);
		}
		break;
	}
	return (-EINPROGRESS);
}

static int
fusb302_vdm_send_discmodes_locked(struct fusb302_softc *sc, uint32_t evt)
{
	int tmp;

	if ((sc->val_tmp >> 1) != sc->vdm_svid_num) {
		switch (sc->vdm_send_state) {
		case 0:
			fusb302_set_vdm_mesg_locked(sc, VDM_DISC_MODES,
			    VDM_TYPE_INIT, 0);
			sc->tx_state = FUSB_TX_IDLE;
			sc->vdm_send_state++;
			/* FALLTHROUGH */
		case 1:
			tmp = fusb302_policy_send_data_locked(sc);
			if (tmp == FUSB_TX_SUCCESS) {
				sc->vdm_send_state++;
				fusb302_start_state_timer(sc, T_SENDER_RESPONSE);
			} else if (tmp == FUSB_TX_FAILED) {
				FUSB302_DPRINTF(sc,
				    "VDM DISC_MODES TX failed\n");
				return (-EIO);
			}
			if (sc->vdm_send_state != 2)
				break;
			/* FALLTHROUGH */
		default:
			if (sc->val_tmp & 1) {
				sc->val_tmp &= 0xfe;
				sc->val_tmp += 2;
				sc->vdm_send_state = 0;
				sc->work_continue |= FUSB_EVT_CONTINUE;
			} else if (evt & FUSB_EVT_TIMER_STATE) {
				FUSB302_DPRINTF(sc,
				    "VDM DISC_MODES timeout\n");
				sc->work_continue |= FUSB_EVT_CONTINUE;
				return (-ETIMEDOUT);
			}
			break;
		}
	} else {
		sc->val_tmp = 0;
		return (0);
	}
	return (-EINPROGRESS);
}

static int
fusb302_vdm_send_entermode_locked(struct fusb302_softc *sc, uint32_t evt)
{
	int tmp;

	switch (sc->vdm_send_state) {
	case 0:
		fusb302_set_vdm_mesg_locked(sc, VDM_ENTER_MODE, VDM_TYPE_INIT,
		    1);
		sc->notify_is_enter_mode = false;
		sc->tx_state = FUSB_TX_IDLE;
		sc->vdm_send_state++;
		/* FALLTHROUGH */
	case 1:
		tmp = fusb302_policy_send_data_locked(sc);
		if (tmp == FUSB_TX_SUCCESS) {
			sc->vdm_send_state++;
			fusb302_start_state_timer(sc, T_SENDER_RESPONSE);
		} else if (tmp == FUSB_TX_FAILED) {
			FUSB302_DPRINTF(sc, "VDM ENTER_MODE TX failed\n");
			return (-EIO);
		}
		if (sc->vdm_send_state != 2)
			break;
		/* FALLTHROUGH */
	default:
		if (sc->val_tmp) {
			sc->val_tmp = 0;
			sc->vdm_send_state = 0;
			return (0);
		} else if (evt & FUSB_EVT_TIMER_STATE) {
			FUSB302_DPRINTF(sc, "VDM ENTER_MODE timeout\n");
			sc->work_continue |= FUSB_EVT_CONTINUE;
			return (-ETIMEDOUT);
		}
		break;
	}
	return (-EINPROGRESS);
}

static int
fusb302_vdm_send_dpstatus_locked(struct fusb302_softc *sc, uint32_t evt)
{
	int tmp;

	switch (sc->vdm_send_state) {
	case 0:
		fusb302_set_vdm_mesg_locked(sc, VDM_DP_STATUS, VDM_TYPE_INIT,
		    1);
		sc->tx_state = FUSB_TX_IDLE;
		sc->vdm_send_state++;
		/* FALLTHROUGH */
	case 1:
		tmp = fusb302_policy_send_data_locked(sc);
		if (tmp == FUSB_TX_SUCCESS) {
			sc->vdm_send_state++;
			fusb302_start_state_timer(sc, T_SENDER_RESPONSE);
		} else if (tmp == FUSB_TX_FAILED) {
			FUSB302_DPRINTF(sc, "VDM DP_STATUS TX failed\n");
			return (-EIO);
		}
		if (sc->vdm_send_state != 2)
			break;
		/* FALLTHROUGH */
	default:
		if (sc->val_tmp) {
			sc->val_tmp = 0;
			sc->vdm_send_state = 0;
			return (0);
		} else if (evt & FUSB_EVT_TIMER_STATE) {
			FUSB302_DPRINTF(sc, "VDM DP_STATUS timeout\n");
			sc->work_continue |= FUSB_EVT_CONTINUE;
			return (-ETIMEDOUT);
		}
		break;
	}
	return (-EINPROGRESS);
}

static int
fusb302_vdm_send_dpconfig_locked(struct fusb302_softc *sc, uint32_t evt)
{
	int tmp;

	switch (sc->vdm_send_state) {
	case 0:
		/* Compute pin assignment from discovered DP caps */
		sc->notify_pin_def = fusb302_dp_pin_assignment(
		    sc->notify_dp_caps, sc->notify_dp_status);
		fusb302_set_vdm_mesg_locked(sc, VDM_DP_CONFIG, VDM_TYPE_INIT,
		    0);
		sc->tx_state = FUSB_TX_IDLE;
		sc->vdm_send_state++;
		/* FALLTHROUGH */
	case 1:
		tmp = fusb302_policy_send_data_locked(sc);
		if (tmp == FUSB_TX_SUCCESS) {
			sc->vdm_send_state++;
			fusb302_start_state_timer(sc, T_SENDER_RESPONSE);
		} else if (tmp == FUSB_TX_FAILED) {
			FUSB302_DPRINTF(sc, "VDM DP_CONFIG TX failed\n");
			return (-EIO);
		}
		if (sc->vdm_send_state != 2)
			break;
		/* FALLTHROUGH */
	default:
		if (sc->val_tmp) {
			sc->val_tmp = 0;
			sc->vdm_send_state = 0;
			return (0);
		} else if (evt & FUSB_EVT_TIMER_STATE) {
			FUSB302_DPRINTF(sc, "VDM DP_CONFIG timeout\n");
			sc->work_continue |= FUSB_EVT_CONTINUE;
			return (-ETIMEDOUT);
		}
		break;
	}
	return (-EINPROGRESS);
}

/* -----------------------------------------------------------------------
 * Notify CDN-DP of DP Alt Mode state
 * ----------------------------------------------------------------------- */
static void
fusb302_notify_dp_locked(struct fusb302_softc *sc)
{
	bool hpd;

	hpd = DP_STATUS_HPD(sc->notify_dp_status) != 0;

	sc->dp_altmode.valid = true;
	sc->dp_altmode.dp_ready = sc->notify_is_enter_mode && hpd;
	sc->dp_altmode.pin_assignment = sc->notify_pin_def;
	sc->dp_altmode.dp_status = sc->notify_dp_status;
	/*
	 * usb_ss: 0 = DP-only (4-lane), 1 = USB3+DP (2-lane).
	 *
	 * Per the RK3399 TC-PHY lane map, pin assignments C/E use the DP-only
	 * lane configuration while D/F use the USB3+DP mixed configuration.
	 * Treating Pin C as mixed mode forces the wrong PMA_LANE_CFG and breaks
	 * AUX on simple USB-C displays that negotiate Pin C.
	 */
	sc->dp_altmode.usb_ss =
	    ((sc->notify_pin_def & DP_PIN_MF_MASK) != 0) ? 1 : 0;

	FUSB302_DPRINTF(sc,
	    "DP Alt Mode: dp_ready=%d pin=0x%x usb_ss=%d dp_status=0x%x\n",
	    sc->dp_altmode.dp_ready, sc->dp_altmode.pin_assignment,
	    sc->dp_altmode.usb_ss, sc->dp_altmode.dp_status);
}

/* -----------------------------------------------------------------------
 * VDM state machine driving the Discover_Identity → Enter_Mode → DP_Config sequence.
 * ----------------------------------------------------------------------- */
static void
fusb302_auto_vdm_machine_locked(struct fusb302_softc *sc, uint32_t evt)
{
	int cond;

	switch (sc->vdm_state) {
	case VDM_DISC_ID_ST:
		VDM_HANDLE(fusb302_vdm_send_discid_locked, sc, evt, cond);
		break;
	case VDM_DISC_SVID_ST:
		VDM_HANDLE(fusb302_vdm_send_discsvid_locked, sc, evt, cond);
		break;
	case VDM_DISC_MODES_ST:
		VDM_HANDLE(fusb302_vdm_send_discmodes_locked, sc, evt, cond);
		break;
	case VDM_ENTER_MODE_ST:
		VDM_HANDLE(fusb302_vdm_send_entermode_locked, sc, evt, cond);
		break;
	case VDM_DP_STATUS_ST:
		VDM_HANDLE(fusb302_vdm_send_dpstatus_locked, sc, evt, cond);
		break;
	case VDM_DP_CONFIG_ST:
		VDM_HANDLE(fusb302_vdm_send_dpconfig_locked, sc, evt, cond);
		break;
	case VDM_NOTIFY_ST:
		fusb302_notify_dp_locked(sc);
		sc->vdm_state = VDM_READY_ST;
		break;
	default:
		break;
	}
}

/* -----------------------------------------------------------------------
 * PD state functions (DFP / source path)
 * ----------------------------------------------------------------------- */
/*
 * After TOGDONE the sink's PD controller needs time to power up via VBUS and
 * start listening on CC.  Wait ~400ms here; without it the first
 * several src_caps transmissions get RETRYFAIL even though the sink is
 * PD-capable.
 */
/*
 * Wait after Type-C attach before driving PD.  USB-C PD spec tStartupSource
 * is 50-275ms; 500ms keeps a comfortable margin for slow sinks while saving
 * ~1s of boot-to-backlight latency vs the previous 1500ms.  Used by both
 * the SRC (waiting for sink PD init) and SNK (waiting for VBUS) attach paths.
 */
#define	T_ATTACH_WAIT_MS	500

static void
fusb302_state_attached_source_locked(struct fusb302_softc *sc, uint32_t evt)
{
	int polarity, error;
	uint8_t st0;

	switch (sc->sub_state) {
	case 0:
		/*
		 * Trust the previously detected polarity. RockPro64 late-load
		 * source bootstrap can synthesize ATTACHED_SRC before STATUS1A
		 * reflects the final orientation, and re-deriving polarity from
		 * TOGSS here can incorrectly flip CC2 back to CC1.
		 */
		polarity = sc->cc_polarity;
		if (polarity != 0 && polarity != 1) {
			switch ((sc->status1a & FUSB_ST1A_TOGSS_MASK) >>
			    FUSB_ST1A_TOGSS_SHIFT) {
			case 2:	/* src-cc2 */
				polarity = 1;
				break;
			default: /* src-cc1 */
				polarity = 0;
				break;
			}
		}

		/*
		 * Partner-sources-VBUS detection: TOGSS resolved as Source on
		 * our side, but if VBUS is already present (we have not yet
		 * enabled our own regulator) the partner is sourcing it. The
		 * monitor was just slow to flip its CC pull during DRP toggle.
		 * Flip to Sink role rather than fight the partner's Source.
		 *
		 * Gated to DRP only. On this RockPro64 the FUSB302's VBUSOK is
		 * a board-level rail that reads 1 regardless of partner state,
		 * so this heuristic always triggers and undoes any explicit SRC
		 * choice. When the user forces role_pref=SRC, honor it: trust
		 * TOGSS, don't second-guess via VBUSOK.
		 */
		if (sc->role_pref == FUSB_ROLE_DRP && !sc->passive_src &&
		    fusb302_read_reg(sc, FUSB_REG_STATUS0, &st0) == 0 &&
		    (st0 & FUSB_ST0_VBUSOK) && !sc->vbus_enabled) {
			device_printf(sc->dev,
			    "TOGSS=src but partner sources VBUS; "
			    "flipping to UFP/sink on CC%d\n", polarity + 1);

			/*
			 * Drop all CC pulls/MEAS so the partner sees an
			 * electrical detach (~tCCDisconnect 25ms; use 50ms
			 * for margin). ATTACHED_SNK will reapply Rd/MEAS.
			 */
			(void)fusb302_update_reg(sc, FUSB_REG_SWITCHES0,
			    FUSB_SW0_PDWN1 | FUSB_SW0_PDWN2 |
			    FUSB_SW0_PU_EN1 | FUSB_SW0_PU_EN2 |
			    FUSB_SW0_MEAS_CC1 | FUSB_SW0_MEAS_CC2 |
			    FUSB_SW0_VCONN_CC1 | FUSB_SW0_VCONN_CC2, 0);
			DELAY(50000);

			sc->cc_polarity = polarity;
			sc->attached_as_sink = true;
			fusb302_set_state_locked(sc, FUSB_ST_ATTACHED_SNK);
			return;
		}

		sc->notify_is_cc = true;
		sc->notify_power_role = 1;	/* source */
		sc->notify_data_role = 1;	/* DFP */
		sc->hardrst_count = 0;
		sc->attached_as_sink = false;

		/*
		 * Now that we are committed to Source, enable VBUS supply --
		 * UNLESS the partner is already sourcing VBUS (VBUSOK set).
		 * On RockPro64, partner-sourcing VBUS is the normal case for
		 * a USB-C monitor that has its own PSU; enabling our SY6280AAC
		 * switch on top of partner's 5V causes a hang in regulator_enable
		 * (current limit / back-feed protection deadlock).  Treat the
		 * present VBUS as supplied for our purposes.
		 */
		if (sc->passive_src) {
			FUSB302_DPRINTF(sc,
			    "src attach: passive_src=%d, enabling VBUS regulator "
			    "(present Rp on CC%d only)\n",
			    sc->passive_src, polarity + 1);
			if (sc->vbus_supply != NULL && !sc->vbus_enabled) {
				error = regulator_enable(sc->vbus_supply);
				if (error == 0)
					sc->vbus_enabled = true;
				else
					device_printf(sc->dev,
					    "regulator_enable(vbus) failed: %d\n",
					    error);
			} else {
				sc->vbus_enabled = true;
			}
		} else if (sc->vbus_supply != NULL && !sc->vbus_enabled) {
			if (st0 == 0)
				(void)fusb302_read_reg(sc, FUSB_REG_STATUS0, &st0);
			if (st0 & FUSB_ST0_VBUSOK) {
				FUSB302_DPRINTF(sc,
				    "src attach: partner already sources VBUS, "
				    "skipping our regulator_enable\n");
				sc->vbus_enabled = true;
			} else {
				error = regulator_enable(sc->vbus_supply);
				if (error != 0) {
					device_printf(sc->dev,
					    "regulator_enable(vbus) failed: %d\n",
					    error);
				} else {
					sc->vbus_enabled = true;
				}
			}
		}

		sc->vconn_enabled = true;
		fusb302_set_polarity_locked(sc, polarity);

		if (sc->passive_src) {
			uint8_t sw1_set;
			/*
			 * Passive SRC: chip is now presenting Rp on the active
			 * CC pin (PU_EN1+MEAS_CC1+VCONN_CC2). Match the reference
			 * build's working SWITCHES1=0xd5 by setting POWER_ROLE=Source,
			 * DATA_ROLE=DFP, AUTO_CRC=1, SPEC_REV=PD3.  AUTO_CRC is
			 * critical: if the display sends Discover_Identity or any
			 * other PD message, the chip auto-ACKs with GoodCRC at
			 * the BMC layer.  Without it, every incoming message is
			 * dropped and the display gives up.
			 *
			 * Use fusb302_update_reg directly (single I2C RMW) rather
			 * than calling fusb302_set_msg_header_locked +
			 * fusb302_enable_rx_locked, which observed a witness
			 * panic ("sleeping thread holds fusb3020") when invoked
			 * from this state context.  set_polarity_locked above
			 * already proved that fusb302_update_reg is safe here.
			 *
			 * Park after this: don't progress to SRC_SEND_CAPS (would
			 * TX Source_Caps and loop on RETRYFAIL with a DP-only
			 * partner that doesn't speak PD).  AUTO_CRC handles any
			 * PD activity from the display in passive RX-only mode.
			 */
			sc->notify_is_cc = true;
			sc->notify_power_role = 1;	/* Source */
			sc->notify_data_role = 1;	/* DFP */

			sw1_set = FUSB_SW1_POWERROLE | FUSB_SW1_DATAROLE |
			    FUSB_SW1_AUTO_CRC |
			    ((sc->pd_spec_rev & 0x3) << 5);
			(void)fusb302_update_reg(sc, FUSB_REG_SWITCHES1,
			    FUSB_SW1_POWERROLE | FUSB_SW1_DATAROLE |
			    FUSB_SW1_AUTO_CRC | FUSB_SW1_SPECREV,
			    sw1_set);

			/* Match the reference build's MASK1 transition on attach:
			 * unmask BC_LVL change (bit 5).  Idle init wrote 0x75;
			 * active session value is 0x55. */
			(void)fusb302_write_reg(sc, FUSB_REG_MASK1,
			    FUSB_MASK1_ATTACHED);

			if (sc->passive_src == 2) {
				/* passive_src=2: regs set with AUTO_CRC, now fall
				 * through to normal SRC progression (SRC_STARTUP
				 * → SRC_SEND_CAPS → full PD negotiation). Use
				 * this when partner can speak PD if we present
				 * SRC + AUTO_CRC properly. */
				FUSB302_DPRINTF(sc, "attached as DFP on CC%d, "
				    "VCONN on CC%d, AUTO_CRC set, proceeding "
				    "to send_caps\n",
				    polarity + 1, (polarity == 0) ? 2 : 1);
				/* fall through to non-passive setup below */
			} else {
				device_printf(sc->dev, "attached as passive DFP on CC%d, "
				    "VCONN on CC%d, AUTO_CRC+POWER+DATA roles set, "
				    "idle (waiting for partner VDMs)\n",
				    polarity + 1, (polarity == 0) ? 2 : 1);
				sc->sub_state = 99;	/* park in default-no-op branch */
				break;
			}
		}

		FUSB302_DPRINTF(sc, "attached as DFP on CC%d, VCONN on CC%d, "
		    "waiting %dms for sink PD init\n",
		    polarity + 1, (polarity == 0) ? 2 : 1, T_ATTACH_WAIT_MS);
		fusb302_start_state_timer(sc, T_ATTACH_WAIT_MS);
		sc->sub_state++;
		break;

	default:
		if (sc->passive_src == 1) {
			/* passive_src=1 ONLY: stay parked. passive_src=2 falls
			 * through to normal SRC progression below. */
			break;
		}
		if (evt & FUSB_EVT_TIMER_STATE)
			fusb302_set_state_locked(sc, FUSB_ST_SRC_STARTUP);
		break;
	}
}

static void
fusb302_state_src_startup_locked(struct fusb302_softc *sc,
    uint32_t evt __unused)
{
	sc->notify_is_pd = false;
	fusb302_reset_pd_params_locked(sc);
	memset(sc->partner_cap, 0, sizeof(sc->partner_cap));

	/*
	 * Match Linux 4.4 fusb_state_src_startup exactly: no PD_RESET
	 * here.  Linux only PD_RESETs in tcpm_init (boot) and on hard
	 * reset.  We previously did an extra reset here which may have
	 * been wiping BMC decoder state right before we expected to RX
	 * GoodCRC.
	 */
	fusb302_set_msg_header_locked(sc);
	fusb302_set_polarity_locked(sc, sc->cc_polarity);
	fusb302_enable_rx_locked(sc, true);

	FUSB302_DPRINTF(sc, "src_startup: PD init done, sending caps\n");
	fusb302_set_state_locked(sc, FUSB_ST_SRC_SEND_CAPS);
}

static void
fusb302_state_src_send_caps_locked(struct fusb302_softc *sc, uint32_t evt)
{
	int tmp;

	switch (sc->sub_state) {
	case 0:
		FUSB302_DPRINTF(sc, "send_caps: writing src caps to FIFO\n");
		fusb302_set_mesg_srccap_locked(sc);
		sc->tx_state = FUSB_TX_IDLE;
		sc->sub_state++;
		/* FALLTHROUGH */
	case 1:
		/*
		 * Some partners reply immediately after seeing Source_Caps, but
		 * this FUSB302 path occasionally never surfaces TXSENT first.
		 * If we already have a received PD packet, treat that as proof
		 * the caps frame made it onto the wire and advance to the
		 * request-handling branch instead of spinning forever in BUSY.
		 */
		if ((evt & FUSB_EVT_RX) && sc->tx_state == FUSB_TX_BUSY) {
			FUSB302_DPRINTF(sc,
			    "send_caps: RX arrived before TXSENT, assuming caps delivered\n");
			sc->tx_state = FUSB_TX_SUCCESS;
			sc->sub_state = 2;
			FUSB302_DPRINTF(sc, "send_caps: TXSENT, waiting for REQUEST\n");
			sc->hardrst_count = 0;
			sc->caps_counter = 0;
			sc->is_pd_support = true;
			fusb302_start_state_timer(sc, T_SENDER_RESPONSE);
			break;
		}
		tmp = fusb302_policy_send_data_locked(sc);
		if (tmp == FUSB_TX_SUCCESS) {
			FUSB302_DPRINTF(sc, "send_caps: TXSENT, waiting for REQUEST\n");
			sc->hardrst_count = 0;
			sc->caps_counter = 0;
			sc->is_pd_support = true;
			fusb302_start_state_timer(sc, T_SENDER_RESPONSE);
			sc->sub_state++;
		} else if (tmp == FUSB_TX_FAILED) {
			uint8_t intr, intra, intrb, st0, st1, ctl3, sw0, sw1;

			sc->caps_counter++;
			(void)fusb302_read_reg(sc, FUSB_REG_INTERRUPT, &intr);
			(void)fusb302_read_reg(sc, FUSB_REG_INTERRUPTA, &intra);
			(void)fusb302_read_reg(sc, FUSB_REG_INTERRUPTB, &intrb);
			(void)fusb302_read_reg(sc, FUSB_REG_STATUS0, &st0);
			(void)fusb302_read_reg(sc, FUSB_REG_STATUS1, &st1);
			(void)fusb302_read_reg(sc, FUSB_REG_CONTROL3, &ctl3);
			(void)fusb302_read_reg(sc, FUSB_REG_SWITCHES0, &sw0);
			(void)fusb302_read_reg(sc, FUSB_REG_SWITCHES1, &sw1);
			FUSB302_DPRINTF(sc,
			    "send_caps: RETRYFAIL caps_counter=%d "
			    "intr=%02x intra=%02x intrb=%02x "
			    "st0=%02x st1=%02x ctl3=%02x sw0=%02x sw1=%02x\n",
			    sc->caps_counter, intr, intra, intrb,
			    st0, st1, ctl3, sw0, sw1);
			if (sc->caps_counter >= 50) {
				/*
				 * No GoodCRC from partner after N_CAPS_COUNT
				 * attempts.  Per USB-PD R3.0 §6.8.3, the
				 * source MAY issue a Hard_Reset to recover
				 * from a Non-Responsive Sink.  This drops
				 * VBUS for tHardResetComplete (~25-35ms) and
				 * forces the partner's PD chip out of any
				 * stuck state.  After Hard_Reset completes,
				 * we re-enter SRC_STARTUP and try caps again.
				 */
				if (sc->hardrst_count <= 0) {
					FUSB302_DPRINTF(sc,
					    "send_caps: %d RETRYFAILs on CC%d, "
					    "issuing Hard_Reset to wake "
					    "non-responsive sink\n",
					    sc->caps_counter,
					    sc->cc_polarity + 1);
					sc->caps_counter = 0;
					fusb302_set_state_locked(sc,
					    FUSB_ST_SRC_SEND_HARDRST);
				} else {
					device_printf(sc->dev,
					    "send_caps: giving up after %d "
					    "attempts on CC%d "
					    "(no PD partner, Hard_Reset "
					    "didn't help)\n",
					    sc->caps_counter,
					    sc->cc_polarity + 1);
					fusb302_set_state_locked(sc,
					    FUSB_ST_DISABLED);
				}
			} else {
				fusb302_start_state_timer(sc,
				    T_TYPEC_SEND_SRCCAP);
				fusb302_set_state_locked(sc, FUSB_ST_SRC_SEND_CAPS);
			}
			return;
		}
		if (sc->sub_state != 2)
			break;
		/* FALLTHROUGH */
	default:
		if (evt & FUSB_EVT_RX) {
			if (PD_IS_DATA(sc->rec_head, PD_DMT_REQUEST)) {
				fusb302_set_state_locked(sc,
				    FUSB_ST_SRC_NEGOTIATE_CAP);
			} else {
				fusb302_set_state_locked(sc,
				    FUSB_ST_SRC_SEND_SOFTRST);
			}
		} else if (evt & FUSB_EVT_TIMER_STATE) {
			device_printf(sc->dev,
			    "send_caps: timeout, no REQUEST (hardrst_count=%d)\n",
			    sc->hardrst_count);
			if (sc->hardrst_count <= 0)
				fusb302_set_state_locked(sc,
				    FUSB_ST_SRC_SEND_HARDRST);
			else
				fusb302_set_state_locked(sc, FUSB_ST_DISABLED);
		}
		break;
	}
}

static void
fusb302_state_src_negotiate_cap_locked(struct fusb302_softc *sc,
    uint32_t evt __unused)
{
	/* We only advertise one PDO; any valid REQUEST position is OK */
	fusb302_set_state_locked(sc, FUSB_ST_SRC_TRANSITION_SUPPLY);
}

static void
fusb302_state_src_transition_supply_locked(struct fusb302_softc *sc,
    uint32_t evt)
{
	int tmp;

	switch (sc->sub_state) {
	case 0:
		fusb302_set_mesg_ctrl_locked(sc, PD_CMT_ACCEPT);
		sc->tx_state = FUSB_TX_IDLE;
		sc->sub_state++;
		/* FALLTHROUGH */
	case 1:
		tmp = fusb302_policy_send_data_locked(sc);
		if (tmp == FUSB_TX_SUCCESS) {
			fusb302_start_state_timer(sc, T_SRC_TRANSITION);
			sc->sub_state++;
		} else if (tmp == FUSB_TX_FAILED) {
			fusb302_set_state_locked(sc, FUSB_ST_SRC_SEND_SOFTRST);
		}
		break;
	case 2:
		if (evt & FUSB_EVT_TIMER_STATE) {
			sc->notify_is_pd = true;
			fusb302_set_mesg_ctrl_locked(sc, PD_CMT_PS_RDY);
			sc->tx_state = FUSB_TX_IDLE;
			sc->sub_state++;
			sc->work_continue |= FUSB_EVT_CONTINUE;
		}
		break;
	default:
		tmp = fusb302_policy_send_data_locked(sc);
		if (tmp == FUSB_TX_SUCCESS) {
			FUSB302_DPRINTF(sc, "PD connected as DFP (5V)\n");
			fusb302_set_state_locked(sc, FUSB_ST_SRC_READY);
		} else if (tmp == FUSB_TX_FAILED) {
			fusb302_set_state_locked(sc, FUSB_ST_SRC_SEND_SOFTRST);
		}
		break;
	}
}

static void
fusb302_state_src_ready_locked(struct fusb302_softc *sc, uint32_t evt)
{
	bool vdm_active;

	vdm_active = (sc->notify_data_role &&
	    sc->vdm_state < VDM_READY_ST);

	if (evt & FUSB_EVT_CONTINUE)
		if (bootverbose)
			FUSB302_DPRINTF(sc,
			    "src_ready: vdm_active=%d vdm_state=%d\n",
			    vdm_active, sc->vdm_state);

	if (evt & FUSB_EVT_RX) {
		if (PD_IS_DATA(sc->rec_head, PD_DMT_VDM)) {
			fusb302_process_vdm_msg_locked(sc);
			sc->work_continue |= FUSB_EVT_CONTINUE;
			/* stop state timer so it doesn't race VDM timeout */
			callout_stop(&sc->timer_state);
		} else if (!vdm_active) {
			/* ignore swap requests for now */
		}
	}

	/*
	 * Skip Get_Sink_Cap entirely.  The reference tcpm/fusb302 path does not
	 * issue Get_Sink_Cap before VDM altmode discovery on DFP/source role,
	 * and many USB-C displays (the partner here is one) do not reply to
	 * Get_Sink_Cap, leaving us indefinitely stuck in SRC_GET_SINK_CAPS
	 * and never starting VDM Discover_Identity.  partner_cap is only
	 * consumed by the SNK path, so it is fine to leave it unpopulated.
	 */
	if (vdm_active)
		fusb302_auto_vdm_machine_locked(sc, evt);
}

static void
fusb302_state_src_get_sink_caps_locked(struct fusb302_softc *sc, uint32_t evt)
{
	int tmp;
	uint32_t i;

	switch (sc->sub_state) {
	case 0:
		fusb302_set_mesg_ctrl_locked(sc, PD_CMT_GETSINKCAP);
		sc->tx_state = FUSB_TX_IDLE;
		sc->sub_state++;
		/* FALLTHROUGH */
	case 1:
		tmp = fusb302_policy_send_data_locked(sc);
		if (tmp == FUSB_TX_SUCCESS) {
			fusb302_start_state_timer(sc, T_SENDER_RESPONSE);
			sc->sub_state++;
		} else if (tmp == FUSB_TX_FAILED) {
			fusb302_set_state_locked(sc, FUSB_ST_SRC_SEND_SOFTRST);
		}
		if (sc->sub_state != 2)
			break;
		/* FALLTHROUGH */
	default:
		if (evt & FUSB_EVT_RX) {
			if (PD_IS_DATA(sc->rec_head, PD_DMT_SINKCAP)) {
				for (i = 0; i < PD_HDR_CNT(sc->rec_head); i++)
					sc->partner_cap[i] = sc->rec_load[i];
			} else {
				sc->partner_cap[0] = 0xffffffff;
			}
			fusb302_set_state_locked(sc, FUSB_ST_SRC_READY);
		} else if (evt & FUSB_EVT_TIMER_STATE) {
			device_printf(sc->dev, "GET_SINK_CAP timeout\n");
			sc->partner_cap[0] = 0xffffffff;
			fusb302_set_state_locked(sc, FUSB_ST_SRC_READY);
		}
		break;
	}
}

static void
fusb302_state_src_send_hardrst_locked(struct fusb302_softc *sc,
    uint32_t evt)
{
	int error;

	switch (sc->sub_state) {
	case 0:
		error = fusb302_update_reg(sc, FUSB_REG_CONTROL3,
		    FUSB_CTL3_SEND_HARDRST, FUSB_CTL3_SEND_HARDRST);
		if (error != 0)
			device_printf(sc->dev, "hard reset send failed\n");
		sc->sub_state++;
		fusb302_start_state_timer(sc, 30);
		break;
	default:
		if (evt & FUSB_EVT_TIMER_STATE) {
			sc->hardrst_count++;
			fusb302_set_state_locked(sc,
			    FUSB_ST_SRC_TRANSITION_DEFAULT);
		}
		break;
	}
}

static void
fusb302_state_src_transition_default_locked(struct fusb302_softc *sc,
    uint32_t evt)
{
	switch (sc->sub_state) {
	case 0:
		sc->notify_is_pd = false;
		fusb302_start_state_timer(sc, 830);	/* T_SRC_RECOVER */
		sc->sub_state++;
		break;
	default:
		if (evt & FUSB_EVT_TIMER_STATE) {
			fusb302_set_state_locked(sc, FUSB_ST_SRC_STARTUP);
		}
		break;
	}
}

static void
fusb302_state_src_softrst_locked(struct fusb302_softc *sc)
{
	int tmp;

	switch (sc->sub_state) {
	case 0:
		fusb302_set_mesg_ctrl_locked(sc, PD_CMT_ACCEPT);
		sc->tx_state = FUSB_TX_IDLE;
		sc->sub_state++;
		/* FALLTHROUGH */
	default:
		tmp = fusb302_policy_send_data_locked(sc);
		if (tmp == FUSB_TX_SUCCESS) {
			fusb302_reset_pd_params_locked(sc);
			fusb302_set_state_locked(sc, FUSB_ST_SRC_SEND_CAPS);
		} else if (tmp == FUSB_TX_FAILED) {
			fusb302_set_state_locked(sc, FUSB_ST_SRC_SEND_HARDRST);
		}
		break;
	}
}

static void
fusb302_state_src_send_softrst_locked(struct fusb302_softc *sc, uint32_t evt)
{
	int tmp;

	switch (sc->sub_state) {
	case 0:
		fusb302_set_mesg_ctrl_locked(sc, PD_CMT_SOFTRESET);
		sc->tx_state = FUSB_TX_IDLE;
		sc->sub_state++;
		/* FALLTHROUGH */
	case 1:
		tmp = fusb302_policy_send_data_locked(sc);
		if (tmp == FUSB_TX_SUCCESS) {
			fusb302_start_state_timer(sc, T_SENDER_RESPONSE);
			sc->sub_state++;
		} else if (tmp == FUSB_TX_FAILED) {
			fusb302_set_state_locked(sc, FUSB_ST_SRC_SEND_HARDRST);
		}
		if (sc->sub_state != 2)
			break;
		/* FALLTHROUGH */
	default:
		if (evt & FUSB_EVT_RX) {
			if (PD_IS_CTRL(sc->rec_head, PD_CMT_ACCEPT)) {
				fusb302_reset_pd_params_locked(sc);
				fusb302_set_state_locked(sc,
				    FUSB_ST_SRC_SEND_CAPS);
			}
		} else if (evt & FUSB_EVT_TIMER_STATE) {
			fusb302_set_state_locked(sc, FUSB_ST_SRC_SEND_HARDRST);
		}
		break;
	}
}

/* -----------------------------------------------------------------------
 * PD state functions (UFP / sink path)
 * ----------------------------------------------------------------------- */
static void
fusb302_state_attached_sink_locked(struct fusb302_softc *sc, uint32_t evt)
{
	int polarity;

	switch (sc->sub_state) {
	case 0:
		switch ((sc->status1a & FUSB_ST1A_TOGSS_MASK) >>
		    FUSB_ST1A_TOGSS_SHIFT) {
		case FUSB_TOGSS_SNK_CC2:
			polarity = 1;
			break;
		default:
			polarity = 0;
			break;
		}

		sc->notify_is_cc = true;
		sc->notify_power_role = 0;	/* sink */
		sc->notify_data_role = 0;	/* UFP */
		sc->hardrst_count = 0;
		sc->softrst_tried = false;
		sc->attached_as_sink = true;

		fusb302_set_polarity_locked(sc, polarity);
		FUSB302_DPRINTF(sc, "attached as UFP on CC%d, "
		    "waiting %dms for VBUS\n",
		    polarity + 1, T_ATTACH_WAIT_MS);
		fusb302_start_state_timer(sc, T_ATTACH_WAIT_MS);
		sc->sub_state++;
		break;

	default:
		if (evt & FUSB_EVT_TIMER_STATE)
			fusb302_set_state_locked(sc, FUSB_ST_SNK_STARTUP);
		break;
	}
}

static void
fusb302_state_snk_startup_locked(struct fusb302_softc *sc,
    uint32_t evt __unused)
{
	sc->notify_is_pd = false;
	fusb302_reset_pd_params_locked(sc);
	memset(sc->partner_cap, 0, sizeof(sc->partner_cap));

	fusb302_pd_reset_locked(sc);
	fusb302_set_msg_header_locked(sc);
	fusb302_set_polarity_locked(sc, sc->cc_polarity);
	fusb302_enable_rx_locked(sc, true);

	if (sc->skip_pd) {
		/*
		 * DP-only partner: display does not run a PD message engine
		 * (no GoodCRC for any of our TX). Skip the entire
		 * SNK_DISCOVERY -> wait-caps -> hard/soft-reset escalation
		 * and synthesize the dp_altmode struct directly with the
		 * RockPro64-canonical Pin C / 4-lane DP-only defaults, then
		 * notify the policy SM so rk_cdn_dp can bring up the DP TX.
		 */
		sc->dp_altmode.valid = true;
		sc->dp_altmode.dp_ready = true;
		sc->dp_altmode.pin_assignment = DP_PIN_C;
		sc->dp_altmode.usb_ss = 0;	/* Pin C is 4-lane DP-only */
		/* DP_Status VDO bit 7 = HPD; cdn_dp's altmode_signature_ok
		 * requires it set before it will run mailbox bring-up. */
		sc->dp_altmode.dp_status = (1u << 7);
		FUSB302_DPRINTF(sc,
		    "snk_startup: skip_pd=1, DP-only attach on CC%d, "
		    "synthesizing Pin C 4-lane DP\n", sc->cc_polarity + 1);
		if (sc->policy != NULL)
			usbc_pd_policy_event(sc->policy,
			    USBC_PD_E_PORT_ENABLE);
		fusb302_set_state_locked(sc, FUSB_ST_SNK_READY);
		return;
	}

	FUSB302_DPRINTF(sc,
	    "snk_startup: PD init done, listening for src caps\n");
	fusb302_set_state_locked(sc, FUSB_ST_SNK_DISCOVERY);
}

static void
fusb302_state_snk_discovery_locked(struct fusb302_softc *sc, uint32_t evt)
{
	int n, i;

	if (sc->sub_state == 0) {
		fusb302_start_state_timer(sc, T_TYPEC_SINK_WAIT_CAP);
		sc->sub_state = 1;
	}
	if (evt & FUSB_EVT_RX) {
		if (PD_IS_DATA(sc->rec_head, PD_DMT_SRCCAP)) {
			n = PD_HDR_CNT(sc->rec_head);
			if (n > 7)
				n = 7;
			for (i = 0; i < n; i++)
				sc->partner_cap[i] = sc->rec_load[i];
			for (; i < 7; i++)
				sc->partner_cap[i] = 0;
			sc->is_pd_support = true;
			FUSB302_DPRINTF(sc,
			    "snk: src_caps n=%d PDO[0]=0x%08x\n",
			    n, sc->partner_cap[0]);
			fusb302_set_state_locked(sc,
			    FUSB_ST_SNK_EVALUATE_CAPS);
			return;
		}
	}
	if (evt & FUSB_EVT_TIMER_STATE) {
		if (sc->skip_pd) {
			device_printf(sc->dev,
			    "snk: skip_pd=1, no PD on CC%d; going DISABLED "
			    "(no DP altmode without real VDM)\n",
			    sc->cc_polarity + 1);
			fusb302_set_state_locked(sc, FUSB_ST_DISABLED);
		} else if (!sc->softrst_tried) {
			/*
			 * Reference precedent: try soft reset before hard reset.
			 * Some sources fail to resume Source_Capabilities
			 * after a hard reset but recover after a soft reset
			 * (PE_SNK_Send_Soft_Reset).  See the reference tcpm.c
			 * SNK_WAIT_CAPABILITIES_TIMEOUT.
			 */
			sc->softrst_tried = true;
			FUSB302_DPRINTF(sc,
			    "snk: no src caps within %dms; trying soft "
			    "reset before hard reset\n",
			    T_TYPEC_SINK_WAIT_CAP);
			fusb302_set_state_locked(sc,
			    FUSB_ST_SNK_SEND_SOFTRST);
		} else if (sc->hardrst_count <= N_SNK_HARDRESET_RETRY) {
			FUSB302_DPRINTF(sc,
			    "snk: no src caps within %dms; sending hard "
			    "reset (try %d/%d) to wake partner\n",
			    T_TYPEC_SINK_WAIT_CAP, sc->hardrst_count + 1,
			    N_SNK_HARDRESET_RETRY + 1);
			fusb302_set_state_locked(sc,
			    FUSB_ST_SNK_SEND_HARDRST);
		} else if (!sc->role_discovery_complete) {
			device_printf(sc->dev,
			    "snk: partner not PD-capable after %d retries on "
			    "CC%d; flipping to forced SRC (role discovery)\n",
			    N_SNK_HARDRESET_RETRY + 1, sc->cc_polarity + 1);
			fusb302_set_state_locked(sc,
			    FUSB_ST_ROLE_DISCOVERY_SRC);
		} else {
			device_printf(sc->dev,
			    "snk: partner not PD-capable after %d retries on "
			    "CC%d and SRC flip already tried; going DISABLED\n",
			    N_SNK_HARDRESET_RETRY + 1, sc->cc_polarity + 1);
			fusb302_set_state_locked(sc, FUSB_ST_DISABLED);
		}
	}
}

/*
 * SNK_SEND_SOFTRST: transmit a PD Soft_Reset control message and return to
 * SNK_DISCOVERY to listen for Source_Capabilities. If the partner accepts
 * the soft reset, it should resend caps. If TX fails or no caps arrive
 * before the SNK_DISCOVERY timer fires again, softrst_tried is now set so
 * SNK_DISCOVERY will escalate to a hard reset on the next cycle.
 */
static void
fusb302_state_snk_send_softrst_locked(struct fusb302_softc *sc, uint32_t evt)
{
	int tmp;

	switch (sc->sub_state) {
	case 0:
		fusb302_set_mesg_ctrl_locked(sc, PD_CMT_SOFTRESET);
		sc->tx_state = FUSB_TX_IDLE;
		sc->sub_state++;
		/* FALLTHROUGH */
	case 1:
		tmp = fusb302_policy_send_data_locked(sc);
		if (tmp == FUSB_TX_SUCCESS) {
			fusb302_reset_pd_params_locked(sc);
			fusb302_set_state_locked(sc, FUSB_ST_SNK_DISCOVERY);
		} else if (tmp == FUSB_TX_FAILED) {
			fusb302_set_state_locked(sc,
			    FUSB_ST_SNK_SEND_HARDRST);
		}
		break;
	}
}

static void
fusb302_state_snk_send_hardrst_locked(struct fusb302_softc *sc, uint32_t evt)
{
	int error;

	switch (sc->sub_state) {
	case 0:
		/*
		 * Increment counter before TX so HARDSENT interrupt routing
		 * (which jumps straight to SNK_TRANSITION_DEFAULT) doesn't
		 * skip our retry accounting.
		 */
		sc->hardrst_count++;
		error = fusb302_update_reg(sc, FUSB_REG_CONTROL3,
		    FUSB_CTL3_SEND_HARDRST, FUSB_CTL3_SEND_HARDRST);
		if (error != 0)
			device_printf(sc->dev,
			    "snk hard reset send failed: %d\n", error);
		sc->sub_state++;
		fusb302_start_state_timer(sc, 30);
		break;
	default:
		if (evt & FUSB_EVT_TIMER_STATE)
			fusb302_set_state_locked(sc,
			    FUSB_ST_SNK_TRANSITION_DEFAULT);
		break;
	}
}

/*
 * Role-discovery via re-toggle.
 *
 * Entered from SNK_STARTUP when the partner doesn't honor SRC role
 * (no Source_Capabilities even after hard-reset retries).  When two
 * DRP devices each toggle independently, the side that happens to
 * present Rp during the other's measurement window wins SRC.  The
 * outcome is essentially random.  Linux on identical hardware lands
 * as SRC and works; we landed as SNK and got stuck.
 *
 * Strategy: drop our terminations to release the chip's settled
 * TOGSS state, re-enable DRP TOGGLE, and let the next TOGDONE roll
 * the dice again.  Up to ROLE_DISCOVERY_MAX_TOGGLES attempts; if
 * TOGSS keeps landing on SNK, give up.
 *
 *   sub_state 0: drop CC terminations + cancel TOGGLE, arm 300 ms
 *                so the partner sees electrical detach and our
 *                chip exits its previous TOGSS state.
 *   sub_state 1: re-enable DRP MODE + TOGGLE, then return to the
 *                normal IRQ-driven TOGDONE flow.  fusb302_irq_task
 *                routes new TOGDONE through fusb302_state_attached_*
 *                exactly like a fresh attach -- if it lands SRC,
 *                normal SRC bring-up runs; if it lands SNK again,
 *                snk_startup fires and on its next giveup we'll
 *                end up here again, incrementing the attempt count.
 */
#define	ROLE_DISCOVERY_MAX_TOGGLES	5

static void
fusb302_state_role_discovery_src_locked(struct fusb302_softc *sc,
    uint32_t evt)
{

	switch (sc->sub_state) {
	case 0:
		/* Electrical detach: drop pulls, MEAS, VCONN, cancel TOGGLE. */
		(void)fusb302_update_reg(sc, FUSB_REG_SWITCHES0,
		    FUSB_SW0_PDWN1 | FUSB_SW0_PDWN2 |
		    FUSB_SW0_PU_EN1 | FUSB_SW0_PU_EN2 |
		    FUSB_SW0_MEAS_CC1 | FUSB_SW0_MEAS_CC2 |
		    FUSB_SW0_VCONN_CC1 | FUSB_SW0_VCONN_CC2, 0);
		(void)fusb302_update_reg(sc, FUSB_REG_CONTROL2,
		    FUSB_CTL2_TOGGLE, 0);
		sc->role_discovery_tries++;
		FUSB302_DPRINTF(sc,
		    "role-discovery: detach + re-toggle (attempt %d/%d)\n",
		    sc->role_discovery_tries, ROLE_DISCOVERY_MAX_TOGGLES);
		sc->sub_state = 1;
		fusb302_start_state_timer(sc, 300);
		break;
	case 1:
		if ((evt & FUSB_EVT_TIMER_STATE) == 0)
			break;
		/*
		 * Re-enable DRP TOGGLE.  Chip will run another toggle cycle
		 * and fire TOGDONE when it settles.  fusb302_irq_task routes
		 * that through the normal attach path: if SRC, we run SRC
		 * bring-up; if SNK, snk_startup retries and on its giveup
		 * comes back here for another attempt.
		 */
		(void)fusb302_update_reg(sc, FUSB_REG_CONTROL2,
		    FUSB_CTL2_MODE_MASK | FUSB_CTL2_TOG_RD_ONLY |
		    FUSB_CTL2_TOGGLE,
		    FUSB_CTL2_MODE_DRP | FUSB_CTL2_TOG_RD_ONLY |
		    FUSB_CTL2_TOGGLE);
		FUSB302_DPRINTF(sc,
		    "role-discovery: TOGGLE re-enabled in DRP mode, awaiting "
		    "TOGDONE\n");
		sc->conn_state = FUSB_ST_UNATTACHED;
		break;
	}

	/*
	 * If we've exhausted attempts (next snk_startup giveup will
	 * see this), the SNK path falls through to DISABLED via the
	 * role_discovery_complete latch.
	 */
	if (sc->role_discovery_tries >= ROLE_DISCOVERY_MAX_TOGGLES)
		sc->role_discovery_complete = true;
}

static void
fusb302_state_snk_evaluate_caps_locked(struct fusb302_softc *sc,
    uint32_t evt __unused)
{
	uint32_t pdo;
	int i, n, pos;

	sc->hardrst_count = 0;
	pos = 0;

	/*
	 * Walk the partner's source caps and pick the highest-numbered
	 * Fixed PDO at <=5V (50mV units => 100). Per spec, position 1 is
	 * always Vsafe5V Fixed, so we always have a valid choice.
	 */
	n = 0;
	for (i = 0; i < 7; i++) {
		if (sc->partner_cap[i] == 0)
			break;
		n++;
	}
	for (i = 0; i < n; i++) {
		pdo = sc->partner_cap[i];
		if (((pdo >> 30) & 3) == 0) {
			uint32_t mv50 = (pdo >> 10) & 0x3ff;
			if (mv50 <= 100)
				pos = i + 1;
		}
	}
	if (pos == 0)
		pos = 1;
	sc->pos_power = pos;
	FUSB302_DPRINTF(sc, "snk: selecting PDO position %d (PDO=0x%08x)\n",
	    pos, sc->partner_cap[pos - 1]);
	fusb302_set_state_locked(sc, FUSB_ST_SNK_SELECT_CAP);
}

static void
fusb302_state_snk_select_cap_locked(struct fusb302_softc *sc, uint32_t evt)
{
	int tmp;

	switch (sc->sub_state) {
	case 0:
		fusb302_set_mesg_request_locked(sc);
		FUSB302_DPRINTF(sc, "snk: send REQUEST RDO=0x%08x\n",
		    sc->send_load[0]);
		sc->tx_state = FUSB_TX_IDLE;
		sc->sub_state++;
		/* FALLTHROUGH */
	case 1:
		tmp = fusb302_policy_send_data_locked(sc);
		if (tmp == FUSB_TX_SUCCESS) {
			fusb302_start_state_timer(sc, T_SENDER_RESPONSE);
			sc->sub_state++;
		} else if (tmp == FUSB_TX_FAILED) {
			device_printf(sc->dev, "snk: REQUEST tx failed\n");
			fusb302_set_state_locked(sc, FUSB_ST_SNK_DISCOVERY);
		}
		break;
	default:
		if (evt & FUSB_EVT_RX) {
			if (PD_IS_CTRL(sc->rec_head, PD_CMT_ACCEPT)) {
				FUSB302_DPRINTF(sc, "snk: REQUEST accepted\n");
				fusb302_start_state_timer(sc, T_PS_TRANSITION);
				fusb302_set_state_locked(sc,
				    FUSB_ST_SNK_TRANSITION_SINK);
			} else if (PD_IS_CTRL(sc->rec_head, PD_CMT_REJECT) ||
			    PD_IS_CTRL(sc->rec_head, PD_CMT_WAIT)) {
				FUSB302_DPRINTF(sc,
				    "snk: REQUEST rejected/wait, staying in 5V default\n");
				fusb302_set_state_locked(sc, FUSB_ST_SNK_READY);
			}
		} else if (evt & FUSB_EVT_TIMER_STATE) {
			device_printf(sc->dev, "snk: SenderResponse timeout\n");
			fusb302_set_state_locked(sc, FUSB_ST_SNK_DISCOVERY);
		}
		break;
	}
}

static void
fusb302_state_snk_transition_sink_locked(struct fusb302_softc *sc,
    uint32_t evt)
{
	if (sc->sub_state == 0) {
		/* Timer was started by snk_select_cap on Accept */
		sc->sub_state = 1;
	}
	if (evt & FUSB_EVT_RX) {
		if (PD_IS_CTRL(sc->rec_head, PD_CMT_PS_RDY)) {
			sc->notify_is_pd = true;
			FUSB302_DPRINTF(sc,
			    "PD connected as UFP (5V contract)\n");
			fusb302_set_state_locked(sc, FUSB_ST_SNK_READY);
			return;
		}
		if (PD_IS_DATA(sc->rec_head, PD_DMT_SRCCAP)) {
			fusb302_set_state_locked(sc,
			    FUSB_ST_SNK_EVALUATE_CAPS);
			return;
		}
	}
	if (evt & FUSB_EVT_TIMER_STATE) {
		FUSB302_DPRINTF(sc, "snk: PS_RDY timeout\n");
		fusb302_set_state_locked(sc, FUSB_ST_SNK_TRANSITION_DEFAULT);
	}
}

static void
fusb302_state_snk_ready_locked(struct fusb302_softc *sc, uint32_t evt)
{
	/*
	 * Minimal ready state. VDM/Alt-Mode dispatch added by the next task.
	 * Re-evaluate caps if a new src_caps arrives.
	 */
	if (evt & FUSB_EVT_RX) {
		if (PD_IS_DATA(sc->rec_head, PD_DMT_SRCCAP)) {
			fusb302_set_state_locked(sc,
			    FUSB_ST_SNK_EVALUATE_CAPS);
			return;
		}
		FUSB302_DPRINTF(sc,
		    "snk_ready: RX head=0x%04x type=%d cnt=%d\n",
		    sc->rec_head, PD_HDR_TYPE(sc->rec_head),
		    PD_HDR_CNT(sc->rec_head));
	}
}

static void
fusb302_state_snk_transition_default_locked(struct fusb302_softc *sc,
    uint32_t evt)
{
	switch (sc->sub_state) {
	case 0:
		sc->notify_is_pd = false;
		/*
		 * Electrical detach: drop all CC pulls/MEAS for ~200ms so the
		 * partner sees a real detach edge (tCCDisconnect=25ms min;
		 * many real partners need 100-150ms to re-arm their PD stack).
		 * This is the in-driver equivalent of unplugging the cable.
		 */
		(void)fusb302_update_reg(sc, FUSB_REG_SWITCHES0,
		    FUSB_SW0_PDWN1 | FUSB_SW0_PDWN2 |
		    FUSB_SW0_PU_EN1 | FUSB_SW0_PU_EN2 |
		    FUSB_SW0_MEAS_CC1 | FUSB_SW0_MEAS_CC2 |
		    FUSB_SW0_VCONN_CC1 | FUSB_SW0_VCONN_CC2, 0);
		fusb302_start_state_timer(sc, 200);
		sc->sub_state++;
		break;
	case 1:
		if (evt & FUSB_EVT_TIMER_STATE) {
			/* Reapply Rd on active CC; finish T_SRC_RECOVER. */
			sc->attached_as_sink = true;
			fusb302_set_polarity_locked(sc, sc->cc_polarity);
			fusb302_start_state_timer(sc, 630);
			sc->sub_state++;
		}
		break;
	default:
		if (evt & FUSB_EVT_TIMER_STATE)
			fusb302_set_state_locked(sc, FUSB_ST_SNK_STARTUP);
		break;
	}
}

/* Translate role_pref into CONTROL2 mode/toggle bits. */
static uint8_t
fusb302_toggle_ctl2_locked(struct fusb302_softc *sc)
{
	uint8_t v = FUSB_CTL2_TOGGLE;

	switch (sc->role_pref) {
	case FUSB_ROLE_SRC:
		v |= FUSB_CTL2_MODE_DFP | FUSB_CTL2_TOG_RD_ONLY;
		break;
	case FUSB_ROLE_SNK:
		v |= FUSB_CTL2_MODE_UFP;
		break;
	case FUSB_ROLE_DRP:
	default:
		/*
		 * TOG_RD_ONLY tells the chip's toggle state machine to only
		 * stop when it sees Rd (i.e., a partner SNK).  Without it,
		 * the chip can settle on either Rd or Rp, and on RockPro64
		 * the partner's DRP toggle phase consistently locked us into
		 * SNK_CC2.  Linux's i2c-rk3x sets TOG_RD_ONLY for DRP for
		 * the same reason -- bias the chip toward landing as SRC,
		 * which is the working configuration on this hardware.
		 */
		v |= FUSB_CTL2_MODE_DRP | FUSB_CTL2_TOG_RD_ONLY;
		break;
	}
	return (v);
}

/* Detach: re-init CC toggle and reset everything */
static void
fusb302_set_state_unattached_locked(struct fusb302_softc *sc)
{
	device_printf(sc->dev, "detached, restarting CC detection\n");

	callout_stop(&sc->timer_state);

	sc->notify_is_cc = false;
	sc->notify_is_pd = false;
	sc->notify_is_enter_mode = false;
	sc->is_pd_support = false;
	sc->attached_as_sink = false;
	sc->role_discovery_complete = false;
	sc->role_discovery_tries = 0;
	memset(&sc->dp_altmode, 0, sizeof(sc->dp_altmode));
	memset(sc->partner_cap, 0, sizeof(sc->partner_cap));

	/* Disable our VBUS regulator so the partner-sources-VBUS detection
	 * in ATTACHED_SRC is clean on next attach. */
	if (sc->vbus_supply != NULL && sc->vbus_enabled) {
		(void)regulator_disable(sc->vbus_supply);
		sc->vbus_enabled = false;
	}

	(void)fusb302_write_reg(sc, FUSB_REG_RESET, FUSB_RESET_SW_RES);
	DELAY(1000);
	(void)fusb302_write_reg(sc, FUSB_REG_POWER, FUSB_POWER_ALL);
	/* Keep INT_N masked until interrupt masks are written (SW_RES resets them). */
	(void)fusb302_write_reg(sc, FUSB_REG_CONTROL0,
	    FUSB_CTL0_HOST_CUR_DEF | FUSB_CTL0_INT_MASK);
	(void)fusb302_write_reg(sc, FUSB_REG_MASK1, FUSB_MASK1_PD);
	(void)fusb302_write_reg(sc, FUSB_REG_MASKA, FUSB_MASKA_PD);
	(void)fusb302_write_reg(sc, FUSB_REG_MASKB, FUSB_MASKB_PD);
	/* Clear any stale flags then re-enable INT_N output. */
	{ uint8_t _t; fusb302_read_reg(sc, FUSB_REG_INTERRUPT, &_t);
	  fusb302_read_reg(sc, FUSB_REG_INTERRUPTA, &_t);
	  fusb302_read_reg(sc, FUSB_REG_INTERRUPTB, &_t); }
	(void)fusb302_update_reg(sc, FUSB_REG_CONTROL0, FUSB_CTL0_INT_MASK, 0);

	(void)fusb302_write_reg(sc, FUSB_REG_CONTROL2,
	    fusb302_toggle_ctl2_locked(sc));

	sc->conn_state = FUSB_ST_UNATTACHED;
	sc->work_continue = 0;
}

/* -----------------------------------------------------------------------
 * Hardware alert → event bits
 *
 * Two parallel out-params: evtp gets the legacy FUSB_EVT_* bits the
 * embedded SM consumes; pd_evtp gets USBC_PD_E_* bits delivered to the
 * policy SM after sc->mtx is dropped.
 * ----------------------------------------------------------------------- */
static void
fusb302_tcpc_alert_locked(struct fusb302_softc *sc, uint32_t *evtp,
    uint32_t *pd_evtp)
{
	uint8_t intr, intra, intrb, status1a;
	int error;

	error = fusb302_read_reg(sc, FUSB_REG_INTERRUPT, &intr);
	if (error != 0)
		return;
	error = fusb302_read_reg(sc, FUSB_REG_INTERRUPTA, &intra);
	if (error != 0)
		return;
	error = fusb302_read_reg(sc, FUSB_REG_INTERRUPTB, &intrb);
	if (error != 0)
		return;

	/* Verbose alert trace — useful for VDM bring-up debug only. */
	if (bootverbose && (intr | intra | intrb))
		FUSB302_DPRINTF(sc,
		    "alert: intr=%02x intra=%02x intrb=%02x\n",
		    intr, intra, intrb);

	/* Refresh legacy status for sysctl */
	fusb302_read_reg(sc, FUSB_REG_STATUS0, &sc->status0);
	fusb302_read_reg(sc, FUSB_REG_STATUS1, &sc->status1);
	fusb302_read_reg(sc, FUSB_REG_STATUS0A, &sc->status0a);
	fusb302_read_reg(sc, FUSB_REG_STATUS1A, &sc->status1a);

	if (intra & FUSB_INTRA_TOGDONE) {
		*evtp |= FUSB_EVT_CC;
		*pd_evtp |= USBC_PD_E_CC_CHANGE;
		status1a = sc->status1a;
		/* Stop toggle; PD code takes manual control of CC */
		(void)fusb302_update_reg(sc, FUSB_REG_CONTROL2,
		    FUSB_CTL2_TOGGLE, 0);
		FUSB302_DPRINTF(sc, "TOGDONE status1a=0x%02x\n", status1a);
	}

	if (intr & 0x80 /* VBUSOK */) {
		*pd_evtp |= USBC_PD_E_VBUS_CHANGE;
		if (sc->notify_is_cc)
			*evtp |= FUSB_EVT_CC;
	}

	if (intra & FUSB_INTRA_TXSENT) {
		*evtp |= FUSB_EVT_TX;
		*pd_evtp |= USBC_PD_E_TX_OK;
		sc->tx_state = FUSB_TX_SUCCESS;
	}

	if (intra & FUSB_INTRA_RETRYFAIL) {
		*evtp |= FUSB_EVT_TX;
		*pd_evtp |= USBC_PD_E_TX_FAIL;
		sc->tx_state = FUSB_TX_FAILED;
	}

	if (intrb & FUSB_INTRB_GCRCSENT) {
		*evtp |= FUSB_EVT_RX;
		*pd_evtp |= USBC_PD_E_RX;
	}

	if (intra & FUSB_INTRA_HARDRST) {
		FUSB302_DPRINTF(sc, "hard reset received\n");
		fusb302_pd_reset_locked(sc);
		sc->msg_id = 0;
		sc->vdm_state = VDM_DISC_ID_ST;
		/*
		 * Partner-initiated reset means partner just woke up its PD
		 * stack — give the discovery loop a fresh budget of retries
		 * even if we previously gave up.
		 */
		sc->hardrst_count = 0;
		fusb302_set_state_locked(sc, sc->attached_as_sink ?
		    FUSB_ST_SNK_TRANSITION_DEFAULT :
		    FUSB_ST_SRC_TRANSITION_DEFAULT);
		*evtp |= FUSB_EVT_REC_RESET;
		*pd_evtp |= USBC_PD_E_HARD_RESET_RX;
	}

	if (intra & FUSB_INTRA_HARDSENT) {
		/* Hard reset transmitted; transition to default state */
		fusb302_pd_reset_locked(sc);
		fusb302_set_state_locked(sc, sc->attached_as_sink ?
		    FUSB_ST_SNK_TRANSITION_DEFAULT :
		    FUSB_ST_SRC_TRANSITION_DEFAULT);
		*pd_evtp |= USBC_PD_E_HARD_RESET_TX_OK;
	}
}

/* -----------------------------------------------------------------------
 * State machine dispatch
 * ----------------------------------------------------------------------- */
static void
fusb302_run_state_locked(struct fusb302_softc *sc, uint32_t evt)
{
	switch (sc->conn_state) {
	case FUSB_ST_DISABLED: {
		uint8_t _t;
		/*
		 * Silence INT_N completely: mask all chip-side sources,
		 * drain any stale flags, then disable INT_N output. CC
		 * pulls are kept intact so the partner continues to see
		 * us as electrically attached — required for the no-PD
		 * DP-Alt-Mode bypass path where cdn_dp tries AUX via SBU.
		 *
		 * Detach detection in DISABLED is currently a known gap:
		 * VBUSOK on this RockPro64 stays asserted regardless of
		 * cable state (board-level signal — VBUS sense appears
		 * to be tied to a 5V rail downstream of the connector),
		 * so we can't trust it as an unplug edge. A polling
		 * callout would be the right fix; deferred until the
		 * real DFP+SRC PD path lands.
		 */
		(void)fusb302_write_reg(sc, FUSB_REG_MASK1, 0xff);
		(void)fusb302_write_reg(sc, FUSB_REG_MASKA, 0xff);
		(void)fusb302_write_reg(sc, FUSB_REG_MASKB, 0xff);
		(void)fusb302_read_reg(sc, FUSB_REG_INTERRUPT, &_t);
		(void)fusb302_read_reg(sc, FUSB_REG_INTERRUPTA, &_t);
		(void)fusb302_read_reg(sc, FUSB_REG_INTERRUPTB, &_t);
		(void)fusb302_update_reg(sc, FUSB_REG_CONTROL0,
		    FUSB_CTL0_INT_MASK, FUSB_CTL0_INT_MASK);
		break;
	}
	case FUSB_ST_ERROR_RECOVERY:
		fusb302_set_state_unattached_locked(sc);
		break;
	case FUSB_ST_UNATTACHED:
		/* Waiting for TOGDONE (EVENT_CC) */
		if (evt & FUSB_EVT_CC) {
			uint8_t ts = (sc->status1a & FUSB_ST1A_TOGSS_MASK)
			    >> FUSB_ST1A_TOGSS_SHIFT;
			switch (ts) {
			case FUSB_TOGSS_SRC_CC1:
			case FUSB_TOGSS_SRC_CC2:
				sc->attached_as_sink = false;
				fusb302_set_state_locked(sc,
				    FUSB_ST_ATTACHED_SRC);
				break;
			case FUSB_TOGSS_SNK_CC1:
			case FUSB_TOGSS_SNK_CC2:
				sc->attached_as_sink = true;
				fusb302_set_state_locked(sc,
				    FUSB_ST_ATTACHED_SNK);
				break;
			default:
				break;
			}
		}
		break;
	case FUSB_ST_ATTACHED_SRC:
		fusb302_state_attached_source_locked(sc, evt);
		break;
	case FUSB_ST_SRC_STARTUP:
		fusb302_state_src_startup_locked(sc, evt);
		break;
	case FUSB_ST_SRC_SEND_CAPS:
		fusb302_state_src_send_caps_locked(sc, evt);
		if (sc->conn_state != FUSB_ST_SRC_NEGOTIATE_CAP)
			break;
		/* FALLTHROUGH */
	case FUSB_ST_SRC_NEGOTIATE_CAP:
		fusb302_state_src_negotiate_cap_locked(sc, evt);
		/* FALLTHROUGH */
	case FUSB_ST_SRC_TRANSITION_SUPPLY:
		fusb302_state_src_transition_supply_locked(sc, evt);
		break;
	case FUSB_ST_SRC_CAP_RESPONSE:
		/* reject — hard reset */
		fusb302_set_state_locked(sc, FUSB_ST_SRC_SEND_HARDRST);
		break;
	case FUSB_ST_SRC_TRANSITION_DEFAULT:
		fusb302_state_src_transition_default_locked(sc, evt);
		break;
	case FUSB_ST_SRC_READY:
		fusb302_state_src_ready_locked(sc, evt);
		break;
	case FUSB_ST_SRC_GET_SINK_CAPS:
		fusb302_state_src_get_sink_caps_locked(sc, evt);
		break;
	case FUSB_ST_SRC_SEND_HARDRST:
		fusb302_state_src_send_hardrst_locked(sc, evt);
		break;
	case FUSB_ST_SRC_SEND_SOFTRST:
		fusb302_state_src_send_softrst_locked(sc, evt);
		break;
	case FUSB_ST_SRC_SOFTRST:
		fusb302_state_src_softrst_locked(sc);
		break;
	case FUSB_ST_SRC_DISCOVERY:
		/* simple retry: just go back to send caps */
		fusb302_set_state_locked(sc, FUSB_ST_SRC_SEND_CAPS);
		break;
	case FUSB_ST_ATTACHED_SNK:
		fusb302_state_attached_sink_locked(sc, evt);
		break;
	case FUSB_ST_SNK_STARTUP:
		fusb302_state_snk_startup_locked(sc, evt);
		break;
	case FUSB_ST_SNK_DISCOVERY:
		fusb302_state_snk_discovery_locked(sc, evt);
		break;
	case FUSB_ST_SNK_EVALUATE_CAPS:
		fusb302_state_snk_evaluate_caps_locked(sc, evt);
		break;
	case FUSB_ST_SNK_SELECT_CAP:
		fusb302_state_snk_select_cap_locked(sc, evt);
		break;
	case FUSB_ST_SNK_TRANSITION_SINK:
		fusb302_state_snk_transition_sink_locked(sc, evt);
		break;
	case FUSB_ST_SNK_READY:
		fusb302_state_snk_ready_locked(sc, evt);
		break;
	case FUSB_ST_SNK_TRANSITION_DEFAULT:
		fusb302_state_snk_transition_default_locked(sc, evt);
		break;
	case FUSB_ST_SNK_SEND_HARDRST:
		fusb302_state_snk_send_hardrst_locked(sc, evt);
		break;
	case FUSB_ST_SNK_SEND_SOFTRST:
		fusb302_state_snk_send_softrst_locked(sc, evt);
		break;
	case FUSB_ST_ROLE_DISCOVERY_SRC:
		fusb302_state_role_discovery_src_locked(sc, evt);
		break;
	default:
		break;
	}
}

/* -----------------------------------------------------------------------
 * Timer callbacks
 * ----------------------------------------------------------------------- */
static void
fusb302_timer_state_cb(void *arg)
{
	struct fusb302_softc *sc;

	sc = arg;
	atomic_set_int(&sc->pending_events, FUSB_EVT_TIMER_STATE);
	taskqueue_enqueue(taskqueue_thread, &sc->irq_task);
}


/* -----------------------------------------------------------------------
 * IRQ work body (called from both ithread and taskqueue contexts)
 *
 * sc->mtx serializes the two callers so the body itself need not care
 * which path invoked it. Hardware-IRQ wakeups arrive via fusb302_irq_ithread
 * with the GPIO source masked by INTRNG; software wakeups (timers, retry
 * re-arming) arrive via fusb302_irq_task on the system taskqueue.
 * ----------------------------------------------------------------------- */
static void
fusb302_do_work(struct fusb302_softc *sc)
{
	uint32_t evt, pd_evt;

	sx_xlock(&sc->sx);
	if (!sc->initialized) {
		sx_xunlock(&sc->sx);
		return;
	}

	/* Collect events: hardware alerts + timer expirations */
	evt = 0;
	pd_evt = 0;
	fusb302_tcpc_alert_locked(sc, &evt, &pd_evt);
	evt |= atomic_swap_int(&sc->pending_events, 0);
	evt |= sc->work_continue;
	sc->work_continue = 0;

	if (evt == 0 && pd_evt == 0) {
		sx_xunlock(&sc->sx);
		return;
	}

	/* Detach detection: source watches COMP, sink watches VBUSOK */
	if (sc->notify_is_cc && (evt & FUSB_EVT_CC)) {
		uint8_t st0;
		fusb302_read_reg(sc, FUSB_REG_STATUS0, &st0);
		if (sc->attached_as_sink) {
			if ((st0 & FUSB_ST0_VBUSOK) == 0) {
				device_printf(sc->dev,
				    "VBUS lost, detaching (sink)\n");
				fusb302_set_state_unattached_locked(sc);
				goto out;
			}
		} else if (st0 & 0x20 /* COMP bit */) {
			device_printf(sc->dev, "CC open, detaching\n");
			fusb302_set_state_unattached_locked(sc);
			goto out;
		}
	}

	/* Process received PD message */
	if (evt & FUSB_EVT_RX) {
		if (fusb302_fifo_read_locked(sc) == 0) {
			if (PD_IS_CTRL(sc->rec_head, PD_CMT_SOFTRESET))
				fusb302_set_state_locked(sc,
				    sc->attached_as_sink ?
				    FUSB_ST_SNK_STARTUP :
				    FUSB_ST_SRC_SOFTRST);
		}
	}

	/* Increment message ID on successful TX */
	if ((evt & FUSB_EVT_TX) && sc->tx_state == FUSB_TX_SUCCESS)
		sc->msg_id = (sc->msg_id + 1) & 0x7;

	/* Run state machine */
	fusb302_run_state_locked(sc, evt);

	/* Re-enqueue if more work is pending */
	if (sc->work_continue)
		taskqueue_enqueue(taskqueue_thread, &sc->irq_task);

out:
	sx_xunlock(&sc->sx);

	/*
	 * Deliver accumulated events to the policy SM.  Done outside
	 * sc->mtx because the policy takes its own lock and may call
	 * back into TCPC ops (which re-acquire sc->mtx).  Each named
	 * USBC_PD_E_* bit gets its own event() call to keep the API
	 * usage straightforward; the policy folds them internally.
	 */
	if (pd_evt != 0 && sc->policy != NULL) {
		uint32_t bits = pd_evt;
		while (bits != 0) {
			uint32_t b = bits & -bits;
			usbc_pd_policy_event(sc->policy,
			    (enum usbc_pd_event)b);
			bits &= ~b;
		}
	}
}

/* Hardware IRQ path: just enqueue the taskqueue task and return. ALL real
 * work (including i2c which sleeps) runs in the taskqueue thread, NEVER in
 * the ithread. This avoids the witness panic that fired when ithread was
 * doing i2c (sleeping) while another thread tried to acquire sc->mtx:
 *
 *   panic: sleeping thread holds fusb3020
 *   fusb302_do_work -> iicdev_readfrom -> _sleep (with sc->mtx held)
 *   another thread: __mtx_lock_sleep on sc->mtx, propagate_priority panics
 *
 * INTRNG masks the GPIO source for the duration of this ithread call
 * (which is microseconds). After we return, INTRNG unmasks. If the chip's
 * INT_N is still asserted (taskqueue hasn't run yet), GIC re-fires this
 * ithread, which re-enqueues — taskqueue_enqueue is idempotent for queued
 * tasks, so no harm. The taskqueue eventually runs do_work which drains the
 * chip's INTERRUPT regs and INT_N goes high.
 */
static void
fusb302_irq_ithread(void *context)
{
	struct fusb302_softc *sc = context;

	/*
	 * Run the work synchronously in the ithread.  Reading the chip's
	 * INTERRUPT/A/B registers (in fusb302_do_work -> tcpc_alert_locked)
	 * deasserts INT_N.  INTRNG only calls pic_post_ithread *after* this
	 * function returns, so the GPIO source stays masked until INT_N is
	 * high — no storm.  Safe now because sc->sx is sx(9), which can be
	 * held across the i2c sleep.  Concurrent timer-driven callers go
	 * through fusb302_irq_task and contend on sc->sx by sleeping, which
	 * sx tolerates (the old MTX_DEF would propagate_priority panic).
	 */
	fusb302_do_work(sc);
}

/* Single-threaded work body. Holds sc->mtx during i2c (MTX_DEF tolerates
 * this when only ONE thread takes the mutex; the witness panic only fires
 * when a second thread contends on it). Timers and IRQ both route here. */
static void
fusb302_irq_task(void *context, int pending __unused)
{
	fusb302_do_work(context);
}

static int
fusb302_intr(void *context __unused)
{
	/*
	 * FUSB302 INT_N is level-low. Returning FILTER_SCHEDULE_THREAD makes
	 * INTRNG mask the GPIO source until the ithread completes and the
	 * chip-side INTERRUPT regs have been read (which deasserts INT_N).
	 *
	 * Earlier design used FILTER_HANDLED + manual bus_suspend_intr +
	 * taskqueue. bus_suspend_intr is a no-op for GPIO-routed IRQs on
	 * INTRNG, so the line stormed at ~210 kHz and starved the task
	 * entirely (verified via fbt:fusb302:fusb302_intr dtrace count vs.
	 * fusb302_irq_task count).
	 */
	return (FILTER_SCHEDULE_THREAD);
}

/* -----------------------------------------------------------------------
 * Initialization
 * ----------------------------------------------------------------------- */
static int
fusb302_init(struct fusb302_softc *sc)
{
	uint8_t intr, intra, intrb;
	int error;

	error = fusb302_write_reg(sc, FUSB_REG_RESET, FUSB_RESET_SW_RES);
	if (error != 0)
		return (error);
	DELAY(1000);

	/*
	 * Do NOT enable our VBUS regulator here. We don't yet know whether the
	 * partner is a Source (will provide VBUS to us) or a Sink (we'd need
	 * to provide VBUS). Enable the regulator only after ATTACHED_SRC is
	 * confirmed below.
	 */

	/*
	 * Reset the PD logic separately after the global SW reset.  Linux 4.4
	 * does both -- the PD_RESET re-initializes the BMC encoder/decoder
	 * state machines beyond what SW_RES alone touches.  Without this,
	 * Source_Capabilities goes out the wire but the partner never replies
	 * with GoodCRC; observed on RockPro64 + USB-C displays where Armbian
	 * gets PD-3.0 contract on the same hardware.
	 */
	error = fusb302_write_reg(sc, FUSB_REG_RESET, FUSB_RESET_PD_RESET);
	if (error != 0)
		return (error);

	/* Keep INT_N masked (INT_MASK=1) until interrupt masks are written. */
	error = fusb302_write_reg(sc, FUSB_REG_CONTROL0,
	    FUSB_CTL0_HOST_CUR_DEF | FUSB_CTL0_INT_MASK);
	if (error != 0)
		return (error);

	/* Enable auto-retry with 3 retries for PD TX reliability */
	error = fusb302_update_reg(sc, FUSB_REG_CONTROL3,
	    FUSB_CTL3_AUTO_RETRY | FUSB_CTL3_N_RETRIES,
	    FUSB_CTL3_AUTO_RETRY | FUSB_CTL3_N_RETRIES);
	if (error != 0)
		return (error);

	/*
	 * Program the BMC slicer threshold.  Default 0x20 matches the
	 * chip reset value; lower values increase RX sensitivity for
	 * marginal cables.  Top two bits 01 keep the 1-bit hysteresis
	 * the chip ships with.
	 */
	if (sc->slice_sdac >= 0 && sc->slice_sdac <= 0x3f)
		(void)fusb302_write_reg(sc, FUSB_REG_SLICE,
		    0x40u | (uint8_t)(sc->slice_sdac & 0x3fu));

	/* Set interrupt masks for full PD operation */
	error = fusb302_write_reg(sc, FUSB_REG_MASK1, FUSB_MASK1_PD);
	if (error != 0)
		return (error);
	error = fusb302_write_reg(sc, FUSB_REG_MASKA, FUSB_MASKA_PD);
	if (error != 0)
		return (error);
	error = fusb302_write_reg(sc, FUSB_REG_MASKB, FUSB_MASKB_PD);
	if (error != 0)
		return (error);

	/*
	 * Power-on internal blocks LAST, after masks + control bits are
	 * programmed.  Linux 4.4 does it in this order; powering on with
	 * default masks (everything masked) lets internal state machines
	 * run briefly with our events suppressed and could leave some
	 * sub-blocks (BMC encoder?) in a stale state.
	 */
	error = fusb302_write_reg(sc, FUSB_REG_POWER, FUSB_POWER_ALL);
	if (error != 0)
		return (error);

	/*
	 * Clear SWITCHES0 explicitly to 0 BEFORE the chip starts toggling.
	 * Linux's regmap_reg_write trace on Armbian shows this is the FIRST
	 * write to SWITCHES0 after power-on -- the chip's POR default has
	 * PDWN1|PDWN2 set (0x03), and leaving them set during DRP toggle
	 * presents Rd to the partner from the start, biasing the toggle
	 * arbitration toward landing as SNK.  Clearing them lets MODE_DRP
	 * + TOG_RD_ONLY do its job (settle on Rd values = SRC role).
	 */
	error = fusb302_write_reg(sc, FUSB_REG_SWITCHES0, 0);
	if (error != 0)
		return (error);

	/*
	 * Program the MDAC comparator threshold for USB-default Rp current.
	 * Linux's cc_meas_high = 0x26 -> ~1.596 V, derived from the table
	 * in FUSB302B datasheet (Table 22) as the Rd-Connect detection
	 * threshold when HOST_CUR = USB-default.  Without this the chip
	 * runs with its POR MDAC = 0x31 (~2.058 V) which sits ABOVE any
	 * realistic CC voltage during attach -- so COMP never trips and
	 * the toggle state machine has bad signal to pick from.
	 */
	(void)fusb302_write_reg(sc, FUSB_REG_MEASURE, 0x26);

	/* Clear any stale interrupts */
	fusb302_read_reg(sc, FUSB_REG_INTERRUPT, &intr);
	fusb302_read_reg(sc, FUSB_REG_INTERRUPTA, &intra);
	fusb302_read_reg(sc, FUSB_REG_INTERRUPTB, &intrb);

	/*
	 * Leave INT_N output masked (CONTROL0.INT_MASK=1, set above) until
	 * the parent has called bus_setup_intr in attach. Enabling INT_N
	 * here would let the chip pull the GPIO low while no handler is
	 * registered, producing the "gpio1: Interrupt pin=2 unhandled"
	 * storm. fusb302_attach clears INT_MASK as the final attach step.
	 */

	/* Start CC toggle per role preference (DRP default for any-monitor) */
	error = fusb302_write_reg(sc, FUSB_REG_CONTROL2,
	    fusb302_toggle_ctl2_locked(sc));
	if (error != 0)
		return (error);
	DELAY(1000);

	sc->conn_state = FUSB_ST_UNATTACHED;
	sc->initialized = true;

	/* Snapshot legacy status fields */
	fusb302_read_reg(sc, FUSB_REG_POWER,    &sc->power);
	fusb302_read_reg(sc, FUSB_REG_CONTROL2, &sc->control2);
	fusb302_read_reg(sc, FUSB_REG_STATUS0,  &sc->status0);
	fusb302_read_reg(sc, FUSB_REG_STATUS1,  &sc->status1);
	fusb302_read_reg(sc, FUSB_REG_STATUS0A, &sc->status0a);
	fusb302_read_reg(sc, FUSB_REG_STATUS1A, &sc->status1a);
	sc->state_valid = true;

	FUSB302_DPRINTF(sc,
	    "initialized: irq=0x%02x irqa=0x%02x irqb=0x%02x "
	    "st0=0x%02x st1=0x%02x st0a=0x%02x st1a=0x%02x\n",
	    intr, intra, intrb,
	    sc->status0, sc->status1, sc->status0a, sc->status1a);

	/*
	 * Hot-plug bootstrap for late-loaded modules.
	 *
	 * If fusb302, tcphy, and cdn-dp all probe during boot while the cable
	 * is already present, the normal TOGDONE/PD/VDM sequence runs from
	 * the start.  With kldload iteration that TOGDONE edge may have
	 * happened while INT_N was still masked, leaving us parked in
	 * UNATTACHED forever even though a partner is already connected.
	 *
	 * Direct CC measurement here:
	 *  - sink-only: measure Rd on both CC pins to find partner Rp
	 *  - source/DRP: measure Rp on both CC pins to find partner Rd
	 *
	 * If one side is already present, synthesize ATTACHED_* and let the
	 * existing PD/VDM state machine continue from there.
	 */
	if (sc->role_pref == FUSB_ROLE_SNK &&
	    (sc->status0 & FUSB_ST0_VBUSOK)) {
		uint8_t st, cc1_max = 0, cc2_max = 0;
		int polarity = -1, j;

		(void)fusb302_update_reg(sc, FUSB_REG_CONTROL2,
		    FUSB_CTL2_TOGGLE, 0);
		DELAY(5000);

		/* Sample CC1 over 50ms to ride past Samsung's DRP toggle */
		(void)fusb302_write_reg(sc, FUSB_REG_SWITCHES0,
		    FUSB_SW0_PDWN1 | FUSB_SW0_PDWN2 | FUSB_SW0_MEAS_CC1);
		DELAY(2000);
		for (j = 0; j < 50; j++) {
			DELAY(1000);
			if (fusb302_read_reg(sc, FUSB_REG_STATUS0,
			    &st) == 0) {
				uint8_t bc = st & FUSB_ST0_BC_LVL_MASK;
				if (bc > cc1_max)
					cc1_max = bc;
			}
		}

		/* Sample CC2 over 50ms */
		(void)fusb302_write_reg(sc, FUSB_REG_SWITCHES0,
		    FUSB_SW0_PDWN1 | FUSB_SW0_PDWN2 | FUSB_SW0_MEAS_CC2);
		DELAY(2000);
		for (j = 0; j < 50; j++) {
			DELAY(1000);
			if (fusb302_read_reg(sc, FUSB_REG_STATUS0,
			    &st) == 0) {
				uint8_t bc = st & FUSB_ST0_BC_LVL_MASK;
				if (bc > cc2_max)
					cc2_max = bc;
			}
		}

		FUSB302_DPRINTF(sc,
		    "manual probe: cc1_max_bc=%d cc2_max_bc=%d\n",
		    cc1_max, cc2_max);

		if (cc1_max > cc2_max && cc1_max != 0)
			polarity = 0;
		else if (cc2_max != 0)
			polarity = 1;

		if (polarity >= 0) {
			FUSB302_DPRINTF(sc,
			    "manual probe: VBUSOK + Rp on CC%d, "
			    "attaching as SNK\n", polarity + 1);
			sc->cc_polarity = polarity;
			sc->attached_as_sink = true;
			sc->status1a = (uint8_t)(
			    (polarity == 0 ? FUSB_TOGSS_SNK_CC1 :
			    FUSB_TOGSS_SNK_CC2) <<
			    FUSB_ST1A_TOGSS_SHIFT);
			fusb302_set_state_locked(sc,
			    FUSB_ST_ATTACHED_SNK);
		} else {
			/* No Rp on either CC — restore TOGGLE. */
			(void)fusb302_update_reg(sc, FUSB_REG_CONTROL2,
			    FUSB_CTL2_TOGGLE, FUSB_CTL2_TOGGLE);
		}
	} else if (sc->role_pref == FUSB_ROLE_SRC) {
		int cc1_rd, cc2_rd, polarity;

		cc1_rd = 0;
		cc2_rd = 0;
		polarity = -1;

		(void)fusb302_update_reg(sc, FUSB_REG_CONTROL2,
		    FUSB_CTL2_TOGGLE, 0);
		DELAY(5000);

		cc1_rd = fusb302_probe_cc_pull_up_locked(sc, 0);
		cc2_rd = fusb302_probe_cc_pull_up_locked(sc, 1);

		FUSB302_DPRINTF(sc,
		    "manual source probe: cc1_rd=%d cc2_rd=%d\n",
		    cc1_rd, cc2_rd);

		if (cc1_rd != 0 && cc2_rd == 0)
			polarity = 0;
		else if (cc2_rd != 0 && cc1_rd == 0)
			polarity = 1;
		else if (cc1_rd != 0 && cc2_rd != 0 &&
		    fusb302_is_rockpro64(sc->dev)) {
			/*
			 * Late-loaded RockPro64 source probing can report Rd
			 * on both CC pins even though only one orientation is
			 * real.  Prefer CC1 as the bootstrap polarity so the
			 * existing PD/VDM machine can start; if that proves
			 * wrong, later retries can still revisit orientation.
			 */
			FUSB302_DPRINTF(sc,
			    "manual source probe: ambiguous dual-Rd, "
			    "bootstrapping CC1 on RockPro64\n");
			polarity = 0;
		}

		if (polarity >= 0) {
			FUSB302_DPRINTF(sc,
			    "manual source probe: Rd on CC%d, "
			    "attaching as SRC/DFP\n", polarity + 1);
			sc->cc_polarity = polarity;
			sc->attached_as_sink = false;
			sc->status1a = (uint8_t)(
			    (polarity == 0 ? FUSB_TOGSS_SRC_CC1 :
			    FUSB_TOGSS_SRC_CC2) <<
			    FUSB_ST1A_TOGSS_SHIFT);
			fusb302_set_state_locked(sc, FUSB_ST_ATTACHED_SRC);
		} else {
			(void)fusb302_update_reg(sc, FUSB_REG_CONTROL2,
			    FUSB_CTL2_TOGGLE, FUSB_CTL2_TOGGLE);
		}
	}

	return (0);
}

/* -----------------------------------------------------------------------
 * Public APIs
 * ----------------------------------------------------------------------- */
int
fusb302_get_typec_status(device_t dev, struct fusb302_typec_status *status)
{
	struct fusb302_softc *sc;

	if (dev == NULL || status == NULL)
		return (EINVAL);

	sc = device_get_softc(dev);
	/*
	 * Lock-free snapshot. Acquiring sc->mtx here was a panic-source:
	 * the irq_task holds sc->mtx across I2C reads (which sleep), so any
	 * caller that takes the mutex while irq_task is mid-I2C trips
	 * propagate_priority's "sleeping thread holds non-sleepable lock"
	 * check. Caller (rk_typec_phy_enable / cdn_dp) is happy with a
	 * slightly stale snapshot — the fields we read here are word-sized
	 * scalars updated atomically by the irq task, and a momentary
	 * inconsistency between them is harmless for orientation/flip
	 * derivation.
	 */
	status->attached = sc->notify_is_cc;
	status->vbusok = (sc->status0 & FUSB_ST0_VBUSOK) != 0;
	status->has_irq = (sc->irq_res != NULL);
	status->state_valid = sc->state_valid;
	status->togss_raw = (sc->status1a & FUSB_ST1A_TOGSS_MASK) >>
	    FUSB_ST1A_TOGSS_SHIFT;
	status->orientation = (sc->cc_polarity == 0) ?
	    FUSB302_TYPEC_ORIENT_CC1 : FUSB302_TYPEC_ORIENT_CC2;
	status->role = sc->notify_is_cc ?
	    FUSB302_TYPEC_ROLE_SOURCE : FUSB302_TYPEC_ROLE_NONE;

	return (sc->state_valid ? 0 : ENXIO);
}

/*
 * Cross-module entry point consumed by rk_cdn_dp(4) (declared in
 * fusb302_var.h, dependency expressed via MODULE_DEPEND on the
 * consumer side).  Returns 0 and fills *status when valid DP Alt
 * Mode state is available.
 */
int
fusb302_get_dp_altmode_state(device_t dev,
    struct rk3399_typec_dp_altmode_status *status)
{
	struct fusb302_softc *sc;

	if (dev == NULL || status == NULL)
		return (EINVAL);

	sc = device_get_softc(dev);
	/*
	 * Lock-free snapshot, same reason as fusb302_get_typec_status:
	 * irq_task can be mid-I2C with sc->mtx held, and any caller (cdn_dp)
	 * that blocks on the mutex while we're sleeping in I2C panics via
	 * propagate_priority. The dp_altmode struct is small and a torn
	 * read between fields is harmless — caller decides on a single
	 * field at a time and re-queries periodically.
	 */
	*status = sc->dp_altmode;

	return (status->valid ? 0 : ENXIO);
}

/* -----------------------------------------------------------------------
 * Sysctl
 * ----------------------------------------------------------------------- */
static int
fusb302_sysctl_reg(SYSCTL_HANDLER_ARGS)
{
	struct fusb302_softc *sc;
	uint8_t reg, val;
	int ival, error;

	sc = arg1;
	reg = (uint8_t)arg2;

	/*
	 * Do NOT hold sc->mtx across the I2C read.  fusb302_read_reg ->
	 * iicdev_readfrom -> rk_i2c_transfer -> _sleep, and sc->mtx is
	 * MTX_DEF (non-sleepable).  If irq_task races us on sc->mtx while
	 * we're sleeping, propagate_priority panics with
	 * "sleeping thread holds fusb3020".
	 */
	error = fusb302_read_reg(sc, reg, &val);
	if (error != 0)
		return (error);

	ival = val;
	return (sysctl_handle_int(oidp, &ival, 0, req));
}

/*
 * Live-tunable BMC slicer threshold.  Reads sc->slice_sdac for the
 * current logical value; on a write, validates the range, updates
 * the field, and writes the SLICE register so the change takes
 * effect on the very next BMC frame.  No mutex needed: the chip
 * tolerates SLICE updates outside of TX/RX, and we hand off through
 * a single I2C write.
 */
static int
fusb302_sysctl_slice_sdac(SYSCTL_HANDLER_ARGS)
{
	struct fusb302_softc *sc = arg1;
	int val, error;

	val = sc->slice_sdac;
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (val < 0 || val > 0x3f)
		return (EINVAL);
	sc->slice_sdac = val;
	(void)fusb302_write_reg(sc, FUSB_REG_SLICE,
	    0x40u | (uint8_t)(val & 0x3fu));
	FUSB302_DPRINTF(sc, "BMC slice SDAC -> 0x%02x (~%d mV)\n",
	    val, val * 42);
	return (0);
}

/*
 * vbus_cycle_now: drop the USB-C VBUS regulator (SY6280AAC on RockPro64)
 * for 1.5s then re-enable, forcing the attached USB-C display through a
 * full power-cycle.  Used to recover from a sink whose DP RX has gone
 * deaf to AUX without responding to HPD pulses.
 */
static int
fusb302_sysctl_vbus_cycle(SYSCTL_HANDLER_ARGS)
{
	struct fusb302_softc *sc;
	int error, val = 0;

	sc = arg1;
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (val != 1)
		return (EINVAL);

	if (sc->vbus_supply == NULL) {
		FUSB302_DPRINTF(sc,
		    "vbus_cycle: no regulator handle, nothing to do\n");
		return (ENXIO);
	}

	FUSB302_DPRINTF(sc, "vbus_cycle: dropping VBUS\n");
	if (sc->vbus_enabled) {
		(void)regulator_disable(sc->vbus_supply);
		sc->vbus_enabled = false;
	}
	pause("vbusoff", hz * 3 / 2);	/* 1.5s */
	FUSB302_DPRINTF(sc, "vbus_cycle: restoring VBUS\n");
	error = regulator_enable(sc->vbus_supply);
	if (error == 0)
		sc->vbus_enabled = true;
	else
		FUSB302_DPRINTF(sc,
		    "vbus_cycle: regulator_enable failed (%d)\n", error);
	return (error);
}

/*
 * reattach_now: force the Type-C state machine back to UNATTACHED and
 * restart toggle/CC detection. This is the software equivalent of a
 * cable replug for role_pref/skip_pd experiments and avoids kldunload
 * while the controller is active.
 */
static int
fusb302_sysctl_reattach(SYSCTL_HANDLER_ARGS)
{
	struct fusb302_softc *sc;
	int error, val;

	sc = arg1;
	val = 0;
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (val != 1)
		return (EINVAL);

	sx_xlock(&sc->sx);
	if (!sc->initialized) {
		sx_xunlock(&sc->sx);
		return (ENXIO);
	}
	FUSB302_DPRINTF(sc,
	    "reattach_now: forcing detach/re-toggle (role_pref=%d skip_pd=%d)\n",
	    sc->role_pref, sc->skip_pd);
	fusb302_set_state_unattached_locked(sc);
	sx_xunlock(&sc->sx);

	taskqueue_enqueue(taskqueue_thread, &sc->irq_task);
	return (0);
}

static void
fusb302_add_sysctls(struct fusb302_softc *sc)
{
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *tree;

	ctx = device_get_sysctl_ctx(sc->dev);
	tree = device_get_sysctl_tree(sc->dev);

	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "debug", CTLFLAG_RW, &sc->debug, 0,
	    "Enable verbose trace prints (alerts, FIFO TX/RX, VDM walk, "
	    "PD message dumps). 0=off (default), 1=on");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "device_id",
	    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, sc,
	    FUSB_REG_DEVICE_ID, fusb302_sysctl_reg, "I", "device ID");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "status0",
	    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, sc,
	    FUSB_REG_STATUS0, fusb302_sysctl_reg, "I", "STATUS0");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "status1",
	    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, sc,
	    FUSB_REG_STATUS1, fusb302_sysctl_reg, "I", "STATUS1");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "status0a",
	    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, sc,
	    FUSB_REG_STATUS0A, fusb302_sysctl_reg, "I", "STATUS0A");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "status1a",
	    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, sc,
	    FUSB_REG_STATUS1A, fusb302_sysctl_reg, "I", "STATUS1A");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "switches0",
	    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, sc,
	    FUSB_REG_SWITCHES0, fusb302_sysctl_reg, "I", "SWITCHES0");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "switches1",
	    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, sc,
	    FUSB_REG_SWITCHES1, fusb302_sysctl_reg, "I", "SWITCHES1");
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "role_pref",
	    CTLFLAG_RW, &sc->role_pref, 0,
	    "Toggle role pref: 0=DRP 1=src-only 2=snk-only (effective on next detach)");
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "skip_pd",
	    CTLFLAG_RW, &sc->skip_pd, 0,
	    "1=DP-only partner: skip PD negotiation, synthesize Pin C 4-lane "
	    "DP altmode, notify cdn_dp directly");
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "passive_src",
	    CTLFLAG_RWTUN, &sc->passive_src, 0,
	    "1=passive SRC: present Rp on CC, never call regulator_enable "
	    "(use when partner sources VBUS but we want to advertise as Source)");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "slice_sdac",
	    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE, sc, 0,
	    fusb302_sysctl_slice_sdac, "I",
	    "BMC slicer SDAC (0..0x3f, LSB ~42 mV); writes SLICE register live");
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "pd_spec_rev",
	    CTLFLAG_RW, &sc->pd_spec_rev, 0,
	    "PD spec rev to advertise: 1=PD 2.0, 2=PD 3.0 (effective on next attach)");
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "conn_state",
	    CTLFLAG_RD, (int *)&sc->conn_state, 0,
	    "Current connection state (enum fusb302_conn_state)");
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "attached_as_sink",
	    CTLFLAG_RD, (int *)&sc->attached_as_sink, 0,
	    "1 if currently attached as UFP/sink, 0 otherwise");
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "dp_altmode_valid",
	    CTLFLAG_RD, (int *)&sc->dp_altmode.valid, 0,
	    "1 when a DisplayPort Alt Mode snapshot is available");
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "dp_altmode_ready",
	    CTLFLAG_RD, (int *)&sc->dp_altmode.dp_ready, 0,
	    "1 when DisplayPort Alt Mode is ready for the DP controller");
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "dp_altmode_usb_ss",
	    CTLFLAG_RD, &sc->dp_altmode.usb_ss, 0,
	    "Alt Mode USB_SS flag: 0=DP-only, 1=USB3+DP");
	SYSCTL_ADD_U32(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "dp_altmode_pin_assignment", CTLFLAG_RD,
	    &sc->dp_altmode.pin_assignment, 0,
	    "Alt Mode pin assignment snapshot");
	SYSCTL_ADD_U32(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "dp_altmode_status", CTLFLAG_RD, &sc->dp_altmode.dp_status, 0,
	    "Alt Mode DP status VDO snapshot");
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "pos_power",
	    CTLFLAG_RD, &sc->pos_power, 0,
	    "Selected partner-source PDO position (sink role)");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "vbus_cycle_now",
	    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE, sc, 0,
	    fusb302_sysctl_vbus_cycle, "I",
	    "Write 1 to drop VBUS for 1.5s then re-enable, forcing the attached USB-C display through a power-cycle");
	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "reattach_now",
	    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE, sc, 0,
	    fusb302_sysctl_reattach, "I",
	    "Write 1 to force detach and restart Type-C attach detection");
}

/* -----------------------------------------------------------------------
 * OFW IRQ recovery (unchanged from original)
 * ----------------------------------------------------------------------- */
static int
fusb302_try_ofw_irq(struct fusb302_softc *sc)
{
	pcell_t *cells;
	phandle_t node, producer;
	rman_res_t count, start;
	int error, irq, ncells;

	if (bus_get_resource(sc->dev, SYS_RES_IRQ, sc->irq_rid, &start,
	    &count) == 0)
		return (EEXIST);

	node = ofw_bus_get_node(sc->dev);
	if (node <= 0)
		return (ENOENT);

	cells = NULL;
	error = ofw_bus_intr_by_rid(sc->dev, node, sc->irq_rid, &producer,
	    &ncells, &cells);
	if (error != 0)
		return (error);

	irq = ofw_bus_map_intr(sc->dev, producer, ncells, cells);
	free(cells, M_OFWPROP);
	if (irq <= 0)
		return (ENOENT);

	error = bus_set_resource(sc->dev, SYS_RES_IRQ, sc->irq_rid, irq, 1);
	if (error != 0)
		return (error);

	FUSB302_DPRINTF(sc, "recovered irq %d from OFW\n", irq);
	return (0);
}

/* -----------------------------------------------------------------------
 * Driver methods
 * ----------------------------------------------------------------------- */
static int
fusb302_probe(device_t dev)
{
	uint8_t buf;

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);
	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	/*
	 * Silence the chip's INT_N output as the very first thing we do.
	 * The chip remembers CONTROL0 across kernel reboots; if a previous
	 * run cleared INT_MASK and we crash/reboot before detach silences
	 * it, the chip keeps pulling its INT_N (open-drain to gpio1) low.
	 * gpio1 then floods the console with "Interrupt pin=2 unhandled"
	 * for the entire window between rk_gpio attach and fusb302 attach
	 * registering an IRQ filter.
	 *
	 * Probe is the earliest place we have an I2C path to the chip, so
	 * write CONTROL0 = HOST_CUR_DEF | INT_MASK here.  The chip-side
	 * INT_N goes high (de-asserted) as soon as this write lands; the
	 * GPIO controller never sees a stuck pin.  Failure is harmless --
	 * if the I2C transaction fails attach will retry the write.
	 */
	buf = FUSB_CTL0_HOST_CUR_DEF | FUSB_CTL0_INT_MASK;
	(void)iicdev_writeto(dev, FUSB_REG_CONTROL0, &buf, 1, IIC_WAIT);

	device_set_desc(dev, "Fairchild FUSB302 Type-C PD controller");
	return (BUS_PROBE_DEFAULT);
}

static int
fusb302_attach(device_t dev)
{
	struct fusb302_softc *sc;
	rman_res_t irq_count, irq_start;
	int error;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->addr = iicbus_get_addr(dev);
	sc->irq_rid = 0;
	sx_init(&sc->sx, device_get_nameunit(dev));
	TASK_INIT(&sc->irq_task, 0, fusb302_irq_task, sc);
	callout_init(&sc->timer_state, 1 /* MPSAFE */);

	error = regulator_get_by_ofw_property(dev, 0, "vbus-supply",
	    &sc->vbus_supply);
	if (error != 0 && error != ENOENT) {
		device_printf(dev, "cannot get vbus-supply: %d\n", error);
		goto fail;
	}

	error = fusb302_read_reg(sc, FUSB_REG_DEVICE_ID, &sc->device_id);
	if (error != 0) {
		device_printf(dev, "cannot read device ID: %d\n", error);
		goto fail;
	}
	FUSB302_DPRINTF(sc, "device id 0x%02x at addr 0x%02x\n",
	    sc->device_id, sc->addr);

	sc->role_pref = fusb302_is_rockpro64(dev) ? FUSB_ROLE_SRC :
	    FUSB_ROLE_DRP;
	TUNABLE_INT_FETCH("hw.fusb302.role_pref", &sc->role_pref);
	if (sc->role_pref < 0 || sc->role_pref > FUSB_ROLE_SNK)
		sc->role_pref = FUSB_ROLE_DRP;
	FUSB302_DPRINTF(sc, "toggle role preference: %s\n",
	    sc->role_pref == FUSB_ROLE_SRC ? "source-only" :
	    sc->role_pref == FUSB_ROLE_SNK ? "sink-only" : "DRP");

	sc->pd_spec_rev = 2;	/* default: PD 3.0 */
	TUNABLE_INT_FETCH("hw.fusb302.pd_spec_rev", &sc->pd_spec_rev);
	if (sc->pd_spec_rev < 1 || sc->pd_spec_rev > 2)
		sc->pd_spec_rev = 2;

	sc->slice_sdac = 0x20;	/* chip default ~840 mV slice threshold */
	TUNABLE_INT_FETCH("hw.fusb302.slice_sdac", &sc->slice_sdac);
	if (sc->slice_sdac < 0 || sc->slice_sdac > 0x3f)
		sc->slice_sdac = 0x20;
	FUSB302_DPRINTF(sc, "BMC slice SDAC: 0x%02x (~%d mV)\n",
	    sc->slice_sdac, sc->slice_sdac * 42);
	FUSB302_DPRINTF(sc, "PD spec rev advertise: PD %s\n",
	    sc->pd_spec_rev == 1 ? "2.0" : "3.0");

	sc->skip_pd = 0;
	TUNABLE_INT_FETCH("hw.fusb302.skip_pd", &sc->skip_pd);
	if (sc->skip_pd)
		FUSB302_DPRINTF(sc, "skip_pd=1: passive DP defaults on SNK attach\n");

	/*
	 * Default passive_src=2: explicit AUTO_CRC + role-bit programming in
	 * ATTACHED_SRC sub_state 0 before falling through to SRC_STARTUP →
	 * SRC_SEND_CAPS.  Empirical fix for silent-PD partner: ensures
	 * SWITCHES1 (POWERROLE/DATAROLE/AUTO_CRC/SPECREV) and MASK1 are
	 * written explicitly before the first BMC TX, instead of relying on
	 * the order in which the standard SRC path writes them.
	 */
	sc->passive_src = 2;
	TUNABLE_INT_FETCH("hw.fusb302.passive_src", &sc->passive_src);
	if (sc->passive_src < 0 || sc->passive_src > 2)
		sc->passive_src = 2;

	/*
	 * Run init_locked WITHOUT taking sc->mtx. init_locked does ~100 I2C
	 * transactions (and the manual SNK probe adds 100+ more), each of
	 * which sleeps in rk_i2c_transfer. sc->mtx is MTX_DEF (non-sleepable
	 * from _sleep's perspective): holding it across an I2C sleep
	 * triggers WITNESS warnings and, if any other thread blocks waiting
	 * on it, propagate_priority panics. Attach is single-threaded
	 * relative to its own softc until bus_setup_intr connects the IRQ
	 * filter, so the lock is unnecessary here.
	 */
	error = fusb302_init(sc);
	if (error != 0) {
		device_printf(dev, "init failed: %d\n", error);
		goto fail;
	}

	/*
	 * Bind the TCPC abstraction and allocate the policy state machine.
	 * Today the embedded SM still drives the chip; the policy shadows
	 * events alongside it.  Phase 3 step 9 will strip the embedded SM
	 * and let the policy take over.
	 *
	 * Role mapping: FUSB_ROLE_SNK → sink default; SRC/DRP → source
	 * default with DRP toggling enabled when the chip's role_pref is
	 * DRP.  Caps advertise a placeholder fixed 5V@900mA in both
	 * directions; future commits will pull these from FDT.
	 */
	sc->tcpc.dev = dev;
	sc->tcpc.softc = sc;
	sc->tcpc.ops = &fusb302_tcpc_ops;
	{
		struct usbc_pd_port_caps caps;

		bzero(&caps, sizeof(caps));
		if (sc->role_pref == FUSB_ROLE_SNK) {
			caps.default_data_role = USBC_PD_DATA_UFP;
			caps.default_power_role = USBC_PD_ROLE_SINK;
		} else {
			caps.default_data_role = USBC_PD_DATA_DFP;
			caps.default_power_role = USBC_PD_ROLE_SOURCE;
		}
		caps.supports_drp = (sc->role_pref == FUSB_ROLE_DRP);
		caps.advertise_rev = (sc->pd_spec_rev == 1) ?
		    USBC_PD_REV_2_0 : USBC_PD_REV_3_0;
		caps.nr_src_pdos = 1;
		caps.src_pdos[0] = USBC_PD_PDO_TYPE_FIXED |
		    USBC_PD_PDO_FIXED_USB_COMMS |
		    USBC_PD_PDO_FIXED_VOLTAGE_MV(5000) |
		    USBC_PD_PDO_FIXED_CURRENT_MA(900);
		caps.nr_snk_pdos = 1;
		caps.snk_pdos[0] = USBC_PD_PDO_TYPE_FIXED |
		    USBC_PD_PDO_FIXED_USB_COMMS |
		    USBC_PD_PDO_FIXED_VOLTAGE_MV(5000) |
		    USBC_PD_PDO_FIXED_CURRENT_MA(900);
		sc->policy = usbc_pd_policy_alloc(&sc->tcpc, &caps);
		if (sc->policy == NULL) {
			device_printf(dev, "cannot allocate PD policy\n");
			error = ENOMEM;
			goto fail;
		}
	}

	/* IRQ allocation — same multi-fallback as original */
	if (bus_get_resource(dev, SYS_RES_IRQ, sc->irq_rid, &irq_start,
	    &irq_count) == 0) {
		FUSB302_DPRINTF(sc, "irq metadata start=%ju count=%ju\n",
		    (uintmax_t)irq_start, (uintmax_t)irq_count);
	}
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &sc->irq_rid,
	    RF_ACTIVE);
	if (sc->irq_res == NULL) {
		error = fusb302_try_ofw_irq(sc);
		if (error == 0)
			sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ,
			    &sc->irq_rid, RF_ACTIVE);
	}
	if (sc->irq_res != NULL) {
		/* Filter + ithread: filter returns FILTER_SCHEDULE_THREAD so
		 * INTRNG masks the GPIO source for the duration of the
		 * ithread. Required because FUSB302 INT_N is level-low and
		 * bus_suspend_intr does not mask GPIO-routed IRQs. */
		error = bus_setup_intr(dev, sc->irq_res,
		    INTR_TYPE_MISC | INTR_MPSAFE, fusb302_intr,
		    fusb302_irq_ithread, sc, &sc->irq_cookie);
		if (error != 0) {
			device_printf(dev, "cannot setup irq: %d\n", error);
			bus_release_resource(dev, SYS_RES_IRQ, sc->irq_rid,
			    sc->irq_res);
			sc->irq_res = NULL;
			sc->irq_cookie = NULL;
		}
	}
	if (sc->irq_res == NULL) {
		device_printf(dev, "cannot allocate irq, aborting attach\n");
		error = ENXIO;
		goto fail;
	}

	/*
	 * Now that the IRQ filter is registered, enable the chip's INT_N
	 * output. Doing this earlier (in init_locked) leaves a window
	 * where the chip pulls GPIO low with no handler, producing the
	 * "gpio1: Interrupt pin=2 unhandled" storm at the rk_gpio level.
	 */
	/*
	 * Clear the chip's INT_N output mask as the FINAL register access
	 * before we expose ourselves to the IRQ task. Done unlocked: any
	 * IRQ that fires after this point sees a fully-initialized softc
	 * and contention on sc->mtx between attach and the IRQ task is
	 * impossible because attach has no further work to do under the
	 * lock. (Holding sc->mtx around this update_reg would resurrect
	 * the same sleep-with-nosleep-lock panic that init_locked hit.)
	 */
	(void)fusb302_update_reg(sc, FUSB_REG_CONTROL0,
	    FUSB_CTL0_INT_MASK, 0);

	/*
	 * If init_locked's manual SNK probe transitioned us into
	 * ATTACHED_SNK, the state machine is parked with work_continue set
	 * but no IRQ pending to drain it. Kick the task once so sub_state 0
	 * runs and arms the T_ATTACH_WAIT timer.
	 */
	if (sc->conn_state != FUSB_ST_UNATTACHED)
		taskqueue_enqueue(taskqueue_thread, &sc->irq_task);

	/*
	 * Lift the policy out of USBC_PD_S_DISABLED so it begins observing
	 * CC_CHANGE / VBUS_CHANGE / RX events from the IRQ task.  The
	 * policy picks UNATTACHED_SRC or UNATTACHED_SNK based on caps;
	 * from there it tracks attach state in parallel with the embedded
	 * SM.  Step 9 will remove the embedded SM and let the policy be
	 * the sole driver.
	 */
	usbc_pd_policy_event(sc->policy, USBC_PD_E_PORT_ENABLE);

	fusb302_add_sysctls(sc);
	OF_device_register_xref(OF_xref_from_node(ofw_bus_get_node(dev)), dev);
	return (0);

fail:
	callout_drain(&sc->timer_state);
	if (sc->policy != NULL) {
		usbc_pd_policy_free(sc->policy);
		sc->policy = NULL;
	}
	if (sc->vbus_enabled)
		regulator_disable(sc->vbus_supply);
	if (sc->vbus_supply != NULL)
		regulator_release(sc->vbus_supply);
	sx_destroy(&sc->sx);
	return (ENXIO);
}

static int
fusb302_detach(device_t dev)
{
	struct fusb302_softc *sc;

	sc = device_get_softc(dev);

	/*
	 * Detach must NOT hold sc->mtx across any I2C transfer (rk_i2c
	 * sleeps in _sleep). Holding the mutex while sleeping triggers
	 * propagate_priority's "sleeping thread holds non-sleepable lock"
	 * panic if the IRQ task wakes up and tries to grab sc->mtx.
	 *
	 * Safe order:
	 *   1. Tear down the IRQ filter (waits for any in-flight handler).
	 *   2. Drain the callout (stops any pending state timer).
	 *   3. Drain the IRQ task (waits for any queued/in-flight task).
	 *   4. Now no other thread can touch sc — do I2C unlocked.
	 *   5. Release resources, destroy mutex.
	 */

	/* Signal in-flight task to bail quickly. Race-free even unlocked
	 * because the task reads `initialized` after acquiring the mutex,
	 * and any value it sees is followed by a quick exit. */
	sc->initialized = false;
	sc->work_continue = 0;

	/*
	 * Mask the chip's INT_N output FIRST, before tearing down the IRQ
	 * handler. Otherwise GPIO1 sees the chip's level-triggered INT_N
	 * still asserted with no consumer and floods the console with
	 * "gpio1: Interrupt pin=2 unhandled". The chip's INT_MASK bit
	 * suppresses the line itself, so once this write lands the GPIO
	 * goes idle. Done unlocked: rk_i2c serializes transactions, and
	 * any concurrent irq_task I2C just queues behind us.
	 */
	(void)fusb302_write_reg(sc, FUSB_REG_CONTROL0,
	    FUSB_CTL0_HOST_CUR_DEF | FUSB_CTL0_INT_MASK);

	if (sc->irq_cookie != NULL) {
		bus_teardown_intr(dev, sc->irq_res, sc->irq_cookie);
		sc->irq_cookie = NULL;
	}

	/* Drain order matters: callout BEFORE task so the timer can't
	 * enqueue a fresh task between drains. */
	callout_drain(&sc->timer_state);
	taskqueue_drain(taskqueue_thread, &sc->irq_task);

	/*
	 * Tear down the policy SM after the IRQ task is fully drained
	 * — once drained no remaining caller can dispatch into the
	 * policy.  Free before mtx_destroy so the policy's own callout
	 * is stopped while sc is still valid.
	 */
	if (sc->policy != NULL) {
		usbc_pd_policy_free(sc->policy);
		sc->policy = NULL;
	}

	/* Now safe to do I2C unlocked — no other thread reaches sc. */
	(void)fusb302_write_reg(sc, FUSB_REG_CONTROL0,
	    FUSB_CTL0_HOST_CUR_DEF | FUSB_CTL0_INT_MASK);
	(void)fusb302_write_reg(sc, FUSB_REG_RESET, FUSB_RESET_SW_RES);

	if (sc->irq_res != NULL) {
		bus_release_resource(dev, SYS_RES_IRQ, sc->irq_rid,
		    sc->irq_res);
		sc->irq_res = NULL;
	}
	bus_delete_resource(dev, SYS_RES_IRQ, sc->irq_rid);

	if (sc->vbus_enabled && sc->vbus_supply != NULL)
		regulator_disable(sc->vbus_supply);
	if (sc->vbus_supply != NULL)
		regulator_release(sc->vbus_supply);

	OF_device_register_xref(OF_xref_from_node(ofw_bus_get_node(dev)), NULL);
	sx_destroy(&sc->sx);
	return (0);
}

/*
 * fusb302_shutdown
 *
 * device_shutdown method, fired during `shutdown -p` / `reboot` while
 * the kernel is still up and able to do I2C transactions.  Its sole
 * job is to keep the FUSB302's INT_N output from continuing to fire
 * after the kernel has begun tearing down GPIO interrupt plumbing.
 *
 * Background: FUSB302 drives INT_N as a level-triggered open-drain
 * pulled by GPIO1 on RockPro64.  After kernel teardown removes our
 * IRQ handler but the chip is still asserting INT_N, the bare GPIO
 * controller raises an unhandled-interrupt warning every PD event,
 * flooding the serial console with `gpio1: Interrupt pin=2 unhandled`
 * for the rest of the shutdown.  Setting INT_MASK in CONTROL0 makes
 * the chip stop driving INT_N regardless of internal state, so the
 * shutdown sequence is silent.  Returns 0 unconditionally — failure
 * to write the register is harmless (we just get the storm).
 */
static int
fusb302_shutdown(device_t dev)
{
	struct fusb302_softc *sc;

	sc = device_get_softc(dev);
	(void)fusb302_write_reg(sc, FUSB_REG_CONTROL0,
	    FUSB_CTL0_HOST_CUR_DEF | FUSB_CTL0_INT_MASK);
	return (0);
}

static device_method_t fusb302_methods[] = {
	DEVMETHOD(device_probe,		fusb302_probe),
	DEVMETHOD(device_attach,	fusb302_attach),
	DEVMETHOD(device_detach,	fusb302_detach),
	DEVMETHOD(device_shutdown,	fusb302_shutdown),
	DEVMETHOD_END
};

static driver_t fusb302_driver = {
	"fusb302",
	fusb302_methods,
	sizeof(struct fusb302_softc),
};

DRIVER_MODULE(fusb302, iicbus, fusb302_driver, 0, 0);
MODULE_DEPEND(fusb302, iicbus, IICBUS_MINVER, IICBUS_PREFVER, IICBUS_MAXVER);
MODULE_DEPEND(fusb302, usbc, 1, 1, 1);
MODULE_VERSION(fusb302, 2);

/* -----------------------------------------------------------------------
 * TCPC backend skeleton (Phase 3, step 1).
 *
 * Stubs that will eventually let the chip-agnostic policy state machine
 * (sys/dev/iicbus/usb/usbc/usbc_pd_policy.c) drive this chip through
 * struct usbc_tcpc_ops instead of having its own embedded SM.  Each op
 * currently returns ENOSYS so consumers can detect "not implemented yet."
 * Subsequent commits replace each stub with real chip access, and
 * eventually the embedded state machine in this file goes away.
 * ----------------------------------------------------------------------- */

static int	fusb302_tcpc_set_rp(struct usbc_tcpc *,
		    enum usbc_pd_rp_value);
static int	fusb302_tcpc_set_role(struct usbc_tcpc *,
		    enum usbc_tcpc_role);
static int	fusb302_tcpc_set_polarity(struct usbc_tcpc *,
		    enum usbc_pd_polarity);
static int	fusb302_tcpc_set_vconn(struct usbc_tcpc *, bool);
static int	fusb302_tcpc_set_rx_enable(struct usbc_tcpc *, bool);
static int	fusb302_tcpc_get_cc(struct usbc_tcpc *,
		    enum usbc_tcpc_cc_state *, enum usbc_tcpc_cc_state *);
static int	fusb302_tcpc_get_vbus_present(struct usbc_tcpc *, bool *);
static int	fusb302_tcpc_set_vbus_source(struct usbc_tcpc *, bool);
static int	fusb302_tcpc_transmit(struct usbc_tcpc *, enum usbc_pd_sop,
		    const struct usbc_pd_msg *);
static int	fusb302_tcpc_receive(struct usbc_tcpc *,
		    enum usbc_pd_sop *, struct usbc_pd_msg *);
static int	fusb302_tcpc_send_hard_reset(struct usbc_tcpc *);

/*
 * set_rp: program the Rp current advertisement (CONTROL0 bits 2:1).
 * 01 = USB-default (80 uA), 10 = 1.5 A (180 uA), 11 = 3.0 A (330 uA).
 * The TCPC abstraction's enum maps directly: USB_DEFAULT, _1_5A, _3_0A.
 */
static int
fusb302_tcpc_set_rp(struct usbc_tcpc *t, enum usbc_pd_rp_value v)
{
	struct fusb302_softc *sc = t->softc;
	uint8_t bits;

	switch (v) {
	case USBC_PD_RP_USB_DEFAULT:
		bits = 0x02;
		break;
	case USBC_PD_RP_1_5A:
		bits = 0x04;
		break;
	case USBC_PD_RP_3_0A:
		bits = 0x06;
		break;
	default:
		return (EINVAL);
	}
	return (iic2errno(fusb302_update_reg(sc, FUSB_REG_CONTROL0,
	    0x06 /* HOST_CUR mask */, bits)));
}

/*
 * set_role: configure the chip's CC pull mode (CONTROL2[2:1]) and
 * enable / disable the toggle state machine (CONTROL2 bit 0).  The
 * TCPC role enum maps to chip mode bits: SNK = UFP, SRC = DFP, DRP
 * keeps DRP toggle enabled.  The chip's existing helper
 * fusb302_toggle_ctl2_locked() composes the right value from
 * sc->role_pref; we update role_pref then call it.
 */
static int
fusb302_tcpc_set_role(struct usbc_tcpc *t, enum usbc_tcpc_role r)
{
	struct fusb302_softc *sc = t->softc;
	int role_pref;

	switch (r) {
	case USBC_TCPC_ROLE_SNK:
		role_pref = FUSB_ROLE_SNK;
		break;
	case USBC_TCPC_ROLE_SRC:
		role_pref = FUSB_ROLE_SRC;
		break;
	case USBC_TCPC_ROLE_DRP:
		role_pref = FUSB_ROLE_DRP;
		break;
	default:
		return (EINVAL);
	}
	sc->role_pref = role_pref;
	return (iic2errno(fusb302_write_reg(sc, FUSB_REG_CONTROL2,
	    fusb302_toggle_ctl2_locked(sc))));
}

/*
 * set_polarity: pin the active CC orientation.  Delegates to the
 * existing chip helper, which programs SWITCHES0 (Rp/Rd/MEAS routing)
 * and SWITCHES1 (TX path) consistently for the chosen pin.
 */
static int
fusb302_tcpc_set_polarity(struct usbc_tcpc *t, enum usbc_pd_polarity p)
{
	struct fusb302_softc *sc = t->softc;
	int polarity = (p == USBC_PD_POLARITY_CC2) ? 1 : 0;

	fusb302_set_polarity_locked(sc, polarity);
	return (0);
}

/*
 * set_vconn: drive 5 V VCONN onto the inactive CC pin (the one
 * opposite the attached partner) to power active-cable e-marker
 * chips.  We update sc->vconn_enabled then re-run set_polarity to
 * have it write SWITCHES0 with the VCONN bit composed correctly
 * for the current cc_polarity.
 */
static int
fusb302_tcpc_set_vconn(struct usbc_tcpc *t, bool en)
{
	struct fusb302_softc *sc = t->softc;

	sc->vconn_enabled = en;
	fusb302_set_polarity_locked(sc, sc->cc_polarity);
	return (0);
}

/*
 * set_rx_enable: arm or disarm BMC reception on the active CC pin.
 * Delegates to the existing fusb302_enable_rx_locked() helper which
 * sets SWITCHES0.MEAS_CC{1,2} (so the chip listens on the right pin)
 * and SWITCHES1.AUTO_CRC (so hardware GoodCRC reply is enabled when
 * a valid PD message is received).
 */
static int
fusb302_tcpc_set_rx_enable(struct usbc_tcpc *t, bool en)
{
	struct fusb302_softc *sc = t->softc;

	fusb302_enable_rx_locked(sc, en);
	return (0);
}

/*
 * get_cc: report CC1/CC2 termination as observed by the chip's
 * toggle / measure logic.  We translate the FUSB302 TOGSS field
 * (last completed CC toggle result) into the policy-machine's
 * generic enum.  On a clean unattached port both pins read OPEN;
 * after toggle locks onto an Rd partner one CC reads RD (we as
 * source) or RP_USB (we as sink), the other stays OPEN.
 */
static int
fusb302_tcpc_get_cc(struct usbc_tcpc *t,
    enum usbc_tcpc_cc_state *cc1, enum usbc_tcpc_cc_state *cc2)
{
	struct fusb302_softc *sc = t->softc;
	uint8_t status1a;
	int err;

	if (cc1 != NULL)
		*cc1 = USBC_TCPC_CC_OPEN;
	if (cc2 != NULL)
		*cc2 = USBC_TCPC_CC_OPEN;
	err = fusb302_read_reg(sc, FUSB_REG_STATUS1A, &status1a);
	if (err != 0)
		return (err);
	switch ((status1a & FUSB_ST1A_TOGSS_MASK) >> FUSB_ST1A_TOGSS_SHIFT) {
	case FUSB_TOGSS_SRC_CC1:
		if (cc1 != NULL)
			*cc1 = USBC_TCPC_CC_RD;
		break;
	case FUSB_TOGSS_SRC_CC2:
		if (cc2 != NULL)
			*cc2 = USBC_TCPC_CC_RD;
		break;
	case FUSB_TOGSS_SNK_CC1:
		if (cc1 != NULL)
			*cc1 = USBC_TCPC_CC_RP_USB;
		break;
	case FUSB_TOGSS_SNK_CC2:
		if (cc2 != NULL)
			*cc2 = USBC_TCPC_CC_RP_USB;
		break;
	default:
		break;	/* TOGSS_NOTHING -- both CC open. */
	}
	return (0);
}

/*
 * get_vbus_present: read the chip's VBUS comparator.  STATUS0 bit
 * VBUSOK reflects whether VBUS is above the chip's "valid" threshold
 * (~vSafe5V).  Note RockPro64-style boards expose VBUSOK as always-1
 * regardless of partner state because of how the rail is wired; that
 * is a board-level wart, not a chip-level one.
 */
static int
fusb302_tcpc_get_vbus_present(struct usbc_tcpc *t, bool *present)
{
	struct fusb302_softc *sc = t->softc;
	uint8_t status0;
	int err;

	if (present == NULL)
		return (EINVAL);
	*present = false;
	err = fusb302_read_reg(sc, FUSB_REG_STATUS0, &status0);
	if (err != 0)
		return (err);
	*present = (status0 & FUSB_ST0_VBUSOK) != 0;
	return (0);
}

/*
 * set_vbus_source: enable / disable the VBUS power regulator the
 * chip drives toward the partner.  On RockPro64 this is the
 * SY6280AAC switch fed from VCC5V0_USB.  Idempotent against
 * sc->vbus_enabled to avoid double-enabling the regulator.
 */
static int
fusb302_tcpc_set_vbus_source(struct usbc_tcpc *t, bool en)
{
	struct fusb302_softc *sc = t->softc;
	int err;

	if (sc->passive_src) {
		/* In passive SRC mode, never touch the regulator. The partner
		 * is sourcing VBUS; we only care about presenting Rp on CC. */
		return (0);
	}
	if (sc->vbus_supply == NULL)
		return (ENXIO);
	if (en && !sc->vbus_enabled) {
		err = regulator_enable(sc->vbus_supply);
		if (err != 0)
			return (err);
		sc->vbus_enabled = true;
	} else if (!en && sc->vbus_enabled) {
		err = regulator_disable(sc->vbus_supply);
		if (err != 0)
			return (err);
		sc->vbus_enabled = false;
	}
	return (0);
}

/*
 * transmit: hand a PD message off to the chip's TX FIFO.  Copies
 * msg->hdr / data[] into sc->send_head / send_load[] and kicks
 * fusb302_fifo_write_locked() which writes BMC tokens + payload to
 * the chip.  Hardware handles BMC encoding, CRC32, retries, and
 * GoodCRC reception.  Completion is asynchronous: the chip's IRQ
 * handler will eventually convert TXSENT or RETRYFAIL to a
 * USBC_PD_E_TX_OK / USBC_PD_E_TX_FAIL event into the policy
 * machine (wiring lands in a later commit).  Only SOP is supported
 * for now; SOP'/SOP'' (cable e-marker) lands later.
 */
static int
fusb302_tcpc_transmit(struct usbc_tcpc *t, enum usbc_pd_sop sop,
    const struct usbc_pd_msg *msg)
{
	struct fusb302_softc *sc = t->softc;
	uint8_t i;

	if (msg == NULL)
		return (EINVAL);
	if (sop != USBC_PD_SOP)
		return (ENOTSUP);

	sc->send_head = msg->hdr;
	for (i = 0; i < msg->ndo && i < nitems(sc->send_load); i++)
		sc->send_load[i] = msg->data[i];
	return (fusb302_fifo_write_locked(sc));
}

/*
 * receive: pull one PD message from the chip's RX FIFO.  Wraps
 * fusb302_fifo_read_locked() and copies sc->rec_head / rec_load[]
 * into the caller's struct usbc_pd_msg.  Returns ENOENT if no
 * message is available; the chip auto-discards GoodCRC frames so
 * what we surface is always a real partner message.
 */
static int
fusb302_tcpc_receive(struct usbc_tcpc *t, enum usbc_pd_sop *sop_out,
    struct usbc_pd_msg *msg)
{
	struct fusb302_softc *sc = t->softc;
	uint8_t ndo, i;
	int err;

	if (msg == NULL)
		return (EINVAL);
	err = fusb302_fifo_read_locked(sc);
	if (err != 0)
		return (err);
	msg->hdr = sc->rec_head;
	ndo = (uint8_t)PD_HDR_CNT(sc->rec_head);
	if (ndo > nitems(msg->data))
		ndo = nitems(msg->data);
	msg->ndo = ndo;
	for (i = 0; i < ndo; i++)
		msg->data[i] = sc->rec_load[i];
	if (sop_out != NULL)
		*sop_out = USBC_PD_SOP;
	return (0);
}

/*
 * send_hard_reset: pulse CONTROL3.SEND_HARDRST to make the chip
 * emit a Hard Reset on the active CC line.  The chip clears the
 * bit on its own once the reset is on the wire and then raises a
 * HARDSENT interrupt which becomes USBC_PD_E_HARD_RESET_TX_OK in
 * the policy machine.
 */
static int
fusb302_tcpc_send_hard_reset(struct usbc_tcpc *t)
{
	struct fusb302_softc *sc = t->softc;

	return (iic2errno(fusb302_update_reg(sc, FUSB_REG_CONTROL3,
	    FUSB_CTL3_SEND_HARDRST, FUSB_CTL3_SEND_HARDRST)));
}

/*
 * Public ops table.  The policy machine populates struct usbc_tcpc with
 * a pointer to this and a chip-private softc handle.
 */
const struct usbc_tcpc_ops fusb302_tcpc_ops = {
	.set_rp		= fusb302_tcpc_set_rp,
	.set_role	= fusb302_tcpc_set_role,
	.set_polarity	= fusb302_tcpc_set_polarity,
	.set_vconn	= fusb302_tcpc_set_vconn,
	.set_rx_enable	= fusb302_tcpc_set_rx_enable,
	.get_cc		= fusb302_tcpc_get_cc,
	.get_vbus_present = fusb302_tcpc_get_vbus_present,
	.set_vbus_source = fusb302_tcpc_set_vbus_source,
	.transmit	= fusb302_tcpc_transmit,
	.receive	= fusb302_tcpc_receive,
	.send_hard_reset = fusb302_tcpc_send_hard_reset,
};
