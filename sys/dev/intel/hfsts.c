/*
 * Copyright (c) 2026 Abdelkader Boudih <seuros@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * hfsts(4) - Intel Management Engine Interface (MEI/HECI) debug probe.
 *
 * This is deliberately *not* a full MEI host-client driver: it only
 * attaches to the PCI function and decodes the Host Firmware Status
 * registers (HFSTS1-6, as supported by the device generation), which live
 * entirely in PCI configuration space.
 * BAR0 (the circular-buffer messaging ring) is never mapped.
 *
 * Register counts and layout follow Linux drivers/misc/mei/hw-me.c:
 * ICH10 provides HFS1, PCH5 through PCH7 provide HFS1-2, and PCH8 and
 * later provide HFS1-6 at PCI config offsets 0x40/0x48/0x60/0x64/0x68/0x6c.
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/sysctl.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/ichwd/ichwd.h>
#include <dev/intel/hfsts.h>

/* Host Firmware Status Registers - PCI configuration space offsets. */
#define HFSTS_CFG_HFS_1		0x40
#define   HFSTS_HFS_1_CWS_MSK		0x0000000f
#define   HFSTS_HFS_1_MFG_MODE_MSK	0x00000010
#define   HFSTS_HFS_1_FPT_BAD_MSK		0x00000020
#define   HFSTS_HFS_1_OPSTATE_MSK	0x000001c0
#define   HFSTS_HFS_1_OPSTATE_SHIFT	6
#define   HFSTS_HFS_1_FW_INIT_CMPL_MSK	0x00000200
#define   HFSTS_HFS_1_BUP_FAIL_MSK	0x00000400
#define   HFSTS_HFS_1_UPDATE_INPROG_MSK	0x00000800
#define   HFSTS_HFS_1_ERROR_MSK		0x0000f000
#define   HFSTS_HFS_1_ERROR_SHIFT	12
#define   HFSTS_HFS_1_OPMODE_MSK		0x000f0000
#define   HFSTS_HFS_1_OPMODE_SHIFT	16
#define   HFSTS_HFS_1_BOOT_OPT_MSK	0x01000000
#define   HFSTS_HFS_1_D0I3_MSK		0x80000000
#define HFSTS_CFG_HFS_2		0x48
#define   HFSTS_HFS_2_PM_EVENT_MSK	0x0f000000
#define   HFSTS_HFS_2_PM_EVENT_SHIFT	24
#define HFSTS_CFG_HFS_3		0x60
#define   HFSTS_HFS_3_FW_SKU_MSK		0x00000070
#define   HFSTS_HFS_3_FW_SKU_SHIFT	4
#define HFSTS_CFG_HFS_4		0x64
#define HFSTS_CFG_HFS_5		0x68
#define HFSTS_CFG_HFS_6		0x6c

/* HFS1 field values.  Operation state and mode are sparse encodings. */
#define HFSTS_HFS_CWS_RESET		0
#define HFSTS_HFS_CWS_INIT		1
#define HFSTS_HFS_CWS_RECOVERY		2
#define HFSTS_HFS_CWS_TEST		3
#define HFSTS_HFS_CWS_DISABLED		4
#define HFSTS_HFS_CWS_NORMAL		5
#define HFSTS_HFS_CWS_WAIT		6
#define HFSTS_HFS_CWS_TRANS		7
#define HFSTS_HFS_CWS_INVALID		8

#define HFSTS_HFS_OPSTATE_PREBOOT		0
#define HFSTS_HFS_OPSTATE_M0_UMA		1
#define HFSTS_HFS_OPSTATE_M3		4
#define HFSTS_HFS_OPSTATE_M0		5
#define HFSTS_HFS_OPSTATE_BRINGUP	6
#define HFSTS_HFS_OPSTATE_IMAGE_ERROR	7

