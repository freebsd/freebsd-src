/*-
 * Copyright (c) 2003 Jake Burkholder.
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
 *
 * $FreeBSD$
 */

#ifndef _SPARC64_FHC_FHCVAR_H_
#define	_SPARC64_FHC_FHCVAR_H_

#define	FHC_CENTRAL		(1<<0)

struct fhc_softc {
	phandle_t		sc_node;
	struct resource *	sc_memres[FHC_NREG];
	bus_space_handle_t	sc_bh[FHC_NREG];
	bus_space_tag_t		sc_bt[FHC_NREG];
	int			sc_nrange;
	struct sbus_ranges	*sc_ranges;
	int			sc_board;
	int			sc_ign;
	int			sc_flags;
};

int fhc_probe(device_t dev);
int fhc_attach(device_t dev);

int fhc_print_child(device_t dev, device_t child);
void fhc_probe_nomatch(device_t dev, device_t child);
int fhc_setup_intr(device_t, device_t, struct resource *, int, driver_intr_t *,
    void *, void **);
int fhc_teardown_intr(device_t, device_t, struct resource *, void *);
struct resource *fhc_alloc_resource(device_t, device_t, int, int *, u_long,
    u_long, u_long, u_int);
int fhc_release_resource(device_t, device_t, int, int, struct resource *);
ofw_bus_get_compat_t fhc_get_compat;
ofw_bus_get_model_t fhc_get_model;
ofw_bus_get_name_t fhc_get_name;
ofw_bus_get_node_t fhc_get_node;
ofw_bus_get_type_t fhc_get_type;

#endif
