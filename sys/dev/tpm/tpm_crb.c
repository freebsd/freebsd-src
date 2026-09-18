/*-
 * Copyright (c) 2018 Stormshield.
 * Copyright (c) 2018 Semihalf.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include "tpm20.h"

/*
 * PTP locality registers, relative to the start of the ACPI memory resource.
 * TCG PC Client PTP 1.03, section 5.5.3.1, Table 23.
 */
#define	TPM_LOC_STATE			0x0
#define	TPM_LOC_CTRL			0x8
#define	TPM_LOC_STS			0xC
#define	TPM_CRB_INTF_ID			0x30
#define	TPM_CRB_CTRL_EXT		0x38
#define	TPM_CRB_LOCALITY_SIZE		0x40

/* Relative to the ControlAddress in the TPM2 ACPI table, not the resource. */
#define	TPM_CRB_CTRL_REQ		0x00
#define	TPM_CRB_CTRL_STS		0x04
#define	TPM_CRB_CTRL_CANCEL		0x08
#define	TPM_CRB_CTRL_START		0x0C
#define	TPM_CRB_INT_ENABLE		0x10
#define	TPM_CRB_INT_STS			0x14
#define	TPM_CRB_CTRL_CMD_SIZE		0x18
#define	TPM_CRB_CTRL_CMD_LADDR		0x1C
#define	TPM_CRB_CTRL_CMD_HADDR		0x20
#define	TPM_CRB_CTRL_RSP_SIZE		0x24
#define	TPM_CRB_CTRL_RSP_ADDR		0x28
#define	TPM_CRB_CTRL_SIZE		0x30

#define	TPM_LOC_STATE_ESTB		BIT(0)
#define	TPM_LOC_STATE_ASSIGNED		BIT(1)
#define	TPM_LOC_STATE_ACTIVE_MASK	0x9C
#define	TPM_LOC_STATE_VALID		BIT(7)

#define	TPM_CRB_INTF_ID_TYPE_CRB	0x1
#define	TPM_CRB_INTF_ID_TYPE		0x7

#define	TPM_LOC_CTRL_REQUEST		BIT(0)
#define	TPM_LOC_CTRL_RELINQUISH		BIT(1)

#define	TPM_CRB_CTRL_REQ_GO_READY	BIT(0)
#define	TPM_CRB_CTRL_REQ_GO_IDLE	BIT(1)

#define	TPM_CRB_CTRL_STS_ERR_BIT	BIT(0)
#define	TPM_CRB_CTRL_STS_IDLE_BIT	BIT(1)

#define	TPM_CRB_CTRL_CANCEL_CMD		0x1
#define	TPM_CRB_CTRL_CANCEL_CLEAR	0x0

#define	TPM_CRB_CTRL_START_CMD		BIT(0)

#define	TPM_CRB_INT_ENABLE_BIT		BIT(31)

struct tpmcrb_sc;
/* Attach/detach callbacks */
typedef bool (sm_attach_t)(struct tpmcrb_sc *, void *, size_t);
typedef void (sm_detach_t)(struct tpmcrb_sc *);
/* State change notification (timeout == 0 for 'no timeout') */
typedef bool (sm_statechange_t)(struct tpmcrb_sc *, int);

struct tpmcrb_sm_cfg {
	sm_attach_t		*sm_attach;
	sm_detach_t		*sm_detach;
	sm_statechange_t	*sm_statechange;
	sm_statechange_t	*sm_cmdready;
};

static sm_attach_t		pluton_attach;
static sm_detach_t		pluton_detach;
static sm_statechange_t		pluton_doorbell;

static const struct tpmcrb_sm_cfg_map {
	int				acpi_sm;
	const char			*desc;
	const struct tpmcrb_sm_cfg	sm_cfg;
} tpmcrb_sm_cfg_map[] = {
	{
		.acpi_sm = TPM2_START_METHOD_CRB,
		.desc = "Trusted Platform Module 2.0, CRB mode",
		.sm_cfg = { NULL },	/* No notifications required */
	},
	{
		.acpi_sm = ACPI_TPM2_COMMAND_BUFFER_WITH_PLUTON,
		.desc = "Trusted Platform Module 2.0, CRB mode (Pluton)",
		.sm_cfg = {
			.sm_attach = &pluton_attach,
			.sm_detach = &pluton_detach,
			.sm_statechange = &pluton_doorbell,
			.sm_cmdready = &pluton_doorbell,
		},
	},
};

