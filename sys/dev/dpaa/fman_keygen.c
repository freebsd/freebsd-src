/*-
 * Copyright (c) 2026 Justin Hibbits
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * FMan KeyGen (KG) support.  The KG block is the FMan sub-unit that
 * classifies incoming frames into destination FQIDs via user-defined
 * "schemes".  Each scheme extracts a key from the frame's parse
 * result, hashes it, and enqueues to FQID = base | (hash & mask).
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <machine/bus.h>

#include "fman.h"
#include "fman_keygen.h"

#define	FMAN_KG_OFFSET	0xc1000

/*
 * Register offsets, relative to FMAN_KG_OFFSET (0xc1000) inside the
 * FMan register block.
 */
#define	FMKG_GCR	0x000		/* general configuration */
#define	  FMKG_GCR_EN	  0x80000000
#define	FMKG_EER	0x00C		/* error event */
#define	  FMKG_EER_DOUBLE_ECC	  0x80000000
#define	  FMKG_EER_KEYSIZE_OVF	  0x40000000
#define	FMKG_EEER	0x010		/* error event enable */
#define	FMKG_SEER	0x01C		/* scheme error event */
#define	FMKG_SEEER	0x020		/* scheme error event enable */
#define	FMKG_GSR	0x024		/* global status */
#define	  FMKG_GSR_BSY	  0x80000000
#define	FMKG_TPC	0x028		/* total packet counter */
#define	FMKG_SERC	0x02C		/* soft error capture */
#define	FMKG_FDOR	0x034		/* frame data offset */
#define	FMKG_GDV0R	0x038		/* global default value 0 */
#define	FMKG_GDV1R	0x03C		/* global default value 1 */
#define	FMKG_FEER	0x044		/* force error event */
#define	FMKG_AR		0x1FC		/* action register */
#define	  FMKG_AR_GO			  0x80000000
#define	  FMKG_AR_READ			  0x40000000	/* clear = write */
#define	  FMKG_AR_ERR			  0x20000000
#define	  FMKG_AR_SEL_CLS_PLAN		  0x01000000
#define	  FMKG_AR_SEL_PORT		  0x02000000	/* clear = scheme entry */
#define	  FMKG_AR_PORT_WSEL_SP		  0x00008000
#define	  FMKG_AR_PORT_WSEL_CPP		  0x00004000
#define	  FMKG_AR_SCM_WSEL_UPDCNT	  0x00008000
#define	  FMKG_AR_NUM_SHIFT		  16		/* scheme id here */

/*
 * Indirect access sub-region (union of "scheme entry" and "port
 * entry" views, discriminated by the last AR selector written).
 */
#define	FMKG_IND	0x100

/* Scheme-view offsets, relative to FMKG_IND. */
#define	FMKG_SE_MODE	0x00		/* mode + NIA */
#define	  FMKG_SE_MODE_EN	  0x80000000
#define	FMKG_SE_EKFC	0x04		/* extract known fields command */
#define	  FMKG_EKFC_IPSRC1		  0x00100000
#define	  FMKG_EKFC_IPDST1		  0x00080000
#define	  FMKG_EKFC_L4PSRC		  0x00000004
#define	  FMKG_EKFC_L4PDST		  0x00000002
#define	  FMKG_EKFC_IPSEC_SPI		  0x00000200
#define	FMKG_SE_EKDV	0x08		/* extract known default value */
#define	  FMKG_EKDV_IP_ADDR_SHIFT	  18
#define	  FMKG_EKDV_L4_PORT_SHIFT	  8
#define	  FMKG_EKDV_USE_DV0		  2
#define	  FMKG_EKDV_USE_DV1		  3
#define	FMKG_SE_BMCH	0x0C		/* bit mask high */
#define	FMKG_SE_BMCL	0x10		/* bit mask low */
#define	FMKG_SE_FQB	0x14		/* frame queue base */
#define	FMKG_SE_HC	0x18		/* hash command */
#define	  FMKG_SE_HC_SYM			  0x40000000	/* symmetric hash */
#define	  FMKG_SE_HC_SHIFT_SHIFT		  24
#define	  FMKG_SE_HC_MASK_SHIFT		  0
#define	FMKG_SE_PPC	0x1C		/* policer profile command */
#define	FMKG_SE_GEC(i)	(0x20 + (i) * 4) /* generic extract command [0..7] */
#define	FMKG_SE_SPC	0x40		/* statistic packet counter */
#define	FMKG_SE_DV0	0x44		/* default value 0 */
#define	FMKG_SE_DV1	0x48		/* default value 1 */
#define	FMKG_SE_CCBS	0x4C		/* coarse classification */
#define	FMKG_SE_MV	0x50		/* match vector */
#define	FMKG_SE_OM	0x54		/* operation mode */
#define	FMKG_SE_VSP	0x58		/* virtual storage profile */
#define	  FMKG_SE_VSP_NO_KSP_EN		  0x80000000

