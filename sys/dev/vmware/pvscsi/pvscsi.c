/*-
 * Copyright (c) 2018 VMware, Inc.
 *
 * SPDX-License-Identifier: (BSD-2-Clause OR GPL-2.0)
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/queue.h>
#include <sys/rman.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_debug.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/scsi/scsi_message.h>

#include "pvscsi.h"

#define	PVSCSI_DEFAULT_NUM_PAGES_REQ_RING	8
#define	PVSCSI_SENSE_LENGTH			256

MALLOC_DECLARE(M_PVSCSI);
MALLOC_DEFINE(M_PVSCSI, "pvscsi", "PVSCSI memory");

#ifdef PVSCSI_DEBUG_LOGGING
#define	DEBUG_PRINTF(level, dev, fmt, ...)				\
	do {								\
		if (pvscsi_log_level >= (level)) {			\
			device_printf((dev), (fmt), ##__VA_ARGS__);	\
		}							\
	} while(0)
#else
#define DEBUG_PRINTF(level, dev, fmt, ...)
#endif /* PVSCSI_DEBUG_LOGGING */

#define	ccb_pvscsi_hcb	spriv_ptr0
#define	ccb_pvscsi_sc	spriv_ptr1

struct pvscsi_softc;
struct pvscsi_hcb;
struct pvscsi_dma;

static inline uint32_t pvscsi_reg_read(struct pvscsi_softc *sc,
    uint32_t offset);
static inline void pvscsi_reg_write(struct pvscsi_softc *sc, uint32_t offset,
    uint32_t val);
static inline uint32_t pvscsi_read_intr_status(struct pvscsi_softc *sc);
static inline void pvscsi_write_intr_status(struct pvscsi_softc *sc,
    uint32_t val);
static inline void pvscsi_intr_enable(struct pvscsi_softc *sc);
static inline void pvscsi_intr_disable(struct pvscsi_softc *sc);
static void pvscsi_kick_io(struct pvscsi_softc *sc, uint8_t cdb0);
static void pvscsi_write_cmd(struct pvscsi_softc *sc, uint32_t cmd, void *data,
    uint32_t len);
static uint32_t pvscsi_get_max_targets(struct pvscsi_softc *sc);
static int pvscsi_setup_req_call(struct pvscsi_softc *sc, uint32_t enable);
static void pvscsi_setup_rings(struct pvscsi_softc *sc);
static void pvscsi_setup_msg_ring(struct pvscsi_softc *sc);
static int pvscsi_hw_supports_msg(struct pvscsi_softc *sc);

static void pvscsi_timeout(void *arg);
static void pvscsi_freeze(struct pvscsi_softc *sc);
static void pvscsi_adapter_reset(struct pvscsi_softc *sc);
static void pvscsi_bus_reset(struct pvscsi_softc *sc);
static void pvscsi_device_reset(struct pvscsi_softc *sc, uint32_t target);
static void pvscsi_abort(struct pvscsi_softc *sc, uint32_t target,
    union ccb *ccb);

static void pvscsi_process_completion(struct pvscsi_softc *sc,
    struct pvscsi_ring_cmp_desc *e);
static void pvscsi_process_cmp_ring(struct pvscsi_softc *sc);
static void pvscsi_process_msg(struct pvscsi_softc *sc,
    struct pvscsi_ring_msg_desc *e);
static void pvscsi_process_msg_ring(struct pvscsi_softc *sc);

static void pvscsi_intr_locked(struct pvscsi_softc *sc);
static void pvscsi_intr(void *xsc);
static void pvscsi_poll(struct cam_sim *sim);

static void pvscsi_execute_ccb(void *arg, bus_dma_segment_t *segs, int nseg,
    int error);
static void pvscsi_action(struct cam_sim *sim, union ccb *ccb);

static inline uint64_t pvscsi_hcb_to_context(struct pvscsi_softc *sc,
    struct pvscsi_hcb *hcb);
static inline struct pvscsi_hcb* pvscsi_context_to_hcb(struct pvscsi_softc *sc,
    uint64_t context);
static struct pvscsi_hcb * pvscsi_hcb_get(struct pvscsi_softc *sc);
static void pvscsi_hcb_put(struct pvscsi_softc *sc, struct pvscsi_hcb *hcb);

static void pvscsi_dma_cb(void *arg, bus_dma_segment_t *segs, int nseg,
    int error);
static void pvscsi_dma_free(struct pvscsi_softc *sc, struct pvscsi_dma *dma);
static int pvscsi_dma_alloc(struct pvscsi_softc *sc, struct pvscsi_dma *dma,
    bus_size_t size, bus_size_t alignment);
static int pvscsi_dma_alloc_ppns(struct pvscsi_softc *sc,
    struct pvscsi_dma *dma, uint64_t *ppn_list, uint32_t num_pages);
static void pvscsi_dma_free_per_hcb(struct pvscsi_softc *sc,
    uint32_t hcbs_allocated);
static int pvscsi_dma_alloc_per_hcb(struct pvscsi_softc *sc);
static void pvscsi_free_rings(struct pvscsi_softc *sc);
static int pvscsi_allocate_rings(struct pvscsi_softc *sc);
static void pvscsi_free_interrupts(struct pvscsi_softc *sc);
static int pvscsi_setup_interrupts(struct pvscsi_softc *sc);
static void pvscsi_free_all(struct pvscsi_softc *sc);

static int pvscsi_attach(device_t dev);
static int pvscsi_detach(device_t dev);
static int pvscsi_probe(device_t dev);
static int pvscsi_shutdown(device_t dev);
static int pvscsi_get_tunable(struct pvscsi_softc *sc, char *name, int value);

#ifdef PVSCSI_DEBUG_LOGGING
static int pvscsi_log_level = 0;
static SYSCTL_NODE(_hw, OID_AUTO, pvscsi, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "PVSCSI driver parameters");
SYSCTL_INT(_hw_pvscsi, OID_AUTO, log_level, CTLFLAG_RWTUN, &pvscsi_log_level,
    0, "PVSCSI debug log level");
#endif

static int pvscsi_request_ring_pages = 0;
TUNABLE_INT("hw.pvscsi.request_ring_pages", &pvscsi_request_ring_pages);

static int pvscsi_use_msg = 1;
TUNABLE_INT("hw.pvscsi.use_msg", &pvscsi_use_msg);

static int pvscsi_use_msi = 1;
TUNABLE_INT("hw.pvscsi.use_msi", &pvscsi_use_msi);

static int pvscsi_use_msix = 1;
TUNABLE_INT("hw.pvscsi.use_msix", &pvscsi_use_msix);

static int pvscsi_use_req_call_threshold = 1;
TUNABLE_INT("hw.pvscsi.use_req_call_threshold", &pvscsi_use_req_call_threshold);

static int pvscsi_max_queue_depth = 0;
TUNABLE_INT("hw.pvscsi.max_queue_depth", &pvscsi_max_queue_depth);

struct pvscsi_sg_list {
	struct pvscsi_sg_element sge[PVSCSI_MAX_SG_ENTRIES_PER_SEGMENT];
};

#define	PVSCSI_ABORT_TIMEOUT	2
#define	PVSCSI_RESET_TIMEOUT	10

#define	PVSCSI_HCB_NONE		0
#define	PVSCSI_HCB_ABORT	1
#define	PVSCSI_HCB_DEVICE_RESET	2
#define	PVSCSI_HCB_BUS_RESET	3

struct pvscsi_hcb {
	union ccb			*ccb;
	struct pvscsi_ring_req_desc	*e;
	int				 recovery;
	SLIST_ENTRY(pvscsi_hcb)		 links;

	struct callout			 callout;
	bus_dmamap_t			 dma_map;
	void				*sense_buffer;
	bus_addr_t			 sense_buffer_paddr;
	struct pvscsi_sg_list		*sg_list;
	bus_addr_t			 sg_list_paddr;
};