#define HFSTS_HFS_OPMODE_NORMAL		0
#define HFSTS_HFS_OPMODE_DEBUG		2
#define HFSTS_HFS_OPMODE_TEMP_DISABLED	3
#define HFSTS_HFS_OPMODE_JUMPER_OVERRIDE	4
#define HFSTS_HFS_OPMODE_MEI_OVERRIDE	5
#define HFSTS_HFS_OPMODE_SPS		15

#define HFSTS_HFS_ERROR_NONE		0
#define HFSTS_HFS_ERROR_UNCATEGORIZED	1
#define HFSTS_HFS_ERROR_DISABLED		2
#define HFSTS_HFS_ERROR_IMAGE		3
#define HFSTS_HFS_ERROR_DEBUG		4

static const char *const hfsts_cws_values[] = {
	[HFSTS_HFS_CWS_RESET] =		"Reset",
	[HFSTS_HFS_CWS_INIT] =		"Initializing",
	[HFSTS_HFS_CWS_RECOVERY] =	"Recovery",
	[HFSTS_HFS_CWS_TEST] =		"Test",
	[HFSTS_HFS_CWS_DISABLED] =	"Disabled",
	[HFSTS_HFS_CWS_NORMAL] =		"Normal",
	[HFSTS_HFS_CWS_WAIT] =		"Platform Disable Wait",
	[HFSTS_HFS_CWS_TRANS] =		"OP State Transition",
	[HFSTS_HFS_CWS_INVALID] =	"Invalid CPU Plugged In",
};

static const char *const hfsts_opstate_values[] = {
	[HFSTS_HFS_OPSTATE_PREBOOT] =	"Preboot",
	[HFSTS_HFS_OPSTATE_M0_UMA] =	"M0 (UMA)",
	[HFSTS_HFS_OPSTATE_M3] =		"M3 (no UMA)",
	[HFSTS_HFS_OPSTATE_M0] =		"M0 (no UMA)",
	[HFSTS_HFS_OPSTATE_BRINGUP] =	"Bring up",
	[HFSTS_HFS_OPSTATE_IMAGE_ERROR] = "M0 (invalid firmware image)",
};

static const char *const hfsts_opmode_values[] = {
	[HFSTS_HFS_OPMODE_NORMAL] =	"Normal",
	[HFSTS_HFS_OPMODE_DEBUG] =	"Debug",
	[HFSTS_HFS_OPMODE_TEMP_DISABLED] = "Temporarily disabled",
	[HFSTS_HFS_OPMODE_JUMPER_OVERRIDE] = "Security override (jumper)",
	[HFSTS_HFS_OPMODE_MEI_OVERRIDE] = "Security override (MEI)",
	[HFSTS_HFS_OPMODE_SPS] =		"Server Platform Services",
};

static const char *const hfsts_error_values[] = {
	[HFSTS_HFS_ERROR_NONE] =		"None",
	[HFSTS_HFS_ERROR_UNCATEGORIZED] = "Uncategorized",
	[HFSTS_HFS_ERROR_DISABLED] =	"Disabled",
	[HFSTS_HFS_ERROR_IMAGE] =	"Firmware image",
	[HFSTS_HFS_ERROR_DEBUG] =	"Debug",
};

static const char *
hfsts_decode(const char *const *tbl, size_t tbl_sz, uint32_t idx)
{

	if (idx >= tbl_sz || tbl[idx] == NULL)
		return ("Unknown");
	return (tbl[idx]);
}
#define HFSTS_DECODE(tbl, idx)	hfsts_decode((tbl), nitems(tbl), (idx))

/* HFS1-6 PCI configuration offsets, indexed 0-5. */
static const uint32_t hfsts_hfs_off[6] = {
	HFSTS_CFG_HFS_1, HFSTS_CFG_HFS_2, HFSTS_CFG_HFS_3,
	HFSTS_CFG_HFS_4, HFSTS_CFG_HFS_5, HFSTS_CFG_HFS_6,
};