/* Port-view offsets, relative to FMKG_IND. */
#define	FMKG_PE_SP	0x00		/* port scheme partition */
#define	FMKG_PE_CPP	0x04		/* port classification-plan partition */

/*
 * Default NIA for post-KeyGen
 */
#define	FMKG_ENQ_KG_DFLT_NIA		\
	(NIA_ENG_BMI | NIA_BMI_AC_ENQ_FRAME)

/*
 * Arbitrary fillers used for the "missing field" fallback slots.
 * The exact values don't matter for correctness -- the hash still
 * distributes uniformly across FQs -- but a non-zero pattern makes
 * two frames missing the same field hash to a consistent bucket.
 */
#define	FMKG_DFLT_IPv4_ADDR		0x0a0a0a0a
#define	FMKG_DFLT_L4_PORT		0x0b0b0b0b

/* Sizing. */
#define	FMKG_MAX_SCHEMES		32
#define	FMKG_MAX_HW_PORTS		64

/*
 * AR-write acknowledge wait.  Linux busy-loops with no bound; we
 * cap at ~1 ms of DELAY(1) to survive a wedged block without
 * hanging the boot.
 */
#define	FMKG_AR_WAIT_US			1000

static inline uint32_t
kg_read(struct fman_softc *sc, bus_size_t off)
{
	return (bus_read_4(sc->mem_res, FMAN_KG_OFFSET + off));
}

static inline void
kg_write(struct fman_softc *sc, bus_size_t off, uint32_t val)
{
	bus_write_4(sc->mem_res, FMAN_KG_OFFSET + off, val);
}

static int
kg_ar_write(struct fman_softc *sc, uint32_t ar)
{
	int i;

	kg_write(sc, FMKG_AR, ar);
	for (i = 0; i < FMKG_AR_WAIT_US; i++) {
		ar = kg_read(sc, FMKG_AR);
		if ((ar & FMKG_AR_GO) == 0)
			break;
		DELAY(1);
	}
	if ((ar & FMKG_AR_GO) != 0) {
		device_printf(sc->sc_base.dev,
		    "fman_kg: AR stuck busy (0x%08x)\n", ar);
		return (ETIMEDOUT);
	}
	if ((ar & FMKG_AR_ERR) != 0) {
		device_printf(sc->sc_base.dev,
		    "fman_kg: AR reported error (0x%08x)\n", ar);
		return (EIO);
	}
	return (0);
}

/*
 * Populate the indirect scheme registers for an RSS-style hash
 * scheme and commit it via an AR-write.
 */