struct pvscsi_dma
{
	bus_dma_tag_t	 tag;
	bus_dmamap_t	 map;
	void		*vaddr;
	bus_addr_t	 paddr;
	bus_size_t	 size;
};

struct pvscsi_softc {
	device_t		 dev;
	struct mtx		 lock;
	struct cam_sim		*sim;
	struct cam_path		*bus_path;
	int			 frozen;
	struct pvscsi_rings_state	*rings_state;
	struct pvscsi_ring_req_desc	*req_ring;
	struct pvscsi_ring_cmp_desc	*cmp_ring;
	struct pvscsi_ring_msg_desc	*msg_ring;
	uint32_t		 hcb_cnt;
	struct pvscsi_hcb	*hcbs;
	SLIST_HEAD(, pvscsi_hcb)	free_list;
	bus_dma_tag_t		parent_dmat;
	bus_dma_tag_t		buffer_dmat;

	bool		 use_msg;
	uint32_t	 max_targets;
	int		 mm_rid;
	struct resource	*mm_res;
	int		 irq_id;
	struct resource	*irq_res;
	void		*irq_handler;
	int		 use_req_call_threshold;
	int		 use_msi_or_msix;

	uint64_t	rings_state_ppn;
	uint32_t	req_ring_num_pages;
	uint64_t	req_ring_ppn[PVSCSI_MAX_NUM_PAGES_REQ_RING];
	uint32_t	cmp_ring_num_pages;
	uint64_t	cmp_ring_ppn[PVSCSI_MAX_NUM_PAGES_CMP_RING];
	uint32_t	msg_ring_num_pages;
	uint64_t	msg_ring_ppn[PVSCSI_MAX_NUM_PAGES_MSG_RING];

	struct	pvscsi_dma rings_state_dma;
	struct	pvscsi_dma req_ring_dma;
	struct	pvscsi_dma cmp_ring_dma;
	struct	pvscsi_dma msg_ring_dma;

	struct	pvscsi_dma sg_list_dma;
	struct	pvscsi_dma sense_buffer_dma;
};

static int pvscsi_get_tunable(struct pvscsi_softc *sc, char *name, int value)
{
	char cfg[64];

	snprintf(cfg, sizeof(cfg), "hw.pvscsi.%d.%s", device_get_unit(sc->dev),
	    name);
	TUNABLE_INT_FETCH(cfg, &value);

	return (value);
}

static void
pvscsi_freeze(struct pvscsi_softc *sc)
{

	if (!sc->frozen) {
		xpt_freeze_simq(sc->sim, 1);
		sc->frozen = 1;
	}
}

static inline uint32_t
pvscsi_reg_read(struct pvscsi_softc *sc, uint32_t offset)
{

	return (bus_read_4(sc->mm_res, offset));
}

static inline void
pvscsi_reg_write(struct pvscsi_softc *sc, uint32_t offset, uint32_t val)
{

	bus_write_4(sc->mm_res, offset, val);
}

static inline uint32_t
pvscsi_read_intr_status(struct pvscsi_softc *sc)
{

	return (pvscsi_reg_read(sc, PVSCSI_REG_OFFSET_INTR_STATUS));
}

static inline void
pvscsi_write_intr_status(struct pvscsi_softc *sc, uint32_t val)
{

	pvscsi_reg_write(sc, PVSCSI_REG_OFFSET_INTR_STATUS, val);
}

static inline void
pvscsi_intr_enable(struct pvscsi_softc *sc)
{
	uint32_t mask;

	mask = PVSCSI_INTR_CMPL_MASK;
	if (sc->use_msg) {
		mask |= PVSCSI_INTR_MSG_MASK;
	}

	pvscsi_reg_write(sc, PVSCSI_REG_OFFSET_INTR_MASK, mask);
}

static inline void
pvscsi_intr_disable(struct pvscsi_softc *sc)
{

	pvscsi_reg_write(sc, PVSCSI_REG_OFFSET_INTR_MASK, 0);
}

static void
pvscsi_kick_io(struct pvscsi_softc *sc, uint8_t cdb0)
{
	struct pvscsi_rings_state *s;

	if (cdb0 == READ_6  || cdb0 == READ_10  ||
	    cdb0 == READ_12  || cdb0 == READ_16 ||
	    cdb0 == WRITE_6 || cdb0 == WRITE_10 ||
	    cdb0 == WRITE_12 || cdb0 == WRITE_16) {
		s = sc->rings_state;

		if (!sc->use_req_call_threshold ||
		    (s->req_prod_idx - s->req_cons_idx) >=
		     s->req_call_threshold) {
			pvscsi_reg_write(sc, PVSCSI_REG_OFFSET_KICK_RW_IO, 0);
		}
	} else {
		pvscsi_reg_write(sc, PVSCSI_REG_OFFSET_KICK_NON_RW_IO, 0);
	}
}

static void
pvscsi_write_cmd(struct pvscsi_softc *sc, uint32_t cmd, void *data,
		 uint32_t len)
{
	uint32_t *data_ptr;
	int i;

	KASSERT(len % sizeof(uint32_t) == 0,
		("command size not a multiple of 4"));

	data_ptr = data;
	len /= sizeof(uint32_t);

	pvscsi_reg_write(sc, PVSCSI_REG_OFFSET_COMMAND, cmd);
	for (i = 0; i < len; ++i) {
		pvscsi_reg_write(sc, PVSCSI_REG_OFFSET_COMMAND_DATA,
		   data_ptr[i]);
	}
}

static inline uint64_t pvscsi_hcb_to_context(struct pvscsi_softc *sc,
    struct pvscsi_hcb *hcb)
{

	/* Offset by 1 because context must not be 0 */
	return (hcb - sc->hcbs + 1);
}

static inline struct pvscsi_hcb* pvscsi_context_to_hcb(struct pvscsi_softc *sc,
    uint64_t context)
{

	return (sc->hcbs + (context - 1));
}

static struct pvscsi_hcb *
pvscsi_hcb_get(struct pvscsi_softc *sc)
{
	struct pvscsi_hcb *hcb;

	mtx_assert(&sc->lock, MA_OWNED);

	hcb = SLIST_FIRST(&sc->free_list);
	if (hcb) {
		SLIST_REMOVE_HEAD(&sc->free_list, links);
	}

	return (hcb);
}

static void
pvscsi_hcb_put(struct pvscsi_softc *sc, struct pvscsi_hcb *hcb)
{

	mtx_assert(&sc->lock, MA_OWNED);
	hcb->ccb = NULL;
	hcb->e = NULL;
	hcb->recovery = PVSCSI_HCB_NONE;
	SLIST_INSERT_HEAD(&sc->free_list, hcb, links);
}

static uint32_t
pvscsi_get_max_targets(struct pvscsi_softc *sc)
{
	uint32_t max_targets;

	pvscsi_write_cmd(sc, PVSCSI_CMD_GET_MAX_TARGETS, NULL, 0);

	max_targets = pvscsi_reg_read(sc, PVSCSI_REG_OFFSET_COMMAND_STATUS);

	if (max_targets == ~0) {
		max_targets = 16;
	}

	return (max_targets);
}

static int pvscsi_setup_req_call(struct pvscsi_softc *sc, uint32_t enable)
{
	uint32_t status;
	struct pvscsi_cmd_desc_setup_req_call cmd;

	if (!pvscsi_get_tunable(sc, "pvscsi_use_req_call_threshold",
	    pvscsi_use_req_call_threshold)) {
		return (0);
	}

	pvscsi_reg_write(sc, PVSCSI_REG_OFFSET_COMMAND,
	    PVSCSI_CMD_SETUP_REQCALLTHRESHOLD);
	status = pvscsi_reg_read(sc, PVSCSI_REG_OFFSET_COMMAND_STATUS);

	if (status != -1) {
		bzero(&cmd, sizeof(cmd));
		cmd.enable = enable;
		pvscsi_write_cmd(sc, PVSCSI_CMD_SETUP_REQCALLTHRESHOLD,
		    &cmd, sizeof(cmd));
		status = pvscsi_reg_read(sc, PVSCSI_REG_OFFSET_COMMAND_STATUS);

		return (status != 0);
	} else {
		return (0);
	}
}