/*
 * All HFS fields are re-read from PCI configuration space on every sysctl
 * access: the ME can transition CWS/opstate/error post-boot (watchdog
 * resets, HAP toggles, S0i3/M0-M3 power events), so an attach-time snapshot
 * would go stale under a running system.
 */
static uint32_t
hfsts_read_hfs(device_t dev, u_int idx)
{

	return (pci_read_config(dev, hfsts_hfs_off[idx], 4));
}

/*
 * One-word summary for the log/sysctl consumer who doesn't want to parse
 * the raw HFS fields: Absent (register read didn't land on a real state),
 * Disabled (ME explicitly turned off), Active (busy, transitional, or in an
 * exceptional mode), or Normal (steady-state, with no reported error).
 */
static const char *
hfsts_state_summary(uint32_t cws, uint32_t opmode, uint32_t error,
	    uint32_t mfg_mode, uint32_t fpt_bad, uint32_t bup_fail,
	    uint32_t fw_init_cmpl, uint32_t update_inprog)
{

	if (cws >= nitems(hfsts_cws_values) || hfsts_cws_values[cws] == NULL)
		return ("Absent");
	if (cws == HFSTS_HFS_CWS_DISABLED || cws == HFSTS_HFS_CWS_WAIT ||
	    opmode == HFSTS_HFS_OPMODE_TEMP_DISABLED ||
	    error == HFSTS_HFS_ERROR_DISABLED)
		return ("Disabled");
	if (cws != HFSTS_HFS_CWS_NORMAL || opmode != HFSTS_HFS_OPMODE_NORMAL ||
	    error != HFSTS_HFS_ERROR_NONE || mfg_mode || fpt_bad || bup_fail ||
	    !fw_init_cmpl || update_inprog)
		return ("Active");
	return ("Normal");
}

struct hfsts_device {
	uint16_t	device;
	uint8_t		hfs_count;
	const char	*name;
};

/*
 * Intel MEI/HECI PCI functions with usable Host Firmware Status registers.
 * ICH10 has HFS1, PCH5 through PCH7 have HFS1-2, and PCH8 and newer have
 * HFS1-6.  Legacy ICH devices, for which no HFS register is defined, are
 * deliberately not matched.
 */
#define HFSTS_DEV_ICH10(id, desc)	{ (id), 1, (desc) }
#define HFSTS_DEV_PCH(id, desc)		{ (id), 2, (desc) }
#define HFSTS_DEV_PCH8(id, desc)	{ (id), 6, (desc) }