struct tpmcrb_mapping {
	bus_space_handle_t	bsh;
	bus_size_t		size;
	bool			mapped;	/* Owns a mapping outside mem_res. */
};

struct tpmcrb_sc {
	struct tpm_sc	base;
	const struct tpmcrb_sm_cfg	*sm_cfg;
	union {
		/*
		 * StartMethod data.  The Pluton start/reply mailbox
		 * registers may live outside of the _CRS window, so they get
		 * their own bus_space mapping.
		 */
		struct {
			bus_space_tag_t		bst;
			bus_space_handle_t	start_bsh;
			bus_space_handle_t	reply_bsh;
		} pluton;
	};
	bus_space_tag_t	bst;
	struct tpmcrb_mapping ctrl;
	struct tpmcrb_mapping cmd;
	struct tpmcrb_mapping rsp;
	bool		has_locality;
};

int tpmcrb_transmit(device_t dev, struct tpm_priv *priv, size_t size);

static int tpmcrb_acpi_probe(device_t dev);
static int tpmcrb_attach(device_t dev);
static int tpmcrb_detach(device_t dev);

static bool tpmcrb_ctrl_wait(struct tpmcrb_sc *crb_sc, bus_size_t off,
    uint32_t mask, uint32_t val, int32_t timeout);
static bool tpmcrb_request_locality(struct tpmcrb_sc *crb_sc, int locality);
static void tpmcrb_relinquish_locality(struct tpmcrb_sc *crb_sc);
static bool tpmcrb_cancel_cmd(struct tpmcrb_sc *crb_sc);
static bool tpmcrb_state_idle(struct tpmcrb_sc *crb_sc, bool wait);
static bool tpmcrb_state_ready(struct tpmcrb_sc *crb_sc, bool wait);

/* All control-register offsets are relative to the mapped control area. */
static uint32_t
tpmcrb_ctrl_read_4(struct tpmcrb_sc *crb_sc, bus_size_t off)
{

	return (bus_space_read_4(crb_sc->bst, crb_sc->ctrl.bsh, off));
}

static uint64_t
tpmcrb_ctrl_read_8(struct tpmcrb_sc *crb_sc, bus_size_t off)
{
#ifdef __i386__
	uint64_t val;

	val = tpmcrb_ctrl_read_4(crb_sc, off);
	val |= (uint64_t)tpmcrb_ctrl_read_4(crb_sc, off + 4) << 32;
	return (val);
#else

	return (bus_space_read_8(crb_sc->bst, crb_sc->ctrl.bsh, off));
#endif
}

static void
tpmcrb_ctrl_write_4(struct tpmcrb_sc *crb_sc, bus_size_t off, uint32_t val)
{

	bus_space_write_4(crb_sc->bst, crb_sc->ctrl.bsh, off, val);
}

static void
tpmcrb_ctrl_write_barrier(struct tpmcrb_sc *crb_sc, bus_size_t off,
    bus_size_t length)
{

	bus_space_barrier(crb_sc->bst, crb_sc->ctrl.bsh, off, length,
	    BUS_SPACE_BARRIER_WRITE);
}

char *tpmcrb_ids[] = {"MSFT0101", NULL};

static const struct tpmcrb_sm_cfg_map *
tpmcrb_acpi_startmethod_cfg(int method)
{
	const struct tpmcrb_sm_cfg_map *entry;

	for (size_t i = 0; i < nitems(tpmcrb_sm_cfg_map); i++) {
		entry = &tpmcrb_sm_cfg_map[i];

		if (method == entry->acpi_sm)
			return (entry);
	}

	return (NULL);
}

