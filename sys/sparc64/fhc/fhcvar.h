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
	struct cdev		*sc_led_dev;
};

device_probe_t fhc_probe;
device_attach_t fhc_attach;

bus_print_child_t fhc_print_child;
bus_probe_nomatch_t fhc_probe_nomatch;
bus_setup_intr_t fhc_setup_intr;
bus_teardown_intr_t fhc_teardown_intr;
bus_alloc_resource_t fhc_alloc_resource;
bus_get_resource_list_t fhc_get_resource_list;

ofw_bus_get_devinfo_t fhc_get_devinfo;

#endif /* !_SPARC64_FHC_FHCVAR_H_ */