static const struct hfsts_device hfsts_devices[] = {
	HFSTS_DEV_ICH10(0x2e04, "Eaglelake MEI"),
	HFSTS_DEV_ICH10(0x2e14, "Eaglelake MEI"),
	HFSTS_DEV_ICH10(0x2e24, "Eaglelake MEI"),
	HFSTS_DEV_ICH10(0x2e34, "Eaglelake MEI"),
	HFSTS_DEV_PCH(0x3b64, "Calpella MEI"),
	HFSTS_DEV_PCH(0x3b65, "Calpella MEI"),
	HFSTS_DEV_PCH(0x1c3a, "Cougar Point MEI"),
	HFSTS_DEV_PCH(0x1d3a, "C600/X79 Patsburg MEI"),
	HFSTS_DEV_PCH(0x1e3a, "Panther Point MEI"),
	HFSTS_DEV_PCH(0x1cba, "Panther Point MEI"),
	HFSTS_DEV_PCH(0x1dba, "Panther Point MEI"),
	HFSTS_DEV_PCH8(0x8c3a, "Lynx Point H MEI"),
	HFSTS_DEV_PCH8(0x8d3a, "Lynx Point Wellsburg MEI"),
	HFSTS_DEV_PCH8(0x9c3a, "Lynx Point LP MEI"),
	HFSTS_DEV_PCH8(0x8cba, "Lynx Point H Refresh MEI"),
	HFSTS_DEV_PCH8(0x9cba, "Wildcat Point LP MEI"),
	HFSTS_DEV_PCH8(0x9cbb, "Wildcat Point LP 2 MEI"),
	HFSTS_DEV_PCH8(0x9d3a, "Sunrise Point MEI"),
	HFSTS_DEV_PCH8(0x9d3b, "Sunrise Point 2 MEI"),
	HFSTS_DEV_PCH8(0x9d3e, "Sunrise Point 3 (iTouch) MEI"),
	HFSTS_DEV_PCH8(0xa13a, "Sunrise Point H MEI"),
	HFSTS_DEV_PCH8(0xa13b, "Sunrise Point H 2 MEI"),
	HFSTS_DEV_PCH8(0xa1ba, "Lewisburg (SPT) MEI"),
	HFSTS_DEV_PCH8(0x1a9a, "Broxton M MEI"),
	HFSTS_DEV_PCH8(0x5a9a, "Apollo Lake I MEI"),
	HFSTS_DEV_PCH8(0x19e5, "Denverton IE MEI"),
	HFSTS_DEV_PCH8(0x319a, "Gemini Lake MEI"),
	HFSTS_DEV_PCH8(0xa2ba, "Kaby Point MEI"),
	HFSTS_DEV_PCH8(0xa2bb, "Kaby Point 2 MEI"),
	HFSTS_DEV_PCH8(0xa2be, "Kaby Point 3 (iTouch) MEI"),
	HFSTS_DEV_PCH8(0x9de0, "Cannon Point LP MEI"),
	HFSTS_DEV_PCH8(0x9de4, "Cannon Point LP 3 (iTouch) MEI"),
	HFSTS_DEV_PCH8(0xa360, "Cannon Point H MEI"),
	HFSTS_DEV_PCH8(0xa364, "Cannon Point H 3 (iTouch) MEI"),
	HFSTS_DEV_PCH8(0x02e0, "Comet Point LP MEI"),
	HFSTS_DEV_PCH8(0x02e4, "Comet Point LP 3 (iTouch) MEI"),
	HFSTS_DEV_PCH8(0xa3ba, "Comet Point Lake V MEI"),
	HFSTS_DEV_PCH8(0x06e0, "Comet Lake H MEI"),
	HFSTS_DEV_PCH8(0x06e4, "Comet Lake H 3 (iTouch) MEI"),
	HFSTS_DEV_PCH8(0x18d3, "Cedar Fork MEI"),
	HFSTS_DEV_PCH8(0x34e0, "Ice Lake Point LP MEI"),
	HFSTS_DEV_PCH8(0x38e0, "Ice Lake Point N MEI"),
	HFSTS_DEV_PCH8(0x4de0, "Jasper Lake Point N MEI"),
	HFSTS_DEV_PCH8(0xa0e0, "Tiger Lake Point LP MEI"),
	HFSTS_DEV_PCH8(0x43e0, "Tiger Lake Point H MEI"),
	HFSTS_DEV_PCH8(0x4b70, "Mule Creek Canyon (EHL) MEI"),
	HFSTS_DEV_PCH8(0x4b75, "Mule Creek Canyon 4 (EHL) MEI"),
	HFSTS_DEV_PCH8(0x1be0, "Emmitsburg WS MEI"),
	HFSTS_DEV_PCH8(0x7ae8, "Alder Lake Point S MEI"),
	HFSTS_DEV_PCH8(0x7a60, "Alder Lake Point LP MEI"),
	HFSTS_DEV_PCH8(0x51e0, "Alder Lake Point P MEI"),
	HFSTS_DEV_PCH8(0x54e0, "Alder Lake Point N MEI"),
	HFSTS_DEV_PCH8(0x7a68, "Raptor Lake Point S MEI"),
	HFSTS_DEV_PCH8(0x7e70, "Meteor Lake Point M MEI"),
	HFSTS_DEV_PCH8(0x7f68, "Arrow Lake Point S MEI"),
	HFSTS_DEV_PCH8(0x7770, "Arrow Lake Point H MEI"),
	HFSTS_DEV_PCH8(0xa870, "Lunar Lake Point M MEI"),
	HFSTS_DEV_PCH8(0xe370, "Panther Lake H MEI"),
	HFSTS_DEV_PCH8(0xe470, "Panther Lake P MEI"),
	HFSTS_DEV_PCH8(0x4d70, "Wildcat Lake P MEI"),
	HFSTS_DEV_PCH8(0x6e68, "Nova Lake Point S MEI"),
	HFSTS_DEV_PCH8(0xd370, "Nova Lake Point H MEI"),
	{ 0, 0, NULL }
};