static int
tpmcrb_acpi_probe(device_t dev)
{
	int err, smethod;
	const struct tpmcrb_sm_cfg_map *sm_cfg_map;
	ACPI_TABLE_TPM23 *tbl;
	ACPI_STATUS status;

	err = ACPI_ID_PROBE(device_get_parent(dev), dev, tpmcrb_ids, NULL);
	if (err > 0)
		return (err);
	/*Find TPM2 Header*/
	status = AcpiGetTable(ACPI_SIG_TPM2, 1, (ACPI_TABLE_HEADER **) &tbl);
	if (ACPI_FAILURE(status))
		return (ENXIO);
	if (tbl->Header.Length < sizeof(*tbl)) {
		AcpiPutTable((ACPI_TABLE_HEADER *)tbl);
		return (ENXIO);
	}

	smethod = tbl->StartMethod;
	AcpiPutTable((ACPI_TABLE_HEADER *)tbl);

	sm_cfg_map = tpmcrb_acpi_startmethod_cfg(smethod);
	if (sm_cfg_map == NULL)
		return (ENXIO);

	device_set_desc(dev, sm_cfg_map->desc);
	return (0);
}

/* Reuse the resource mapping when possible; buffers may be in reserved RAM. */
static int
tpmcrb_map(struct tpmcrb_sc *crb_sc, struct tpmcrb_mapping *map,
    uint64_t addr, bus_size_t size)
{
	struct resource *res = crb_sc->base.mem_res;
	rman_res_t start, end;
	int error;

	if (addr == 0 || size == 0 || addr > BUS_SPACE_MAXADDR ||
	    size - 1 > BUS_SPACE_MAXADDR - addr)
		return (EINVAL);
	start = rman_get_start(res);
	end = rman_get_end(res);
	if (addr <= end && addr + size - 1 >= start) {
		if (addr < start || addr + size - 1 > end)
			return (EINVAL);
		error = bus_space_subregion(crb_sc->bst, rman_get_bushandle(res),
		    addr - start, size, &map->bsh);
	} else {
		error = bus_space_map(crb_sc->bst, addr, size, 0, &map->bsh);
		if (error == 0)
			map->mapped = true;
	}
	if (error == 0)
		map->size = size;
	return (error);
}

static void
tpmcrb_unmap(struct tpmcrb_sc *crb_sc, struct tpmcrb_mapping *map)
{

	if (map->mapped)
		bus_space_unmap(crb_sc->bst, map->bsh, map->size);
	memset(map, 0, sizeof(*map));
}

static int
tpmcrb_map_buffer(struct tpmcrb_sc *crb_sc, struct tpmcrb_mapping *map,
    uint64_t addr, uint32_t size)
{
	struct resource *res = crb_sc->base.mem_res;
	rman_res_t start, end;

	/* The character device never transfers more than TPM_BUFSIZE bytes. */
	size = MIN(size, TPM_BUFSIZE);
	start = rman_get_start(res);
	end = rman_get_end(res);
	/* Firmware can report a buffer larger than its ACPI memory resource. */
	if (addr >= start && addr <= end && end - addr + 1 < size)
		size = end - addr + 1;
	if (size < TPM_HEADER_SIZE)
		return (EINVAL);
	return (tpmcrb_map(crb_sc, map, addr, size));
}

