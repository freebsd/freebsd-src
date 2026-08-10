/*-
 * Copyright (c) 2026 Justin Hibbits
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef	_DEV_DPAA_FMAN_KEYGEN_H_
#define	_DEV_DPAA_FMAN_KEYGEN_H_

struct fman_softc;

/*
 * Bring up the FMan KeyGen block once per FMan instance.  Enables error
 * event reporting and clears all port/scheme bindings.  Called from
 * fman_init() in fman.c.
 */
int	fman_kg_init(struct fman_softc *sc);
void	fman_kg_fini(struct fman_softc *sc);

/*
 * Configure a hash-distribution scheme for @hw_port_id: RSS-style
 * spreading over @nfqs FQs starting at @base_fqid, keyed on the IP
 * 5-tuple.  @nfqs must be a power of two, @base_fqid must be aligned
 * to @nfqs, and both fit in 24 bits (Linux constraint, matches the
 * FMKG_SCM_FQB field width).  Returns 0 on success or a non-zero
 * errno; on success the scheme is bound to the port immediately and
 * incoming frames start hashing on the next dequeue cycle.
 */
int	fman_kg_alloc_hash_scheme(struct fman_softc *sc, int hw_port_id,
	    uint32_t base_fqid, uint32_t nfqs);

/*
 * Undo fman_kg_alloc_hash_scheme() for @hw_port_id.  Unbinds the
 * scheme from the port, disables it, and releases the scheme slot.
 * Frames after this point fall through to the port's dflt_fqid.
 */
int	fman_kg_free_hash_scheme(struct fman_softc *sc, int hw_port_id);

#endif /* _DEV_DPAA_FMAN_KEYGEN_H_ */