static const struct hfsts_device *
hfsts_find_device(device_t dev)
{
	const struct hfsts_device *id;

	for (id = hfsts_devices; id->name != NULL; id++) {
		if (pci_get_device(dev) == id->device)
			return (id);
	}
	return (NULL);
}

/*
 * The isa-side Function Disable check is only meaningful when no known
 * MEI/HECI function enumerated on PCI.
 */
bool
hfsts_pci_present(void)
{
	const struct hfsts_device *id;

	for (id = hfsts_devices; id->name != NULL; id++) {
		if (pci_find_device(VENDORID_INTEL, id->device) != NULL)
			return (true);
	}
	return (false);
}

struct hfsts_softc {
	device_t		sc_dev;
	struct sysctl_ctx_list	*sc_sysctlctx;
	struct sysctl_oid	*sc_sysctlnode;
	uint8_t			sc_hfs_count;
};

/* Simple masked/shifted HFS sub-fields, read live via a common handler. */
enum hfsts_field_id {
	HFSTS_FLD_D0I3_SUPPORTED,
	HFSTS_FLD_OPMODE,
	HFSTS_FLD_PM_EVENT,
	HFSTS_FLD_FW_SKU,
	HFSTS_FLD_MFG_MODE,
	HFSTS_FLD_UPDATE_INPROG,
};

struct hfsts_field {
	uint8_t		hfs_idx;
	uint32_t	mask;
	uint8_t		shift;
};

static const struct hfsts_field hfsts_fields[] = {
	[HFSTS_FLD_D0I3_SUPPORTED] = { 0, HFSTS_HFS_1_D0I3_MSK, 31 },
	[HFSTS_FLD_OPMODE] =	{ 0, HFSTS_HFS_1_OPMODE_MSK,
				    HFSTS_HFS_1_OPMODE_SHIFT },
	[HFSTS_FLD_PM_EVENT] =	{ 1, HFSTS_HFS_2_PM_EVENT_MSK,
				    HFSTS_HFS_2_PM_EVENT_SHIFT },
	[HFSTS_FLD_FW_SKU] =	{ 2, HFSTS_HFS_3_FW_SKU_MSK,
				    HFSTS_HFS_3_FW_SKU_SHIFT },
	[HFSTS_FLD_MFG_MODE] =	{ 0, HFSTS_HFS_1_MFG_MODE_MSK, 4 },
	[HFSTS_FLD_UPDATE_INPROG] = { 0, HFSTS_HFS_1_UPDATE_INPROG_MSK, 11 },
};

static int
hfsts_sysctl_raw(SYSCTL_HANDLER_ARGS)
{
	struct hfsts_softc *sc = arg1;
	uint32_t val;

	val = hfsts_read_hfs(sc->sc_dev, (u_int)arg2);
	return (sysctl_handle_32(oidp, &val, 0, req));
}

static int
hfsts_sysctl_field(SYSCTL_HANDLER_ARGS)
{
	struct hfsts_softc *sc = arg1;
	const struct hfsts_field *f = &hfsts_fields[arg2];
	uint32_t val;

	val = (hfsts_read_hfs(sc->sc_dev, f->hfs_idx) & f->mask) >> f->shift;
	return (sysctl_handle_32(oidp, &val, 0, req));
}