static bool
tpmcrb_attach_acpi(struct tpmcrb_sc *crb_sc)
{
	const struct tpmcrb_sm_cfg_map *sm_cfg_map;
	const struct tpmcrb_sm_cfg *sm_cfg;
	ACPI_TABLE_TPM23 *tbl;
	void *smdata;
	ACPI_STATUS status;
	bool ret;

	status = AcpiGetTable(ACPI_SIG_TPM2, 1, (ACPI_TABLE_HEADER **)(void **)&tbl);
	if (ACPI_FAILURE(status))
		return (false);
	if (tbl->Header.Length < sizeof(*tbl)) {
		AcpiPutTable((ACPI_TABLE_HEADER *)tbl);
		return (false);
	}

	/*
	 * TCG ACPI 1.5, Table 7: ControlAddress points at CTRL_REQ.  Only
	 * the PTP layout (PTP 1.03, Table 23) has locality registers in the
	 * preceding 64 bytes.  A control-area-only interface has no such
	 * registers; do not interpret its Cancel register as LOC_CTRL.
	 */
	if ((tbl->ControlAddress & 7) != 0 ||
	    tpmcrb_map(crb_sc, &crb_sc->ctrl, tbl->ControlAddress,
	    TPM_CRB_CTRL_SIZE) != 0) {
		device_printf(crb_sc->base.dev, "Cannot map CRB control area\n");
		AcpiPutTable((ACPI_TABLE_HEADER *)tbl);
		return (false);
	}
	crb_sc->has_locality = tbl->ControlAddress >= TPM_CRB_LOCALITY_SIZE &&
	    tbl->ControlAddress - TPM_CRB_LOCALITY_SIZE ==
	    rman_get_start(crb_sc->base.mem_res);

	sm_cfg_map = tpmcrb_acpi_startmethod_cfg(tbl->StartMethod);
	MPASS(sm_cfg_map != NULL);
	sm_cfg = &sm_cfg_map->sm_cfg;

	crb_sc->sm_cfg = sm_cfg;
	smdata = tbl + 1;
	if (sm_cfg->sm_attach != NULL) {
		ret = (*sm_cfg->sm_attach)(crb_sc, smdata,
		    tbl->Header.Length - sizeof(*tbl));
	} else {
		ret = true;
	}

	AcpiPutTable((ACPI_TABLE_HEADER *)tbl);
	return (ret);
}

static int
tpmcrb_attach(device_t dev)
{
	struct tpmcrb_sc *crb_sc;
	struct tpm_sc *sc;
	uint64_t cmd_addr, rsp_addr;
	uint32_t cmd_size, rsp_size;
	int result;

	crb_sc = device_get_softc(dev);
	sc = &crb_sc->base;
	sc->dev = dev;

	sx_init(&sc->dev_lock, "TPM driver lock");

	sc->mem_rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &sc->mem_rid,
					     RF_ACTIVE);
	if (sc->mem_res == NULL) {
		tpmcrb_detach(dev);
		return (ENXIO);
	}
	crb_sc->bst = rman_get_bustag(sc->mem_res);

	if (!tpmcrb_attach_acpi(crb_sc)) {
		tpmcrb_detach(dev);
		return (ENXIO);
	}

	if (!tpmcrb_request_locality(crb_sc, 0)) {
		device_printf(dev, "Failed to obtain locality\n");
		tpmcrb_detach(dev);
		return (ENXIO);
	}
	if (!tpmcrb_state_ready(crb_sc, true)) {
		device_printf(dev, "Failed to transition to ready state\n");
		result = ENXIO;
		goto out;
	}

	/*
	 * Disable all interrupts for now, since I don't have a device that
	 * works in CRB mode and supports them.
	 */
	tpmcrb_ctrl_write_4(crb_sc, TPM_CRB_INT_ENABLE,
	    tpmcrb_ctrl_read_4(crb_sc, TPM_CRB_INT_ENABLE) &
	    ~TPM_CRB_INT_ENABLE_BIT);
	sc->interrupts = false;

	/*
	 * Read addresses of Tx/Rx buffers and their sizes. Note that they
	 * can be implemented by a single buffer.  PTP 1.03, section 5.5.3.1
	 * requires naturally aligned accesses: the command address needs two
	 * 32-bit reads, whereas the response address permits a 64-bit read.
	 */
	rsp_addr = tpmcrb_ctrl_read_8(crb_sc, TPM_CRB_CTRL_RSP_ADDR);
	cmd_addr = tpmcrb_ctrl_read_4(crb_sc, TPM_CRB_CTRL_CMD_LADDR);
	cmd_addr |= (uint64_t)tpmcrb_ctrl_read_4(crb_sc,
	    TPM_CRB_CTRL_CMD_HADDR) << 32;
	cmd_size = tpmcrb_ctrl_read_4(crb_sc, TPM_CRB_CTRL_CMD_SIZE);
	rsp_size = tpmcrb_ctrl_read_4(crb_sc, TPM_CRB_CTRL_RSP_SIZE);

	/* PTP 1.03, Table 23 requires equal sizes for a shared buffer. */
	if (cmd_size < TPM_HEADER_SIZE || rsp_size < TPM_HEADER_SIZE ||
	    (cmd_addr == rsp_addr && cmd_size != rsp_size)) {
		device_printf(dev, "Invalid CRB buffer sizes (%u, %u)\n",
		    cmd_size, rsp_size);
		result = ENXIO;
		goto out;
	}
	result = tpmcrb_map_buffer(crb_sc, &crb_sc->cmd, cmd_addr, cmd_size);
	if (result != 0) {
		device_printf(dev, "Cannot map CRB command buffer\n");
		goto out;
	}
	if (cmd_addr == rsp_addr) {
		crb_sc->rsp = crb_sc->cmd;
		crb_sc->rsp.mapped = false;
	} else {
		result = tpmcrb_map_buffer(crb_sc, &crb_sc->rsp, rsp_addr, rsp_size);
		if (result != 0)
			device_printf(dev, "Cannot map CRB response buffer\n");
	}