static int
kg_program_scheme(struct fman_softc *sc, uint8_t scheme_id,
    uint32_t base_fqid, uint32_t nfqs)
{
	uint32_t ekdv;
	int error;

	kg_write(sc, FMKG_IND + FMKG_SE_MODE,
	    FMKG_SE_MODE_EN | FMKG_ENQ_KG_DFLT_NIA);

	kg_write(sc, FMKG_IND + FMKG_SE_EKFC,
	    FMKG_EKFC_IPSRC1 | FMKG_EKFC_IPDST1 |
	    FMKG_EKFC_L4PSRC | FMKG_EKFC_L4PDST |
	    FMKG_EKFC_IPSEC_SPI);

	ekdv = (FMKG_EKDV_USE_DV0 << FMKG_EKDV_IP_ADDR_SHIFT) |
	    (FMKG_EKDV_USE_DV1 << FMKG_EKDV_L4_PORT_SHIFT);
	kg_write(sc, FMKG_IND + FMKG_SE_EKDV, ekdv);
	kg_write(sc, FMKG_IND + FMKG_SE_DV0, FMKG_DFLT_IPv4_ADDR);
	kg_write(sc, FMKG_IND + FMKG_SE_DV1, FMKG_DFLT_L4_PORT);

	kg_write(sc, FMKG_IND + FMKG_SE_FQB, base_fqid);
	/*
	 * FQID mask width = (nfqs - 1), no hash right-shift.  Symmetric
	 * hash is deliberately off: per NXP's own driver notes,
	 * spreading breaks with SYM set even though the extraction key
	 * is nominally symmetric.
	 */
	kg_write(sc, FMKG_IND + FMKG_SE_HC,
	    (nfqs - 1) << FMKG_SE_HC_MASK_SHIFT);

	/*
	 * TODO: Revisit these registers later, if necessary.
	 */
	kg_write(sc, FMKG_IND + FMKG_SE_BMCH, 0);
	kg_write(sc, FMKG_IND + FMKG_SE_BMCL, 0);
	kg_write(sc, FMKG_IND + FMKG_SE_PPC, 0);
	kg_write(sc, FMKG_IND + FMKG_SE_SPC, 0);
	for (int i = 0; i < 8; i++)
		kg_write(sc, FMKG_IND + FMKG_SE_GEC(i), 0);
	kg_write(sc, FMKG_IND + FMKG_SE_CCBS, 0);
	kg_write(sc, FMKG_IND + FMKG_SE_MV, 0);	/* indirect scheme */
	kg_write(sc, FMKG_IND + FMKG_SE_OM, 0);
	/*
	 * Don't let the scheme override the port's Virtual Storage
	 * Profile.
	 */
	kg_write(sc, FMKG_IND + FMKG_SE_VSP, FMKG_SE_VSP_NO_KSP_EN);
	error = kg_ar_write(sc, FMKG_AR_GO |
	    (scheme_id << FMKG_AR_NUM_SHIFT) | FMKG_AR_SCM_WSEL_UPDCNT);
	if (error != 0)
		device_printf(sc->sc_base.dev,
		    "fman_kg: scheme %u program failed\n", scheme_id);
	return (error);
}

static int
kg_disable_scheme(struct fman_softc *sc, uint8_t scheme_id)
{
	kg_write(sc, FMKG_IND + FMKG_SE_MODE, 0);
	return (kg_ar_write(sc, FMKG_AR_GO | (scheme_id << FMKG_AR_NUM_SHIFT)));
}

/*
 * Read-modify-write the port's scheme-partition bitmap to add or
 * remove @scheme_id.
 */
static int
kg_bind_scheme(struct fman_softc *sc, uint8_t hw_port_id, uint8_t scheme_id,
    bool bind)
{
	uint32_t sp;
	int error;

	error = kg_ar_write(sc,
	    FMKG_AR_GO | FMKG_AR_READ | FMKG_AR_SEL_PORT |
	    hw_port_id | FMKG_AR_PORT_WSEL_SP);
	if (error != 0)
		return (error);

	sp = kg_read(sc, FMKG_IND + FMKG_PE_SP);
	if (bind)
		sp |= (1U << (31 - scheme_id));
	else
		sp &= ~(1U << (31 - scheme_id));
	kg_write(sc, FMKG_IND + FMKG_PE_SP, sp);

	return (kg_ar_write(sc,
	    FMKG_AR_GO | FMKG_AR_SEL_PORT | hw_port_id | FMKG_AR_PORT_WSEL_SP));
}