/* HFS1 sub-fields that decode to a string via a lookup table. */
enum hfsts_decode_id {
	HFSTS_DEC_CWS,
	HFSTS_DEC_OPSTATE,
	HFSTS_DEC_OPMODE,
	HFSTS_DEC_ERROR,
};

struct hfsts_decode {
	uint32_t		mask;
	uint8_t			shift;
	const char *const	*tbl;
	size_t			tbl_sz;
};

static const struct hfsts_decode hfsts_decodes[] = {
	[HFSTS_DEC_CWS] =     { HFSTS_HFS_1_CWS_MSK, 0,
				    hfsts_cws_values, nitems(hfsts_cws_values) },
	[HFSTS_DEC_OPSTATE] = { HFSTS_HFS_1_OPSTATE_MSK,
				    HFSTS_HFS_1_OPSTATE_SHIFT,
				    hfsts_opstate_values,
				    nitems(hfsts_opstate_values) },
	[HFSTS_DEC_OPMODE] =  { HFSTS_HFS_1_OPMODE_MSK,
				    HFSTS_HFS_1_OPMODE_SHIFT,
				    hfsts_opmode_values,
				    nitems(hfsts_opmode_values) },
	[HFSTS_DEC_ERROR] =   { HFSTS_HFS_1_ERROR_MSK, HFSTS_HFS_1_ERROR_SHIFT,
				    hfsts_error_values, nitems(hfsts_error_values) },
};

static int
hfsts_sysctl_decode(SYSCTL_HANDLER_ARGS)
{
	struct hfsts_softc *sc = arg1;
	const struct hfsts_decode *d = &hfsts_decodes[arg2];
	uint32_t val;

	val = (hfsts_read_hfs(sc->sc_dev, 0) & d->mask) >> d->shift;
	return (SYSCTL_OUT_STR(req, hfsts_decode(d->tbl, d->tbl_sz, val)));
}

static int
hfsts_sysctl_summary(SYSCTL_HANDLER_ARGS)
{
	struct hfsts_softc *sc = arg1;
	uint32_t hfs1, cws, opmode, error;
	uint32_t mfg_mode, fpt_bad, bup_fail, fw_init_cmpl, update_inprog;

	hfs1 = hfsts_read_hfs(sc->sc_dev, 0);
	cws = hfs1 & HFSTS_HFS_1_CWS_MSK;
	opmode = (hfs1 & HFSTS_HFS_1_OPMODE_MSK) >> HFSTS_HFS_1_OPMODE_SHIFT;
	error = (hfs1 & HFSTS_HFS_1_ERROR_MSK) >> HFSTS_HFS_1_ERROR_SHIFT;
	mfg_mode = (hfs1 & HFSTS_HFS_1_MFG_MODE_MSK) != 0;
	fpt_bad = (hfs1 & HFSTS_HFS_1_FPT_BAD_MSK) != 0;
	bup_fail = (hfs1 & HFSTS_HFS_1_BUP_FAIL_MSK) != 0;
	fw_init_cmpl = (hfs1 & HFSTS_HFS_1_FW_INIT_CMPL_MSK) != 0;
	update_inprog = (hfs1 & HFSTS_HFS_1_UPDATE_INPROG_MSK) != 0;

	return (SYSCTL_OUT_STR(req, hfsts_state_summary(cws, opmode, error,
	    mfg_mode, fpt_bad, bup_fail, fw_init_cmpl, update_inprog)));
}

/* Table-driven sysctl registration; skipped when sc_hfs_count < min_hfs. */
typedef int hfsts_sysctl_handler_t(SYSCTL_HANDLER_ARGS);

struct hfsts_sysctl_def {
	const char		*name;
	int			ctltype;
	intmax_t		arg2;
	hfsts_sysctl_handler_t	*handler;
	const char		*fmt;
	const char		*descr;
	uint8_t			min_hfs;
};