out:
	if (!tpmcrb_state_idle(crb_sc, true)) {
		device_printf(dev, "Failed to transition to idle state\n");
		if (result == 0)
			result = ENXIO;
	}
	tpmcrb_relinquish_locality(crb_sc);
	if (result == 0)
		result = tpm20_init(sc);

	if (result != 0)
		tpmcrb_detach(dev);

	return (result);
}

static int
tpmcrb_detach(device_t dev)
{
	struct tpmcrb_sc *crb_sc;
	struct tpm_sc *sc;

	crb_sc = device_get_softc(dev);
	sc = &crb_sc->base;
	tpm20_release(sc);

	if (crb_sc->sm_cfg != NULL && crb_sc->sm_cfg->sm_detach != NULL)
		(*crb_sc->sm_cfg->sm_detach)(crb_sc);
	crb_sc->sm_cfg = NULL;
	tpmcrb_unmap(crb_sc, &crb_sc->rsp);
	tpmcrb_unmap(crb_sc, &crb_sc->cmd);
	tpmcrb_unmap(crb_sc, &crb_sc->ctrl);

	if (sc->mem_res != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY,
		    sc->mem_rid, sc->mem_res);
		sc->mem_res = NULL;
	}

	return (0);
}

static bool
tpmcrb_ctrl_wait(struct tpmcrb_sc *crb_sc, bus_size_t off, uint32_t mask,
    uint32_t val, int32_t timeout)
{

	/* Check for condition */
	if ((tpmcrb_ctrl_read_4(crb_sc, off) & mask) == val)
		return (true);

	while (timeout > 0) {
		if ((tpmcrb_ctrl_read_4(crb_sc, off) & mask) == val)
			return (true);

		pause("TPM in polling mode", 1);
		timeout -= tick;
	}
	return (false);
}

static bool
tpmcrb_request_locality(struct tpmcrb_sc *crb_sc, int locality)
{
	struct resource *res = crb_sc->base.mem_res;
	uint32_t mask;
	int timeout;

	/* Currently we only support Locality 0 */
	if (locality != 0)
		return (false);
	if (!crb_sc->has_locality)
		return (true);

	mask = TPM_LOC_STATE_VALID | TPM_LOC_STATE_ASSIGNED;

	bus_write_4(res, TPM_LOC_CTRL, TPM_LOC_CTRL_REQUEST);
	for (timeout = TPM_TIMEOUT_C; timeout > 0; timeout -= tick) {
		if ((bus_read_4(res, TPM_LOC_STATE) & mask) == mask)
			return (true);
		pause("TPM locality", 1);
	}
	return (false);
}

static void
tpmcrb_relinquish_locality(struct tpmcrb_sc *crb_sc)
{

	if (crb_sc->has_locality)
		bus_write_4(crb_sc->base.mem_res, TPM_LOC_CTRL,
		    TPM_LOC_CTRL_RELINQUISH);
}