int
fman_kg_init(struct fman_softc *sc)
{
	int error, i;

	sc->sc_kg_schemes_used = 0;
	for (i = 0; i < FMKG_MAX_HW_PORTS; i++)
		sc->sc_kg_port_scheme[i] = -1;

	kg_write(sc, FMKG_GCR, FMKG_ENQ_KG_DFLT_NIA);
	kg_write(sc, FMKG_EER,
	    FMKG_EER_DOUBLE_ECC | FMKG_EER_KEYSIZE_OVF);

	kg_write(sc, FMKG_FDOR, 0);
	kg_write(sc, FMKG_GDV0R, 0);
	kg_write(sc, FMKG_GDV1R, 0);

	for (i = 0; i < FMKG_MAX_HW_PORTS; i++) {
		kg_write(sc, FMKG_IND + FMKG_PE_SP, 0);
		error = kg_ar_write(sc, FMKG_AR_GO | FMKG_AR_SEL_PORT |
		    i | FMKG_AR_PORT_WSEL_SP);
		if (error != 0)
			return (error);

		kg_write(sc, FMKG_IND + FMKG_PE_CPP, 0);
		error = kg_ar_write(sc, FMKG_AR_GO | FMKG_AR_SEL_PORT |
		    i | FMKG_AR_PORT_WSEL_CPP);
		if (error != 0)
			return (error);
	}

	/* Enable per-scheme error events. */
	kg_write(sc, FMKG_SEER, 0xffffffff);
	kg_write(sc, FMKG_SEEER, 0xffffffff);

	/* TODO: Error handling interrupts -- register with FMan */

	/* Enable the block. */
	kg_write(sc, FMKG_GCR, kg_read(sc, FMKG_GCR) | FMKG_GCR_EN);

	return (0);
}

void
fman_kg_fini(struct fman_softc *sc)
{
	/* No need for a full teardown, just disable the module. */
	kg_write(sc, FMKG_GCR, 0);
	while ((kg_read(sc, FMKG_GSR) & FMKG_GSR_BSY) != 0)
		DELAY(1);
}

int
fman_kg_alloc_hash_scheme(struct fman_softc *sc, int hw_port_id,
    uint32_t base_fqid, uint32_t nfqs)
{
	uint8_t scheme_id;
	int error, i;

	if (hw_port_id < 0 || hw_port_id >= FMKG_MAX_HW_PORTS)
		return (EINVAL);
	if (base_fqid == 0 || (base_fqid & ~0x00ffffff) != 0)
		return (EINVAL);
	if (nfqs == 0 || (nfqs & (nfqs - 1)) != 0)
		return (EINVAL);	/* not a power of two */
	if ((base_fqid & (nfqs - 1)) != 0)
		return (EINVAL);	/* base not aligned to nfqs */
	if (sc->sc_kg_port_scheme[hw_port_id] != -1)
		return (EBUSY);

	for (i = 0; i < FMKG_MAX_SCHEMES; i++) {
		if ((sc->sc_kg_schemes_used & (1U << i)) == 0) {
			scheme_id = i;
			break;
		}
	}
	if (i == FMKG_MAX_SCHEMES)
		return (ENOSPC);

	error = kg_program_scheme(sc, scheme_id, base_fqid, nfqs);
	if (error != 0)
		return (error);

	error = kg_bind_scheme(sc, hw_port_id, scheme_id, true);
	if (error != 0) {
		(void)kg_disable_scheme(sc, scheme_id);
		return (error);
	}

	sc->sc_kg_schemes_used |= (1U << scheme_id);
	sc->sc_kg_port_scheme[hw_port_id] = scheme_id;
	return (0);
}

int
fman_kg_free_hash_scheme(struct fman_softc *sc, int hw_port_id)
{
	int8_t scheme_id;
	int error;

	if (hw_port_id < 0 || hw_port_id >= FMKG_MAX_HW_PORTS)
		return (EINVAL);
	scheme_id = sc->sc_kg_port_scheme[hw_port_id];
	if (scheme_id == -1)
		return (ENOENT);

	error = kg_bind_scheme(sc, hw_port_id, scheme_id, false);
	if (error != 0)
		return (error);
	error = kg_disable_scheme(sc, scheme_id);
	if (error != 0)
		return (error);

	sc->sc_kg_schemes_used &= ~(1U << scheme_id);
	sc->sc_kg_port_scheme[hw_port_id] = -1;
	return (0);
}