static const struct hfsts_sysctl_def hfsts_sysctls[] = {
	{ "hfs1", CTLTYPE_U32, 0, hfsts_sysctl_raw, "IU",
	    "Host Firmware Status 1 (raw, live)", 1 },
	{ "hfs2", CTLTYPE_U32, 1, hfsts_sysctl_raw, "IU",
	    "Host Firmware Status 2 (raw, live)", 2 },
	{ "hfs3", CTLTYPE_U32, 2, hfsts_sysctl_raw, "IU",
	    "Host Firmware Status 3 (raw, live)", 6 },
	{ "hfs4", CTLTYPE_U32, 3, hfsts_sysctl_raw, "IU",
	    "Host Firmware Status 4 (raw, live)", 6 },
	{ "hfs5", CTLTYPE_U32, 4, hfsts_sysctl_raw, "IU",
	    "Host Firmware Status 5 (raw, live)", 6 },
	{ "hfs6", CTLTYPE_U32, 5, hfsts_sysctl_raw, "IU",
	    "Host Firmware Status 6 (raw, live)", 6 },
	{ "d0i3_supported", CTLTYPE_U32, HFSTS_FLD_D0I3_SUPPORTED,
	    hfsts_sysctl_field, "IU",
	    "ME D0i3 capability (from HFS1, live)", 6 },
	{ "opmode", CTLTYPE_U32, HFSTS_FLD_OPMODE, hfsts_sysctl_field, "IU",
	    "ME operation mode (from HFS1, live)", 1 },
	{ "pm_event", CTLTYPE_U32, HFSTS_FLD_PM_EVENT, hfsts_sysctl_field,
	    "IU", "ME power management event (from HFS2, live)", 2 },
	{ "fw_sku", CTLTYPE_U32, HFSTS_FLD_FW_SKU, hfsts_sysctl_field, "IU",
	    "ME firmware SKU (from HFS3, live)", 6 },
	{ "mfg_mode", CTLTYPE_U32, HFSTS_FLD_MFG_MODE, hfsts_sysctl_field,
	    "IU", "ME running in Manufacturing Mode (from HFS1, live)", 1 },
	{ "update_in_progress", CTLTYPE_U32, HFSTS_FLD_UPDATE_INPROG,
	    hfsts_sysctl_field, "IU",
	    "ME firmware update in progress (from HFS1, live)", 1 },
	{ "state", CTLTYPE_STRING, HFSTS_DEC_CWS, hfsts_sysctl_decode, "A",
	    "ME Current Working State (from HFS1, live)", 1 },
	{ "opstate", CTLTYPE_STRING, HFSTS_DEC_OPSTATE, hfsts_sysctl_decode,
	    "A", "ME Current Operation State (from HFS1, live)", 1 },
	{ "opmode_str", CTLTYPE_STRING, HFSTS_DEC_OPMODE, hfsts_sysctl_decode,
	    "A", "ME Current Operation Mode (from HFS1, live)", 1 },
	{ "error", CTLTYPE_STRING, HFSTS_DEC_ERROR, hfsts_sysctl_decode, "A",
	    "ME Error Code (from HFS1, live)", 1 },
	{ "summary", CTLTYPE_STRING, 0, hfsts_sysctl_summary, "A",
	    "One-word ME state summary (live)", 1 },
};

static int
hfsts_probe(device_t dev)
{
	const struct hfsts_device *id;

	if (pci_get_vendor(dev) != VENDORID_INTEL)
		return (ENXIO);

	id = hfsts_find_device(dev);
	if (id == NULL)
		return (ENXIO);
	device_set_desc(dev, id->name);
	return (BUS_PROBE_GENERIC);
}