static void
pvscsi_dma_cb(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	bus_addr_t *dest;

	KASSERT(nseg == 1, ("more than one segment"));

	dest = arg;

	if (!error) {
		*dest = segs->ds_addr;
	}
}

static void
pvscsi_dma_free(struct pvscsi_softc *sc, struct pvscsi_dma *dma)
{

	if (dma->tag != NULL) {
		if (dma->paddr != 0) {
			bus_dmamap_unload(dma->tag, dma->map);
		}

		if (dma->vaddr != NULL) {
			bus_dmamem_free(dma->tag, dma->vaddr, dma->map);
		}

		bus_dma_tag_destroy(dma->tag);
	}

	bzero(dma, sizeof(*dma));
}

static int
pvscsi_dma_alloc(struct pvscsi_softc *sc, struct pvscsi_dma *dma,
    bus_size_t size, bus_size_t alignment)
{
	int error;

	bzero(dma, sizeof(*dma));

	error = bus_dma_tag_create(sc->parent_dmat, alignment, 0,
	    BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL, NULL, size, 1, size,
	    BUS_DMA_ALLOCNOW, NULL, NULL, &dma->tag);
	if (error) {
		device_printf(sc->dev, "error creating dma tag, error %d\n",
		    error);
		goto fail;
	}

	error = bus_dmamem_alloc(dma->tag, &dma->vaddr,
	    BUS_DMA_NOWAIT | BUS_DMA_ZERO, &dma->map);
	if (error) {
		device_printf(sc->dev, "error allocating dma mem, error %d\n",
		    error);
		goto fail;
	}

	error = bus_dmamap_load(dma->tag, dma->map, dma->vaddr, size,
	    pvscsi_dma_cb, &dma->paddr, BUS_DMA_NOWAIT);
	if (error) {
		device_printf(sc->dev, "error mapping dma mam, error %d\n",
		    error);
		goto fail;
	}

	dma->size = size;

fail:
	if (error) {
		pvscsi_dma_free(sc, dma);
	}
	return (error);
}

static int
pvscsi_dma_alloc_ppns(struct pvscsi_softc *sc, struct pvscsi_dma *dma,
    uint64_t *ppn_list, uint32_t num_pages)
{
	int error;
	uint32_t i;
	uint64_t ppn;

	error = pvscsi_dma_alloc(sc, dma, num_pages * PAGE_SIZE, PAGE_SIZE);
	if (error) {
		device_printf(sc->dev, "Error allocating pages, error %d\n",
		    error);
		return (error);
	}

	ppn = dma->paddr >> PAGE_SHIFT;
	for (i = 0; i < num_pages; i++) {
		ppn_list[i] = ppn + i;
	}

	return (0);
}

static void
pvscsi_dma_free_per_hcb(struct pvscsi_softc *sc, uint32_t hcbs_allocated)
{
	int i;
	int lock_owned;
	struct pvscsi_hcb *hcb;

	lock_owned = mtx_owned(&sc->lock);

	if (lock_owned) {
		mtx_unlock(&sc->lock);
	}
	for (i = 0; i < hcbs_allocated; ++i) {
		hcb = sc->hcbs + i;
		callout_drain(&hcb->callout);
	};
	if (lock_owned) {
		mtx_lock(&sc->lock);
	}

	for (i = 0; i < hcbs_allocated; ++i) {
		hcb = sc->hcbs + i;
		bus_dmamap_destroy(sc->buffer_dmat, hcb->dma_map);
	};

	pvscsi_dma_free(sc, &sc->sense_buffer_dma);
	pvscsi_dma_free(sc, &sc->sg_list_dma);
}

static int
pvscsi_dma_alloc_per_hcb(struct pvscsi_softc *sc)
{
	int i;
	int error;
	struct pvscsi_hcb *hcb;

	i = 0;

	error = pvscsi_dma_alloc(sc, &sc->sg_list_dma,
	    sizeof(struct pvscsi_sg_list) * sc->hcb_cnt, 1);
	if (error) {
		device_printf(sc->dev,
		    "Error allocation sg list DMA memory, error %d\n", error);
		goto fail;
	}

	error = pvscsi_dma_alloc(sc, &sc->sense_buffer_dma,
				 PVSCSI_SENSE_LENGTH * sc->hcb_cnt, 1);
	if (error) {
		device_printf(sc->dev,
		    "Error allocation sg list DMA memory, error %d\n", error);
		goto fail;
	}

	for (i = 0; i < sc->hcb_cnt; ++i) {
		hcb = sc->hcbs + i;

		error = bus_dmamap_create(sc->buffer_dmat, 0, &hcb->dma_map);
		if (error) {
			device_printf(sc->dev,
			    "Error creating dma map for hcb %d, error %d\n",
			    i, error);
			goto fail;
		}

		hcb->sense_buffer =
		    (void *)((caddr_t)sc->sense_buffer_dma.vaddr +
		    PVSCSI_SENSE_LENGTH * i);
		hcb->sense_buffer_paddr =
		    sc->sense_buffer_dma.paddr + PVSCSI_SENSE_LENGTH * i;

		hcb->sg_list =
		    (struct pvscsi_sg_list *)((caddr_t)sc->sg_list_dma.vaddr +
		    sizeof(struct pvscsi_sg_list) * i);
		hcb->sg_list_paddr =
		    sc->sg_list_dma.paddr + sizeof(struct pvscsi_sg_list) * i;

		callout_init_mtx(&hcb->callout, &sc->lock, 0);
	}

	SLIST_INIT(&sc->free_list);
	for (i = (sc->hcb_cnt - 1); i >= 0; --i) {
		hcb = sc->hcbs + i;
		SLIST_INSERT_HEAD(&sc->free_list, hcb, links);
	}

fail:
	if (error) {
		pvscsi_dma_free_per_hcb(sc, i);
	}

	return (error);
}

static void
pvscsi_free_rings(struct pvscsi_softc *sc)
{

	pvscsi_dma_free(sc, &sc->rings_state_dma);
	pvscsi_dma_free(sc, &sc->req_ring_dma);
	pvscsi_dma_free(sc, &sc->cmp_ring_dma);
	if (sc->use_msg) {
		pvscsi_dma_free(sc, &sc->msg_ring_dma);
	}
}

static int
pvscsi_allocate_rings(struct pvscsi_softc *sc)
{
	int error;

	error = pvscsi_dma_alloc_ppns(sc, &sc->rings_state_dma,
	    &sc->rings_state_ppn, 1);
	if (error) {
		device_printf(sc->dev,
		    "Error allocating rings state, error = %d\n", error);
		goto fail;
	}
	sc->rings_state = sc->rings_state_dma.vaddr;

	error = pvscsi_dma_alloc_ppns(sc, &sc->req_ring_dma, sc->req_ring_ppn,
	    sc->req_ring_num_pages);
	if (error) {
		device_printf(sc->dev,
		    "Error allocating req ring pages, error = %d\n", error);
		goto fail;
	}
	sc->req_ring = sc->req_ring_dma.vaddr;

	error = pvscsi_dma_alloc_ppns(sc, &sc->cmp_ring_dma, sc->cmp_ring_ppn,
	    sc->cmp_ring_num_pages);
	if (error) {
		device_printf(sc->dev,
		    "Error allocating cmp ring pages, error = %d\n", error);
		goto fail;
	}
	sc->cmp_ring = sc->cmp_ring_dma.vaddr;

	sc->msg_ring = NULL;
	if (sc->use_msg) {
		error = pvscsi_dma_alloc_ppns(sc, &sc->msg_ring_dma,
		    sc->msg_ring_ppn, sc->msg_ring_num_pages);
		if (error) {
			device_printf(sc->dev,
			    "Error allocating cmp ring pages, error = %d\n",
			    error);
			goto fail;
		}
		sc->msg_ring = sc->msg_ring_dma.vaddr;
	}

	DEBUG_PRINTF(1, sc->dev, "rings_state: %p\n", sc->rings_state);
	DEBUG_PRINTF(1, sc->dev, "req_ring: %p - %u pages\n", sc->req_ring,
	    sc->req_ring_num_pages);
	DEBUG_PRINTF(1, sc->dev, "cmp_ring: %p - %u pages\n", sc->cmp_ring,
	    sc->cmp_ring_num_pages);
	DEBUG_PRINTF(1, sc->dev, "msg_ring: %p - %u pages\n", sc->msg_ring,
	    sc->msg_ring_num_pages);

fail:
	if (error) {
		pvscsi_free_rings(sc);
	}
	return (error);
}