static bool
tpmcrb_cancel_cmd(struct tpmcrb_sc *crb_sc)
{
	uint32_t mask = ~0;

	tpmcrb_ctrl_write_4(crb_sc, TPM_CRB_CTRL_CANCEL, TPM_CRB_CTRL_CANCEL_CMD);
	if (!tpmcrb_ctrl_wait(crb_sc, TPM_CRB_CTRL_START,
		    mask, ~mask, TPM_TIMEOUT_B)) {
		device_printf(crb_sc->base.dev,
		    "Device failed to cancel command\n");
		return (false);
	}

	tpmcrb_ctrl_write_4(crb_sc, TPM_CRB_CTRL_CANCEL, TPM_CRB_CTRL_CANCEL_CLEAR);
	return (true);
}

static bool
tpmcrb_notify_cmdready(struct tpmcrb_sc *crb_sc, int timeout)
{
	sm_statechange_t *cmdready_fn;

	cmdready_fn = crb_sc->sm_cfg->sm_cmdready;
	if (cmdready_fn == NULL)
		return (true);

	return ((*cmdready_fn)(crb_sc, timeout));
}

static bool
tpmcrb_notify_state_changing(struct tpmcrb_sc *crb_sc, int timeout)
{
	sm_statechange_t *statechange_fn;

	statechange_fn = crb_sc->sm_cfg->sm_statechange;
	if (statechange_fn == NULL)
		return (true);

	return ((*statechange_fn)(crb_sc, timeout));
}

static bool
tpmcrb_state_idle(struct tpmcrb_sc *crb_sc, bool wait)
{
	int mask, timeout;

	timeout = wait ? TPM_TIMEOUT_C : 0;

	tpmcrb_ctrl_write_4(crb_sc, TPM_CRB_CTRL_REQ, TPM_CRB_CTRL_REQ_GO_IDLE);

	if (!tpmcrb_notify_state_changing(crb_sc, timeout))
		return (false);

	if (timeout > 0) {
		/* PTP 1.03, 5.5.3.4: the TPM clears the request to acknowledge. */
		mask = TPM_CRB_CTRL_REQ_GO_IDLE;
		if (!tpmcrb_ctrl_wait(crb_sc, TPM_CRB_CTRL_REQ, mask, 0,
		    timeout))
			return (false);
	}

	return (true);
}

static bool
tpmcrb_state_ready(struct tpmcrb_sc *crb_sc, bool wait)
{
	int mask, timeout;

	timeout = wait ? TPM_TIMEOUT_C : 0;

	tpmcrb_ctrl_write_4(crb_sc, TPM_CRB_CTRL_REQ, TPM_CRB_CTRL_REQ_GO_READY);

	if (!tpmcrb_notify_state_changing(crb_sc, timeout))
		return (false);

	if (timeout > 0) {
		mask = TPM_CRB_CTRL_REQ_GO_READY;
		if (!tpmcrb_ctrl_wait(crb_sc, TPM_CRB_CTRL_REQ, mask, 0,
		    timeout))
			return (false);
	}

	return (true);
}