static int
hfsts_attach(device_t dev)
{
	const struct hfsts_device *id;
	struct hfsts_softc *sc;
	struct sysctl_oid_list *children;
	const char *summary;
	uint32_t hfs1;
	uint32_t opmode;
	uint32_t cws, opstate, error, mfg_mode, fpt_bad, fw_init_cmpl;
	uint32_t bup_fail, update_inprog, boot_opt;
	u_int i;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;
	id = hfsts_find_device(dev);
	if (id == NULL)
		return (ENXIO);
	sc->sc_hfs_count = id->hfs_count;

	/* One-shot read for the attach-time log line only; sysctls re-read. */
	hfs1 = hfsts_read_hfs(dev, 0);
	opmode = (hfs1 & HFSTS_HFS_1_OPMODE_MSK) >> HFSTS_HFS_1_OPMODE_SHIFT;
	cws = hfs1 & HFSTS_HFS_1_CWS_MSK;
	opstate = (hfs1 & HFSTS_HFS_1_OPSTATE_MSK) >> HFSTS_HFS_1_OPSTATE_SHIFT;
	error = (hfs1 & HFSTS_HFS_1_ERROR_MSK) >> HFSTS_HFS_1_ERROR_SHIFT;
	mfg_mode = (hfs1 & HFSTS_HFS_1_MFG_MODE_MSK) != 0;
	fpt_bad = (hfs1 & HFSTS_HFS_1_FPT_BAD_MSK) != 0;
	fw_init_cmpl = (hfs1 & HFSTS_HFS_1_FW_INIT_CMPL_MSK) != 0;
	bup_fail = (hfs1 & HFSTS_HFS_1_BUP_FAIL_MSK) != 0;
	update_inprog = (hfs1 & HFSTS_HFS_1_UPDATE_INPROG_MSK) != 0;
	boot_opt = (hfs1 & HFSTS_HFS_1_BOOT_OPT_MSK) != 0;
	summary = hfsts_state_summary(cws, opmode, error, mfg_mode, fpt_bad,
	    bup_fail, fw_init_cmpl, update_inprog);

	if (bootverbose) {
		device_printf(dev,
		    "state=%s opstate=\"%s\" opmode=\"%s\" error=\"%s\" "
		    "mfg_mode=%u fpt_bad=%u fw_init_complete=%u "
		    "update_in_progress=%u boot_options_present=%u\n",
		    HFSTS_DECODE(hfsts_cws_values, cws),
		    HFSTS_DECODE(hfsts_opstate_values, opstate),
		    HFSTS_DECODE(hfsts_opmode_values, opmode),
		    HFSTS_DECODE(hfsts_error_values, error),
		    mfg_mode, fpt_bad, fw_init_cmpl, update_inprog, boot_opt);
	}

	device_printf(dev, "ME state: %s\n", summary);

	sc->sc_sysctlctx = device_get_sysctl_ctx(dev);
	sc->sc_sysctlnode = device_get_sysctl_tree(dev);
	children = SYSCTL_CHILDREN(sc->sc_sysctlnode);

	for (i = 0; i < nitems(hfsts_sysctls); i++) {
		const struct hfsts_sysctl_def *d = &hfsts_sysctls[i];

		if (sc->sc_hfs_count < d->min_hfs)
			continue;
		sysctl_add_oid(sc->sc_sysctlctx, children, OID_AUTO, d->name,
		    d->ctltype | CTLFLAG_RD | CTLFLAG_MPSAFE, sc, d->arg2,
		    d->handler, d->fmt, __DESCR(d->descr), NULL);
	}

	return (0);
}

static int
hfsts_detach(device_t dev)
{

	return (0);
}

static device_method_t hfsts_methods[] = {
	DEVMETHOD(device_probe,	hfsts_probe),
	DEVMETHOD(device_attach,	hfsts_attach),
	DEVMETHOD(device_detach,	hfsts_detach),
	DEVMETHOD_END
};

static driver_t hfsts_driver = {
	"hfsts",
	hfsts_methods,
	sizeof(struct hfsts_softc)
};

DRIVER_MODULE(hfsts, pci, hfsts_driver, 0, 0);
MODULE_VERSION(hfsts, 1);
MODULE_DEPEND(hfsts, pci, 1, 1, 1);