static void
pvscsi_setup_rings(struct pvscsi_softc *sc)
{
	struct pvscsi_cmd_desc_setup_rings cmd;
	uint32_t i;

	bzero(&cmd, sizeof(cmd));

	cmd.rings_state_ppn = sc->rings_state_ppn;

	cmd.req_ring_num_pages = sc->req_ring_num_pages;
	for (i = 0; i < sc->req_ring_num_pages; ++i) {
		cmd.req_ring_ppns[i] = sc->req_ring_ppn[i];
	}

	cmd.cmp_ring_num_pages = sc->cmp_ring_num_pages;
	for (i = 0; i < sc->cmp_ring_num_pages; ++i) {
		cmd.cmp_ring_ppns[i] = sc->cmp_ring_ppn[i];
	}

	pvscsi_write_cmd(sc, PVSCSI_CMD_SETUP_RINGS, &cmd, sizeof(cmd));
}

static int
pvscsi_hw_supports_msg(struct pvscsi_softc *sc)
{
	uint32_t status;

	pvscsi_reg_write(sc, PVSCSI_REG_OFFSET_COMMAND,
	    PVSCSI_CMD_SETUP_MSG_RING);
	status = pvscsi_reg_read(sc, PVSCSI_REG_OFFSET_COMMAND_STATUS);

	return (status != -1);
}

static void
pvscsi_setup_msg_ring(struct pvscsi_softc *sc)
{
	struct pvscsi_cmd_desc_setup_msg_ring cmd;
	uint32_t i;

	KASSERT(sc->use_msg, ("msg is not being used"));

	bzero(&cmd, sizeof(cmd));

	cmd.num_pages = sc->msg_ring_num_pages;
	for (i = 0; i < sc->msg_ring_num_pages; ++i) {
		cmd.ring_ppns[i] = sc->msg_ring_ppn[i];
	}

	pvscsi_write_cmd(sc, PVSCSI_CMD_SETUP_MSG_RING, &cmd, sizeof(cmd));
}

static void
pvscsi_adapter_reset(struct pvscsi_softc *sc)
{
	uint32_t val __unused;

	device_printf(sc->dev, "Adapter Reset\n");

	pvscsi_write_cmd(sc, PVSCSI_CMD_ADAPTER_RESET, NULL, 0);
	val = pvscsi_read_intr_status(sc);

	DEBUG_PRINTF(2, sc->dev, "adapter reset done: %u\n", val);
}

static void
pvscsi_bus_reset(struct pvscsi_softc *sc)
{

	device_printf(sc->dev, "Bus Reset\n");

	pvscsi_write_cmd(sc, PVSCSI_CMD_RESET_BUS, NULL, 0);
	pvscsi_process_cmp_ring(sc);

	DEBUG_PRINTF(2, sc->dev, "bus reset done\n");
}

static void
pvscsi_device_reset(struct pvscsi_softc *sc, uint32_t target)
{
	struct pvscsi_cmd_desc_reset_device cmd;

	memset(&cmd, 0, sizeof(cmd));

	cmd.target = target;

	device_printf(sc->dev, "Device reset for target %u\n", target);

	pvscsi_write_cmd(sc, PVSCSI_CMD_RESET_DEVICE, &cmd, sizeof cmd);
	pvscsi_process_cmp_ring(sc);

	DEBUG_PRINTF(2, sc->dev, "device reset done\n");
}

static void
pvscsi_abort(struct pvscsi_softc *sc, uint32_t target, union ccb *ccb)
{
	struct pvscsi_cmd_desc_abort_cmd cmd;
	struct pvscsi_hcb *hcb;
	uint64_t context;

	pvscsi_process_cmp_ring(sc);

	hcb = ccb->ccb_h.ccb_pvscsi_hcb;

	if (hcb != NULL) {
		context = pvscsi_hcb_to_context(sc, hcb);

		memset(&cmd, 0, sizeof cmd);
		cmd.target = target;
		cmd.context = context;

		device_printf(sc->dev, "Abort for target %u context %llx\n",
		    target, (unsigned long long)context);

		pvscsi_write_cmd(sc, PVSCSI_CMD_ABORT_CMD, &cmd, sizeof(cmd));
		pvscsi_process_cmp_ring(sc);

		DEBUG_PRINTF(2, sc->dev, "abort done\n");
	} else {
		DEBUG_PRINTF(1, sc->dev,
		    "Target %u ccb %p not found for abort\n", target, ccb);
	}
}

static int
pvscsi_probe(device_t dev)
{

	if (pci_get_vendor(dev) == PCI_VENDOR_ID_VMWARE &&
	    pci_get_device(dev) == PCI_DEVICE_ID_VMWARE_PVSCSI) {
		device_set_desc(dev, "VMware Paravirtual SCSI Controller");
		return (BUS_PROBE_DEFAULT);
	}
	return (ENXIO);
}

static int
pvscsi_shutdown(device_t dev)
{

	return (0);
}

static void
pvscsi_timeout(void *arg)
{
	struct pvscsi_hcb *hcb;
	struct pvscsi_softc *sc;
	union ccb *ccb;

	hcb = arg;
	ccb = hcb->ccb;

	if (ccb == NULL) {
		/* Already completed */
		return;
	}

	sc = ccb->ccb_h.ccb_pvscsi_sc;
	mtx_assert(&sc->lock, MA_OWNED);

	device_printf(sc->dev, "Command timed out hcb=%p ccb=%p.\n", hcb, ccb);

	switch (hcb->recovery) {
	case PVSCSI_HCB_NONE:
		hcb->recovery = PVSCSI_HCB_ABORT;
		pvscsi_abort(sc, ccb->ccb_h.target_id, ccb);
		callout_reset_sbt(&hcb->callout, PVSCSI_ABORT_TIMEOUT * SBT_1S,
		    0, pvscsi_timeout, hcb, 0);
		break;
	case PVSCSI_HCB_ABORT:
		hcb->recovery = PVSCSI_HCB_DEVICE_RESET;
		pvscsi_freeze(sc);
		pvscsi_device_reset(sc, ccb->ccb_h.target_id);
		callout_reset_sbt(&hcb->callout, PVSCSI_RESET_TIMEOUT * SBT_1S,
		    0, pvscsi_timeout, hcb, 0);
		break;
	case PVSCSI_HCB_DEVICE_RESET:
		hcb->recovery = PVSCSI_HCB_BUS_RESET;
		pvscsi_freeze(sc);
		pvscsi_bus_reset(sc);
		callout_reset_sbt(&hcb->callout, PVSCSI_RESET_TIMEOUT * SBT_1S,
		    0, pvscsi_timeout, hcb, 0);
		break;
	case PVSCSI_HCB_BUS_RESET:
		pvscsi_freeze(sc);
		pvscsi_adapter_reset(sc);
		break;
	};
}