int
tpmcrb_transmit(device_t dev, struct tpm_priv *priv, size_t length)
{
	struct tpmcrb_sc *crb_sc;
	uint32_t mask, curr_cmd;
	int bytes_available, error, timeout;
	bool command_started, locality;

	crb_sc = device_get_softc(dev);

	sx_assert(&crb_sc->base.dev_lock, SA_XLOCKED);
	command_started = false;
	locality = false;

	if (length > crb_sc->cmd.size) {
		device_printf(dev,
		    "Requested transfer is bigger than buffer size\n");
		return (E2BIG);
	}

	if (!tpmcrb_request_locality(crb_sc, 0)) {
		device_printf(dev,
		    "Failed to obtain locality\n");
		return (EIO);
	}
	locality = true;

	/*
	 * Only check for the error once we own the locality: some
	 * implementations do not return meaningful values in the control area
	 * before then.  An AMD Pluton fTPM using the plain CRB start method
	 * reads all-ones, which looks like a permanently stuck tpmSts.
	 */
	if (tpmcrb_ctrl_read_4(crb_sc, TPM_CRB_CTRL_STS) &
	    TPM_CRB_CTRL_STS_ERR_BIT) {
		device_printf(dev,
		    "Device has Error bit set\n");
		error = EIO;
		goto out;
	}
	/* Clear cancellation bit */
	tpmcrb_ctrl_write_4(crb_sc, TPM_CRB_CTRL_CANCEL, TPM_CRB_CTRL_CANCEL_CLEAR);

	/* Switch device to idle state if necessary */
	if (!(tpmcrb_ctrl_read_4(crb_sc, TPM_CRB_CTRL_STS) &
	    TPM_CRB_CTRL_STS_IDLE_BIT)) {
		if (!tpmcrb_state_idle(crb_sc, true)) {
			device_printf(dev,
			    "Failed to transition to idle state\n");
			error = EIO;
			goto out;
		}
	}

	/* Switch to ready state */
	if (!tpmcrb_state_ready(crb_sc, true)) {
		device_printf(dev,
		    "Failed to transition to ready state\n");
		error = EIO;
		goto out;
	}

	/*
	 * Calculate timeout for current command.
	 * Command code is passed in bytes 6-10.
	 */
	curr_cmd = be32toh(*(uint32_t *) (&priv->buf[6]));
	timeout = tpm20_get_timeout(curr_cmd);

	/* Send command and tell device to process it. */
	bus_space_write_region_stream_1(crb_sc->bst, crb_sc->cmd.bsh, 0,
	    priv->buf, length);
	bus_space_barrier(crb_sc->bst, crb_sc->cmd.bsh, 0, length,
	    BUS_SPACE_BARRIER_WRITE);

	tpmcrb_ctrl_write_4(crb_sc, TPM_CRB_CTRL_START, TPM_CRB_CTRL_START_CMD);
	tpmcrb_ctrl_write_barrier(crb_sc, TPM_CRB_CTRL_START, 4);
	command_started = true;

	if (!tpmcrb_notify_cmdready(crb_sc, timeout)) {
		device_printf(dev,
		    "Timeout while waiting for device to ready\n");
		if (!tpmcrb_cancel_cmd(crb_sc)) {
			error = EIO;
			goto out;
		}
	}

	mask = ~0;
	if (!tpmcrb_ctrl_wait(crb_sc, TPM_CRB_CTRL_START, mask, ~mask, timeout)) {
		device_printf(dev,
		    "Timeout while waiting for device to process cmd\n");
		if (!tpmcrb_cancel_cmd(crb_sc)) {
			error = EIO;
			goto out;
		}
	}

	/* A fatal error during execution may leave no valid response. */
	if (tpmcrb_ctrl_read_4(crb_sc, TPM_CRB_CTRL_STS) &
	    TPM_CRB_CTRL_STS_ERR_BIT) {
		device_printf(dev, "Device has Error bit set after command\n");
		error = EIO;
		goto out;
	}

	/* Read response header. Length is passed in bytes 2 - 6. */
	bus_space_barrier(crb_sc->bst, crb_sc->rsp.bsh, 0, crb_sc->rsp.size,
	    BUS_SPACE_BARRIER_READ);
	bus_space_read_region_stream_1(crb_sc->bst, crb_sc->rsp.bsh, 0,
	    priv->buf, TPM_HEADER_SIZE);
	bytes_available = be32toh(*(uint32_t *) (&priv->buf[2]));

	if (bytes_available < TPM_HEADER_SIZE ||
	    bytes_available > crb_sc->rsp.size) {
		device_printf(dev,
		    "Incorrect response size: %d\n",
		    bytes_available);
		error = EIO;
		goto out;
	}

	bus_space_read_region_stream_1(crb_sc->bst, crb_sc->rsp.bsh,
	    TPM_HEADER_SIZE, &priv->buf[TPM_HEADER_SIZE],
	    bytes_available - TPM_HEADER_SIZE);

	priv->offset = 0;
	priv->len = bytes_available;
	error = 0;

out:
	if (locality) {
		if (error != 0 && command_started &&
		    (tpmcrb_ctrl_read_4(crb_sc, TPM_CRB_CTRL_START) &
		    TPM_CRB_CTRL_START_CMD) != 0)
			(void)tpmcrb_cancel_cmd(crb_sc);
		if (!tpmcrb_state_idle(crb_sc, false)) {
			device_printf(dev,
			    "Failed to transition to idle state post-send\n");
			if (error == 0)
				error = EIO;
		}
		tpmcrb_relinquish_locality(crb_sc);
	}

	return (error);
}

