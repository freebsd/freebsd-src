/*
 * Copyright 2026 Justin Hibbits <jhibbits@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef	DPAA_SEC_VAR_H
#define	DPAA_SEC_VAR_H

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/callout.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/rman.h>
#include <sys/smp.h>

#include <machine/atomic.h>
#include <machine/bus.h>
#include <machine/resource.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <sys/endian.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <opencrypto/cryptodev.h>
#include <opencrypto/xform_auth.h>

#include "dpaa_common.h"

#include "cryptodev_if.h"

/* A job descriptor can have up to 64 words */
#define	SEC_MAX_DESC_WORDS	64

/* Arbitrary limit */
#define	SEC_MAX_SEGMENTS	64
#define	SEC_MAX_SIZE		((SEC_MAX_SEGMENTS - 1) * PAGE_SIZE)

#define	SEC_MAX_DIGEST		64	/* SHA-512 */
#define	SEC_CCM_CTX_LEN		56

/*
 * SEC's output ring entry: 8-byte descriptor phys addr
 * echoed from the input ring, followed by a 4-byte job termination
 * status word.  Entries are packed.
 */
struct sec_or_entry {
	uint64_t	desc_addr;
	uint32_t	status;
} __packed;

_Static_assert(sizeof(struct sec_or_entry) == 12, "OR entry size");

struct sec_job;
struct sec_session;

/*
 * One Job Ring.  Rings are independent all the way down: separate
 * register block, separate completion interrupt, separate lock.
 * Spreading jobs across them keeps submitters off each other's locks.
 * The rings themselves live in DMA-safe memory from bus_dmamem_alloc.
 */
struct sec_jr {
	struct sec_softc	*jr_sc;		/* for the interrupt handler */
	phandle_t		 jr_node;	/* FDT node for this JR */
	uint32_t		 jr_off;	/* JR base within sc_rres */

	bus_dma_tag_t		 jr_ring_tag;
	bus_dmamap_t		 jr_map;	/* covers both rings */
	uint64_t		*jr_ir;		/* input, JR_RING_SIZE * 8B */
	vm_paddr_t		 jr_ir_pa;
	struct sec_or_entry	*jr_or;		/* output ring */
	vm_paddr_t		 jr_or_pa;
	uint32_t		 jr_ir_head;	/* next slot driver writes */
	uint32_t		 jr_or_tail;	/* next slot driver reads */

	/* Completion IRQ, separate from the SEC top-level error IRQ. */
	struct resource		*jr_ires;
	int			 jr_irid;
	void			*jr_icookie;

	/*
	 * Jobs handed to the ring and not yet seen on the output ring.
	 * jr_lock covers this and both ring indices, and is held only for
	 * the ring manipulation itself, never across a job.  When the ring
	 * fills, jr_blocked records that opencrypto needs a
	 * crypto_unblock() once slots free up.
	 */
	uint32_t		 jr_inflight;
	int			 jr_blocked;

	/*
	 * Outstanding jobs in submission order, so the watchdog only has
	 * to look at the head to find the oldest.  jr_flushing marks the
	 * window between asking SEC to flush the ring and clearing HALT.
	 */
	TAILQ_HEAD(, sec_job)	 jr_active;
	struct callout		 jr_wdog;
	bool			 jr_flushing;
	bool			 jr_dying;

	struct mtx		 jr_lock;
};

struct sec_softc {
	device_t		 sc_dev;
	struct resource		*sc_rres;	/* CCSR MMIO for SEC */
	int			 sc_rrid;
	struct resource		*sc_ires;	/* SEC error IRQ */
	int			 sc_irid;
	void			*sc_icookie;
	bus_dma_tag_t		 sc_dmatag;	/* for crypto payloads */
	int32_t			 sc_cid;	/* opencrypto driver id */
	int			 sc_version;

	struct sec_jr		 *sc_jr;	/* Job rings */
	u_int			 sc_njr;
};

/*
 * Per-request state.  The job descriptor at the head becomes the
 * address we push into the input ring.  SEC echoes that same address
 * in the output ring, and PHYS_TO_DMAP() gives us the struct back.
 */

struct sec_job {
	uint32_t		 jd[SEC_MAX_DESC_WORDS];
	struct dpaa_sgte		 in_sgt[1 + SEC_MAX_SEGMENTS];
	struct dpaa_sgte		 out_sgt[SEC_MAX_SEGMENTS];
	/* Driver-private (SEC does not touch anything below). */
	struct cryptop		*crp;
	struct sec_session	*sess;
	bus_dmamap_t		 map;
	TAILQ_ENTRY(sec_job)	 job_link;
	int			 job_deadline;	/* ticks */
	/* Also holds the expanded 16-byte XTS tweak; see sec_xts_tweak(). */
	uint8_t			 iv[AES_BLOCK_LEN];
	uint8_t			 ccm_ctx[SEC_CCM_CTX_LEN];
	uint8_t			 ccm_alen[2];
	uint8_t			 digest[SEC_MAX_DIGEST];
	int			 nsegs;
	bus_dma_segment_t	 segs[SEC_MAX_SEGMENTS];
};

int	sec_init_rings(struct sec_softc *sc);
int	sec_destroy_rings(struct sec_softc *sc);
int	sec_jr_submit_job(struct sec_softc *sc, struct sec_jr *jr,
    struct sec_job *job);
void	sec_jr_teardown(struct sec_softc *sc, struct sec_jr *jr);
void	sec_complete_one(struct sec_softc *sc, uint64_t desc_pa, uint32_t status);

MALLOC_DECLARE(M_SEC);

#endif