static void
pvscsi_process_completion(struct pvscsi_softc *sc,
    struct pvscsi_ring_cmp_desc *e)
{
	struct pvscsi_hcb *hcb;
	union ccb *ccb;
	uint32_t status;
	uint32_t btstat;
	uint32_t sdstat;
	bus_dmasync_op_t op;

	hcb = pvscsi_context_to_hcb(sc, e->context);

	callout_stop(&hcb->callout);

	ccb = hcb->ccb;

	btstat = e->host_status;
	sdstat = e->scsi_status;

	ccb->csio.scsi_status = sdstat;
	ccb->csio.resid = ccb->csio.dxfer_len - e->data_len;

	if ((ccb->ccb_h.flags & CAM_DIR_MASK) != CAM_DIR_NONE) {
		if ((ccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN) {
			op = BUS_DMASYNC_POSTREAD;
		} else {
			op = BUS_DMASYNC_POSTWRITE;
		}
		bus_dmamap_sync(sc->buffer_dmat, hcb->dma_map, op);
		bus_dmamap_unload(sc->buffer_dmat, hcb->dma_map);
	}

	if (btstat == BTSTAT_SUCCESS && sdstat == SCSI_STATUS_OK) {
		DEBUG_PRINTF(3, sc->dev,
		    "completing command context %llx success\n",
		    (unsigned long long)e->context);
		ccb->csio.resid = 0;
		status = CAM_REQ_CMP;
	} else {
		switch (btstat) {
		case BTSTAT_SUCCESS:
		case BTSTAT_LINKED_COMMAND_COMPLETED:
		case BTSTAT_LINKED_COMMAND_COMPLETED_WITH_FLAG:
			switch (sdstat) {
			case SCSI_STATUS_OK:
				ccb->csio.resid = 0;
				status = CAM_REQ_CMP;
				break;
			case SCSI_STATUS_CHECK_COND:
				status = CAM_SCSI_STATUS_ERROR;

				if (ccb->csio.sense_len != 0) {
					status |= CAM_AUTOSNS_VALID;

					memset(&ccb->csio.sense_data, 0,
					    sizeof(ccb->csio.sense_data));
					memcpy(&ccb->csio.sense_data,
					    hcb->sense_buffer,
					    MIN(ccb->csio.sense_len,
						e->sense_len));
				}
				break;
			case SCSI_STATUS_BUSY:
			case SCSI_STATUS_QUEUE_FULL:
				status = CAM_REQUEUE_REQ;
				break;
			case SCSI_STATUS_CMD_TERMINATED:
			case SCSI_STATUS_TASK_ABORTED:
				status = CAM_REQ_ABORTED;
				break;
			default:
				DEBUG_PRINTF(1, sc->dev,
				    "ccb: %p sdstat=0x%x\n", ccb, sdstat);
				status = CAM_SCSI_STATUS_ERROR;
				break;
			}
			break;
		case BTSTAT_SELTIMEO:
			status = CAM_SEL_TIMEOUT;
			break;
		case BTSTAT_DATARUN:
		case BTSTAT_DATA_UNDERRUN:
			status = CAM_DATA_RUN_ERR;
			break;
		case BTSTAT_ABORTQUEUE:
		case BTSTAT_HATIMEOUT:
			status = CAM_REQUEUE_REQ;
			break;
		case BTSTAT_NORESPONSE:
		case BTSTAT_SENTRST:
		case BTSTAT_RECVRST:
		case BTSTAT_BUSRESET:
			status = CAM_SCSI_BUS_RESET;
			break;
		case BTSTAT_SCSIPARITY:
			status = CAM_UNCOR_PARITY;
			break;
		case BTSTAT_BUSFREE:
			status = CAM_UNEXP_BUSFREE;
			break;
		case BTSTAT_INVPHASE:
			status = CAM_SEQUENCE_FAIL;
			break;
		case BTSTAT_SENSFAILED:
			status = CAM_AUTOSENSE_FAIL;
			break;
		case BTSTAT_LUNMISMATCH:
		case BTSTAT_TAGREJECT:
		case BTSTAT_DISCONNECT:
		case BTSTAT_BADMSG:
		case BTSTAT_INVPARAM:
			status = CAM_REQ_CMP_ERR;
			break;
		case BTSTAT_HASOFTWARE:
		case BTSTAT_HAHARDWARE:
			status = CAM_NO_HBA;
			break;
		default:
			device_printf(sc->dev, "unknown hba status: 0x%x\n",
			    btstat);
			status = CAM_NO_HBA;
			break;
		}

		DEBUG_PRINTF(3, sc->dev,
		    "completing command context %llx btstat %x sdstat %x - status %x\n",
		    (unsigned long long)e->context, btstat, sdstat, status);
	}

	ccb->ccb_h.ccb_pvscsi_hcb = NULL;
	ccb->ccb_h.ccb_pvscsi_sc = NULL;
	pvscsi_hcb_put(sc, hcb);

	ccb->ccb_h.status =
	    status | (ccb->ccb_h.status & ~(CAM_STATUS_MASK | CAM_SIM_QUEUED));

	if (sc->frozen) {
		ccb->ccb_h.status |= CAM_RELEASE_SIMQ;
		sc->frozen = 0;
	}

	if (status != CAM_REQ_CMP) {
		ccb->ccb_h.status |= CAM_DEV_QFRZN;
		xpt_freeze_devq(ccb->ccb_h.path, /*count*/ 1);
	}
	xpt_done(ccb);
}

static void
pvscsi_process_cmp_ring(struct pvscsi_softc *sc)
{
	struct pvscsi_ring_cmp_desc *ring;
	struct pvscsi_rings_state *s;
	struct pvscsi_ring_cmp_desc *e;
	uint32_t mask;

	mtx_assert(&sc->lock, MA_OWNED);

	s = sc->rings_state;
	ring = sc->cmp_ring;
	mask = MASK(s->cmp_num_entries_log2);

	while (s->cmp_cons_idx != s->cmp_prod_idx) {
		e = ring + (s->cmp_cons_idx & mask);

		pvscsi_process_completion(sc, e);

		mb();
		s->cmp_cons_idx++;
	}
}

static void
pvscsi_process_msg(struct pvscsi_softc *sc, struct pvscsi_ring_msg_desc *e)
{
	struct pvscsi_ring_msg_dev_status_changed *desc;

	union ccb *ccb;
	switch (e->type) {
	case PVSCSI_MSG_DEV_ADDED:
	case PVSCSI_MSG_DEV_REMOVED: {
		desc = (struct pvscsi_ring_msg_dev_status_changed *)e;

		device_printf(sc->dev, "MSG: device %s at scsi%u:%u:%u\n",
		    desc->type == PVSCSI_MSG_DEV_ADDED ? "addition" : "removal",
		    desc->bus, desc->target, desc->lun[1]);

		ccb = xpt_alloc_ccb_nowait();
		if (ccb == NULL) {
			device_printf(sc->dev,
			    "Error allocating CCB for dev change.\n");
			break;
		}

		if (xpt_create_path(&ccb->ccb_h.path, NULL,
		    cam_sim_path(sc->sim), desc->target, desc->lun[1])
		    != CAM_REQ_CMP) {
			device_printf(sc->dev,
			    "Error creating path for dev change.\n");
			xpt_free_ccb(ccb);
			break;
		}

		xpt_rescan(ccb);
	} break;
	default:
		device_printf(sc->dev, "Unknown msg type 0x%x\n", e->type);
	};
}

static void
pvscsi_process_msg_ring(struct pvscsi_softc *sc)
{
	struct pvscsi_ring_msg_desc *ring;
	struct pvscsi_rings_state *s;
	struct pvscsi_ring_msg_desc *e;
	uint32_t mask;

	mtx_assert(&sc->lock, MA_OWNED);

	s = sc->rings_state;
	ring = sc->msg_ring;
	mask = MASK(s->msg_num_entries_log2);

	while (s->msg_cons_idx != s->msg_prod_idx) {
		e = ring + (s->msg_cons_idx & mask);

		pvscsi_process_msg(sc, e);

		mb();
		s->msg_cons_idx++;
	}
}

static void
pvscsi_intr_locked(struct pvscsi_softc *sc)
{
	uint32_t val;

	mtx_assert(&sc->lock, MA_OWNED);

	val = pvscsi_read_intr_status(sc);

	if ((val & PVSCSI_INTR_ALL_SUPPORTED) != 0) {
		pvscsi_write_intr_status(sc, val & PVSCSI_INTR_ALL_SUPPORTED);
		pvscsi_process_cmp_ring(sc);
		if (sc->use_msg) {
			pvscsi_process_msg_ring(sc);
		}
	}
}

static void
pvscsi_intr(void *xsc)
{
	struct pvscsi_softc *sc;

	sc = xsc;

	mtx_assert(&sc->lock, MA_NOTOWNED);

	mtx_lock(&sc->lock);
	pvscsi_intr_locked(xsc);
	mtx_unlock(&sc->lock);
}

static void
pvscsi_poll(struct cam_sim *sim)
{
	struct pvscsi_softc *sc;

	sc = cam_sim_softc(sim);

	mtx_assert(&sc->lock, MA_OWNED);
	pvscsi_intr_locked(sc);
}

static void
pvscsi_execute_ccb(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	struct pvscsi_hcb *hcb;
	struct pvscsi_ring_req_desc *e;
	union ccb *ccb;
	struct pvscsi_softc *sc;
	struct pvscsi_rings_state *s;
	uint8_t cdb0;
	bus_dmasync_op_t op;

	hcb = arg;
	ccb = hcb->ccb;
	e = hcb->e;
	sc = ccb->ccb_h.ccb_pvscsi_sc;
	s = sc->rings_state;

	mtx_assert(&sc->lock, MA_OWNED);

	if (error) {
		device_printf(sc->dev, "pvscsi_execute_ccb error %d\n", error);

		if (error == EFBIG) {
			ccb->ccb_h.status = CAM_REQ_TOO_BIG;
		} else {
			ccb->ccb_h.status = CAM_REQ_CMP_ERR;
		}

		pvscsi_hcb_put(sc, hcb);
		xpt_done(ccb);
		return;
	}

	e->flags = 0;
	op = 0;
	switch (ccb->ccb_h.flags & CAM_DIR_MASK) {
	case CAM_DIR_NONE:
		e->flags |= PVSCSI_FLAG_CMD_DIR_NONE;
		break;
	case CAM_DIR_IN:
		e->flags |= PVSCSI_FLAG_CMD_DIR_TOHOST;
		op = BUS_DMASYNC_PREREAD;
		break;
	case CAM_DIR_OUT:
		e->flags |= PVSCSI_FLAG_CMD_DIR_TODEVICE;
		op = BUS_DMASYNC_PREWRITE;
		break;
	case CAM_DIR_BOTH:
		/* TODO: does this need handling? */
		break;
	}

	if (nseg != 0) {
		if (nseg > 1) {
			int i;
			struct pvscsi_sg_element *sge;

			KASSERT(nseg <= PVSCSI_MAX_SG_ENTRIES_PER_SEGMENT,
			    ("too many sg segments"));

			sge = hcb->sg_list->sge;
			e->flags |= PVSCSI_FLAG_CMD_WITH_SG_LIST;

			for (i = 0; i < nseg; ++i) {
				sge[i].addr = segs[i].ds_addr;
				sge[i].length = segs[i].ds_len;
				sge[i].flags = 0;
			}

			e->data_addr = hcb->sg_list_paddr;
		} else {
			e->data_addr = segs->ds_addr;
		}

		bus_dmamap_sync(sc->buffer_dmat, hcb->dma_map, op);
	} else {
		e->data_addr = 0;
	}

	cdb0 = e->cdb[0];
	ccb->ccb_h.status |= CAM_SIM_QUEUED;

	if (ccb->ccb_h.timeout != CAM_TIME_INFINITY) {
		callout_reset_sbt(&hcb->callout, ccb->ccb_h.timeout * SBT_1MS,
		    0, pvscsi_timeout, hcb, 0);
	}

	mb();
	s->req_prod_idx++;
	pvscsi_kick_io(sc, cdb0);
}

static void
pvscsi_action(struct cam_sim *sim, union ccb *ccb)
{
	struct pvscsi_softc *sc;
	struct ccb_hdr *ccb_h;

	sc = cam_sim_softc(sim);
	ccb_h = &ccb->ccb_h;

	mtx_assert(&sc->lock, MA_OWNED);

	switch (ccb_h->func_code) {
	case XPT_SCSI_IO:
	{
		struct ccb_scsiio *csio;
		uint32_t req_num_entries_log2;
		struct pvscsi_ring_req_desc *ring;
		struct pvscsi_ring_req_desc *e;
		struct pvscsi_rings_state *s;
		struct pvscsi_hcb *hcb;

		csio = &ccb->csio;
		ring = sc->req_ring;
		s = sc->rings_state;

		hcb = NULL;

		/*
		 * Check if it was completed already (such as aborted
		 * by upper layers)
		 */
		if ((ccb_h->status & CAM_STATUS_MASK) != CAM_REQ_INPROG) {
			xpt_done(ccb);
			return;
		}

		req_num_entries_log2 = s->req_num_entries_log2;

		if (s->req_prod_idx - s->cmp_cons_idx >=
		    (1 << req_num_entries_log2)) {
			device_printf(sc->dev,
			    "Not enough room on completion ring.\n");
			pvscsi_freeze(sc);
			ccb_h->status = CAM_REQUEUE_REQ;
			goto finish_ccb;
		}

		hcb = pvscsi_hcb_get(sc);
		if (hcb == NULL) {
			device_printf(sc->dev, "No free hcbs.\n");
			pvscsi_freeze(sc);
			ccb_h->status = CAM_REQUEUE_REQ;
			goto finish_ccb;
		}

		hcb->ccb = ccb;
		ccb_h->ccb_pvscsi_hcb = hcb;
		ccb_h->ccb_pvscsi_sc = sc;

		if (csio->cdb_len > sizeof(e->cdb)) {
			DEBUG_PRINTF(2, sc->dev, "cdb length %u too large\n",
			    csio->cdb_len);
			ccb_h->status = CAM_REQ_INVALID;
			goto finish_ccb;
		}

		if (ccb_h->flags & CAM_CDB_PHYS) {
			DEBUG_PRINTF(2, sc->dev,
			    "CAM_CDB_PHYS not implemented\n");
			ccb_h->status = CAM_REQ_INVALID;
			goto finish_ccb;
		}

		e = ring + (s->req_prod_idx & MASK(req_num_entries_log2));

		e->bus = cam_sim_bus(sim);
		e->target = ccb_h->target_id;
		memset(e->lun, 0, sizeof(e->lun));
		e->lun[1] = ccb_h->target_lun;
		e->data_addr = 0;
		e->data_len = csio->dxfer_len;
		e->vcpu_hint = curcpu;

		e->cdb_len = csio->cdb_len;
		memcpy(e->cdb, scsiio_cdb_ptr(csio), csio->cdb_len);

		e->sense_addr = 0;
		e->sense_len = csio->sense_len;
		if (e->sense_len > 0) {
			e->sense_addr = hcb->sense_buffer_paddr;
		}

		e->tag = MSG_SIMPLE_Q_TAG;
		if (ccb_h->flags & CAM_TAG_ACTION_VALID) {
			e->tag = csio->tag_action;
		}

		e->context = pvscsi_hcb_to_context(sc, hcb);
		hcb->e = e;

		DEBUG_PRINTF(3, sc->dev,
		    " queuing command %02x context %llx\n", e->cdb[0],
		    (unsigned long long)e->context);
		bus_dmamap_load_ccb(sc->buffer_dmat, hcb->dma_map, ccb,
		    pvscsi_execute_ccb, hcb, 0);
		break;

finish_ccb:
		if (hcb != NULL) {
			pvscsi_hcb_put(sc, hcb);
		}
		xpt_done(ccb);
	} break;
	case XPT_ABORT:
	{
		struct pvscsi_hcb *abort_hcb;
		union ccb *abort_ccb;

		abort_ccb = ccb->cab.abort_ccb;
		abort_hcb = abort_ccb->ccb_h.ccb_pvscsi_hcb;

		if (abort_hcb->ccb != NULL && abort_hcb->ccb == abort_ccb) {
			if (abort_ccb->ccb_h.func_code == XPT_SCSI_IO) {
				pvscsi_abort(sc, ccb_h->target_id, abort_ccb);
				ccb_h->status = CAM_REQ_CMP;
			} else {
				ccb_h->status = CAM_UA_ABORT;
			}
		} else {
			device_printf(sc->dev,
			    "Could not find hcb for ccb %p (tgt %u)\n",
			    ccb, ccb_h->target_id);
			ccb_h->status = CAM_REQ_CMP;
		}
		xpt_done(ccb);
	} break;
	case XPT_RESET_DEV:
	{
		pvscsi_device_reset(sc, ccb_h->target_id);
		ccb_h->status = CAM_REQ_CMP;
		xpt_done(ccb);
	} break;
	case XPT_RESET_BUS:
	{
		pvscsi_bus_reset(sc);
		ccb_h->status = CAM_REQ_CMP;
		xpt_done(ccb);
	} break;
	case XPT_PATH_INQ:
	{
		struct ccb_pathinq *cpi;

		cpi = &ccb->cpi;

		cpi->version_num = 1;
		cpi->hba_inquiry = PI_TAG_ABLE;
		cpi->target_sprt = 0;
		cpi->hba_misc = PIM_NOBUSRESET | PIM_UNMAPPED;
		cpi->hba_eng_cnt = 0;
		/* cpi->vuhba_flags = 0; */
		cpi->max_target = sc->max_targets;
		cpi->max_lun = 0;
		cpi->async_flags = 0;
		cpi->hpath_id = 0;
		cpi->unit_number = cam_sim_unit(sim);
		cpi->bus_id = cam_sim_bus(sim);
		cpi->initiator_id = 7;
		cpi->base_transfer_speed = 750000;
		strlcpy(cpi->sim_vid, "VMware", SIM_IDLEN);
		strlcpy(cpi->hba_vid, "VMware", HBA_IDLEN);
		strlcpy(cpi->dev_name, cam_sim_name(sim), DEV_IDLEN);
		/* Limit I/O to 256k since we can't do 512k unaligned I/O */
		cpi->maxio = (PVSCSI_MAX_SG_ENTRIES_PER_SEGMENT / 2) * PAGE_SIZE;
		cpi->protocol = PROTO_SCSI;
		cpi->protocol_version = SCSI_REV_SPC2;
		cpi->transport = XPORT_SAS;
		cpi->transport_version = 0;

		ccb_h->status = CAM_REQ_CMP;
		xpt_done(ccb);
	} break;
	case XPT_GET_TRAN_SETTINGS:
	{
		struct ccb_trans_settings *cts;

		cts = &ccb->cts;

		cts->protocol = PROTO_SCSI;
		cts->protocol_version = SCSI_REV_SPC2;
		cts->transport = XPORT_SAS;
		cts->transport_version = 0;

		cts->proto_specific.scsi.flags = CTS_SCSI_FLAGS_TAG_ENB;
		cts->proto_specific.scsi.valid = CTS_SCSI_VALID_TQ;

		ccb_h->status = CAM_REQ_CMP;
		xpt_done(ccb);
	} break;
	case XPT_CALC_GEOMETRY:
	{
		cam_calc_geometry(&ccb->ccg, 1);
		xpt_done(ccb);
	} break;
	default:
		ccb_h->status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;
	}
}

static void
pvscsi_free_interrupts(struct pvscsi_softc *sc)
{

	if (sc->irq_handler != NULL) {
		bus_teardown_intr(sc->dev, sc->irq_res, sc->irq_handler);
	}
	if (sc->irq_res != NULL) {
		bus_release_resource(sc->dev, SYS_RES_IRQ, sc->irq_id,
		    sc->irq_res);
	}
	if (sc->use_msi_or_msix) {
		pci_release_msi(sc->dev);
	}
}

static int
pvscsi_setup_interrupts(struct pvscsi_softc *sc)
{
	int error;
	int flags;
	int use_msix;
	int use_msi;
	int count;

	sc->use_msi_or_msix = 0;

	use_msix = pvscsi_get_tunable(sc, "use_msix", pvscsi_use_msix);
	use_msi = pvscsi_get_tunable(sc, "use_msi", pvscsi_use_msi);

	if (use_msix && pci_msix_count(sc->dev) > 0) {
		count = 1;
		if (pci_alloc_msix(sc->dev, &count) == 0 && count == 1) {
			sc->use_msi_or_msix = 1;
			device_printf(sc->dev, "Interrupt: MSI-X\n");
		} else {
			pci_release_msi(sc->dev);
		}
	}

	if (sc->use_msi_or_msix == 0 && use_msi && pci_msi_count(sc->dev) > 0) {
		count = 1;
		if (pci_alloc_msi(sc->dev, &count) == 0 && count == 1) {
			sc->use_msi_or_msix = 1;
			device_printf(sc->dev, "Interrupt: MSI\n");
		} else {
			pci_release_msi(sc->dev);
		}
	}

	flags = RF_ACTIVE;
	if (sc->use_msi_or_msix) {
		sc->irq_id = 1;
	} else {
		device_printf(sc->dev, "Interrupt: INT\n");
		sc->irq_id = 0;
		flags |= RF_SHAREABLE;
	}

	sc->irq_res = bus_alloc_resource_any(sc->dev, SYS_RES_IRQ, &sc->irq_id,
	    flags);
	if (sc->irq_res == NULL) {
		device_printf(sc->dev, "IRQ allocation failed\n");
		if (sc->use_msi_or_msix) {
			pci_release_msi(sc->dev);
		}
		return (ENXIO);
	}

	error = bus_setup_intr(sc->dev, sc->irq_res,
	    INTR_TYPE_CAM | INTR_MPSAFE, NULL, pvscsi_intr, sc,
	    &sc->irq_handler);
	if (error) {
		device_printf(sc->dev, "IRQ handler setup failed\n");
		pvscsi_free_interrupts(sc);
		return (error);
	}

	return (0);
}

static void
pvscsi_free_all(struct pvscsi_softc *sc)
{

	if (sc->sim) {
		int error;

		if (sc->bus_path) {
			xpt_free_path(sc->bus_path);
		}

		error = xpt_bus_deregister(cam_sim_path(sc->sim));
		if (error != 0) {
			device_printf(sc->dev,
			    "Error deregistering bus, error %d\n", error);
		}

		cam_sim_free(sc->sim, TRUE);
	}

	pvscsi_dma_free_per_hcb(sc, sc->hcb_cnt);

	if (sc->hcbs) {
		free(sc->hcbs, M_PVSCSI);
	}

	pvscsi_free_rings(sc);

	pvscsi_free_interrupts(sc);

	if (sc->buffer_dmat != NULL) {
		bus_dma_tag_destroy(sc->buffer_dmat);
	}

	if (sc->parent_dmat != NULL) {
		bus_dma_tag_destroy(sc->parent_dmat);
	}

	if (sc->mm_res != NULL) {
		bus_release_resource(sc->dev, SYS_RES_MEMORY, sc->mm_rid,
		    sc->mm_res);
	}
}

static int
pvscsi_attach(device_t dev)
{
	struct pvscsi_softc *sc;
	int rid;
	int barid;
	int error;
	int max_queue_depth;
	int adapter_queue_size;
	struct cam_devq *devq;

	sc = device_get_softc(dev);
	sc->dev = dev;

	mtx_init(&sc->lock, "pvscsi", NULL, MTX_DEF);

	pci_enable_busmaster(dev);

	sc->mm_rid = -1;
	for (barid = 0; barid <= PCIR_MAX_BAR_0; ++barid) {
		rid = PCIR_BAR(barid);

		sc->mm_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
		    RF_ACTIVE);
		if (sc->mm_res != NULL) {
			sc->mm_rid = rid;
			break;
		}
	}