/* StartMethod Implementation Details */

/** Pluton **/
struct tpmcrb_startmethod_pluton {
	uint64_t		sm_startaddr;
	uint64_t		sm_replyaddr;
};

static bool
pluton_attach(struct tpmcrb_sc *crb_sc, void *smdataregion, size_t datasz)
{
	struct tpmcrb_startmethod_pluton *smdata;
	struct tpm_sc *sc;
	bus_space_tag_t bst;

	if (datasz < sizeof(*smdata))
		return (false);

	smdata = smdataregion;
	sc = &crb_sc->base;

	/*
	 * The start/reply mailbox registers are not necessarily part of
	 * the CRB register window described by _CRS, so we map them
	 * independently.
	 */
	bst = rman_get_bustag(sc->mem_res);
	if (bus_space_map(bst, smdata->sm_startaddr, sizeof(uint32_t), 0,
	    &crb_sc->pluton.start_bsh) != 0) {
		device_printf(sc->dev,
		    "Failed to map Pluton start register at %#jx\n",
		    (uintmax_t)smdata->sm_startaddr);
		return (false);
	}

	if (bus_space_map(bst, smdata->sm_replyaddr, sizeof(uint32_t), 0,
	    &crb_sc->pluton.reply_bsh) != 0) {
		device_printf(sc->dev,
		    "Failed to map Pluton reply register at %#jx\n",
		    (uintmax_t)smdata->sm_replyaddr);
		bus_space_unmap(bst, crb_sc->pluton.start_bsh,
		    sizeof(uint32_t));
		return (false);
	}

	crb_sc->pluton.bst = bst;
	return (true);
}

static void
pluton_detach(struct tpmcrb_sc *crb_sc)
{

	if (crb_sc->pluton.bst == 0)
		return;

	bus_space_unmap(crb_sc->pluton.bst, crb_sc->pluton.start_bsh,
	    sizeof(uint32_t));
	bus_space_unmap(crb_sc->pluton.bst, crb_sc->pluton.reply_bsh,
	    sizeof(uint32_t));
}

static bool
pluton_wait_reply(struct tpmcrb_sc *crb_sc, int32_t timeout)
{
	for (;;) {
		/*
		 * Always read at least once, and try one more time after we hit
		 * the timeout.
		 */
		if (bus_space_read_4(crb_sc->pluton.bst,
		    crb_sc->pluton.reply_bsh, 0) == 1)
			return (true);
		else if (timeout <= 0)
			break;

		pause("TPM in polling mode", 1);
		timeout -= tick;
	}

	return (false);
}

static bool
pluton_doorbell(struct tpmcrb_sc *crb_sc, int timeout)
{
	bus_space_write_4(crb_sc->pluton.bst, crb_sc->pluton.start_bsh, 0, 1);
	bus_space_barrier(crb_sc->pluton.bst, crb_sc->pluton.start_bsh, 0,
	    sizeof(uint32_t), BUS_SPACE_BARRIER_WRITE);

	/*
	 * We assume at timeout == 0 that they're not really interested in a
	 * reply immediately.  Otherwise, we poll for a reply at least once,
	 * even with low timeouts.
	 */
	if (timeout <= 0)
		return (true);
	return (pluton_wait_reply(crb_sc, timeout));
}

/* ACPI Driver */
static device_method_t	tpmcrb_methods[] = {
	DEVMETHOD(device_probe,		tpmcrb_acpi_probe),
	DEVMETHOD(device_attach,	tpmcrb_attach),
	DEVMETHOD(device_detach,	tpmcrb_detach),
	DEVMETHOD(device_shutdown,	tpm20_shutdown),
	DEVMETHOD(device_suspend,	tpm20_suspend),
	DEVMETHOD(device_resume,	tpm20_resume),
	DEVMETHOD(tpm_transmit,		tpmcrb_transmit),
	{0, 0}
};

DEFINE_CLASS_0(tpmcrb, tpmcrb_driver, tpmcrb_methods, sizeof(struct tpmcrb_sc));

DRIVER_MODULE(tpmcrb, acpi, tpmcrb_driver, 0, 0);