	if (sc->mm_res == NULL) {
		device_printf(dev, "could not map device memory\n");
		return (ENXIO);
	}

	error = bus_dma_tag_create(bus_get_dma_tag(dev), 1, 0,
	    BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL, NULL, BUS_SPACE_MAXSIZE,
	    BUS_SPACE_UNRESTRICTED, BUS_SPACE_MAXSIZE, 0, NULL, NULL,
	    &sc->parent_dmat);
	if (error) {
		device_printf(dev, "parent dma tag create failure, error %d\n",
		    error);
		pvscsi_free_all(sc);
		return (ENXIO);
	}

	error = bus_dma_tag_create(sc->parent_dmat, 1, 0,
	    BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL, NULL,
	    PVSCSI_MAX_SG_ENTRIES_PER_SEGMENT * PAGE_SIZE,
	    PVSCSI_MAX_SG_ENTRIES_PER_SEGMENT, PAGE_SIZE, BUS_DMA_ALLOCNOW,
	    NULL, NULL, &sc->buffer_dmat);
	if (error) {
		device_printf(dev, "parent dma tag create failure, error %d\n",
		    error);
		pvscsi_free_all(sc);
		return (ENXIO);
	}

	error = pvscsi_setup_interrupts(sc);
	if (error) {
		device_printf(dev, "Interrupt setup failed\n");
		pvscsi_free_all(sc);
		return (error);
	}

	sc->max_targets = pvscsi_get_max_targets(sc);

	sc->use_msg = pvscsi_get_tunable(sc, "use_msg", pvscsi_use_msg) &&
	    pvscsi_hw_supports_msg(sc);
	sc->msg_ring_num_pages = sc->use_msg ? 1 : 0;

	sc->req_ring_num_pages = pvscsi_get_tunable(sc, "request_ring_pages",
	    pvscsi_request_ring_pages);
	if (sc->req_ring_num_pages <= 0) {
		if (sc->max_targets <= 16) {
			sc->req_ring_num_pages =
			    PVSCSI_DEFAULT_NUM_PAGES_REQ_RING;
		} else {
			sc->req_ring_num_pages = PVSCSI_MAX_NUM_PAGES_REQ_RING;
		}
	} else if (sc->req_ring_num_pages > PVSCSI_MAX_NUM_PAGES_REQ_RING) {
		sc->req_ring_num_pages = PVSCSI_MAX_NUM_PAGES_REQ_RING;
	}
	sc->cmp_ring_num_pages = sc->req_ring_num_pages;

	max_queue_depth = pvscsi_get_tunable(sc, "max_queue_depth",
	    pvscsi_max_queue_depth);

	adapter_queue_size = (sc->req_ring_num_pages * PAGE_SIZE) /
	    sizeof(struct pvscsi_ring_req_desc);
	if (max_queue_depth > 0) {
		adapter_queue_size = MIN(adapter_queue_size, max_queue_depth);
	}
	adapter_queue_size = MIN(adapter_queue_size,
	    PVSCSI_MAX_REQ_QUEUE_DEPTH);

	device_printf(sc->dev, "Use Msg: %d\n", sc->use_msg);
	device_printf(sc->dev, "REQ num pages: %d\n", sc->req_ring_num_pages);
	device_printf(sc->dev, "CMP num pages: %d\n", sc->cmp_ring_num_pages);
	device_printf(sc->dev, "MSG num pages: %d\n", sc->msg_ring_num_pages);
	device_printf(sc->dev, "Queue size: %d\n", adapter_queue_size);

	if (pvscsi_allocate_rings(sc)) {
		device_printf(dev, "ring allocation failed\n");
		pvscsi_free_all(sc);
		return (ENXIO);
	}

	sc->hcb_cnt = adapter_queue_size;
	sc->hcbs = malloc(sc->hcb_cnt * sizeof(*sc->hcbs), M_PVSCSI,
	    M_NOWAIT | M_ZERO);
	if (sc->hcbs == NULL) {
		device_printf(dev, "error allocating hcb array\n");
		pvscsi_free_all(sc);
		return (ENXIO);
	}

	if (pvscsi_dma_alloc_per_hcb(sc)) {
		device_printf(dev, "error allocating per hcb dma memory\n");
		pvscsi_free_all(sc);
		return (ENXIO);
	}

	pvscsi_adapter_reset(sc);

	devq = cam_simq_alloc(adapter_queue_size);
	if (devq == NULL) {
		device_printf(dev, "cam devq alloc failed\n");
		pvscsi_free_all(sc);
		return (ENXIO);
	}

	sc->sim = cam_sim_alloc(pvscsi_action, pvscsi_poll, "pvscsi", sc,
	    device_get_unit(dev), &sc->lock, 1, adapter_queue_size, devq);
	if (sc->sim == NULL) {
		device_printf(dev, "cam sim alloc failed\n");
		cam_simq_free(devq);
		pvscsi_free_all(sc);
		return (ENXIO);
	}

	mtx_lock(&sc->lock);

	if (xpt_bus_register(sc->sim, dev, 0) != CAM_SUCCESS) {
		device_printf(dev, "xpt bus register failed\n");
		pvscsi_free_all(sc);
		mtx_unlock(&sc->lock);
		return (ENXIO);
	}

	if (xpt_create_path(&sc->bus_path, NULL, cam_sim_path(sc->sim),
	    CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
		device_printf(dev, "xpt create path failed\n");
		pvscsi_free_all(sc);
		mtx_unlock(&sc->lock);
		return (ENXIO);
	}

	pvscsi_setup_rings(sc);
	if (sc->use_msg) {
		pvscsi_setup_msg_ring(sc);
	}

	sc->use_req_call_threshold = pvscsi_setup_req_call(sc, 1);

	pvscsi_intr_enable(sc);

	mtx_unlock(&sc->lock);

	return (0);
}

static int
pvscsi_detach(device_t dev)
{
	struct pvscsi_softc *sc;

	sc = device_get_softc(dev);

	pvscsi_intr_disable(sc);
	pvscsi_adapter_reset(sc);

	if (sc->irq_handler != NULL) {
		bus_teardown_intr(dev, sc->irq_res, sc->irq_handler);
	}

	mtx_lock(&sc->lock);
	pvscsi_free_all(sc);
	mtx_unlock(&sc->lock);

	mtx_destroy(&sc->lock);

	return (0);
}

static device_method_t pvscsi_methods[] = {
	DEVMETHOD(device_probe, pvscsi_probe),
	DEVMETHOD(device_shutdown, pvscsi_shutdown),
	DEVMETHOD(device_attach, pvscsi_attach),
	DEVMETHOD(device_detach, pvscsi_detach),
	DEVMETHOD_END
};

static driver_t pvscsi_driver = {
	"pvscsi", pvscsi_methods, sizeof(struct pvscsi_softc)
};

DRIVER_MODULE(pvscsi, pci, pvscsi_driver, 0, 0);

MODULE_DEPEND(pvscsi, pci, 1, 1, 1);
MODULE_DEPEND(pvscsi, cam, 1, 1, 1);
