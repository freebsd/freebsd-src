/*-
 * Copyright (c) 2012 Semihalf.
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
 */

#ifndef DPAA_PORTALS_H
#define	DPAA_PORTALS_H

struct dpaa_portal_softc {
	device_t	sc_dev;			/* device handle */
	vm_paddr_t	sc_ce_pa;		/* portal's CE PA */
	vm_offset_t	sc_ce_va;
	vm_paddr_t	sc_ci_pa;		/* portal's CI PA */
	vm_offset_t	sc_ci_va;
	int		sc_cpu;
	uint32_t	sc_ce_size;		/* portal's CE size */
	uint32_t	sc_ci_size;		/* portal's CI size */
	struct resource	*sc_mres[2];		/* memory resource */
	struct resource	*sc_ires;		/* Interrupt */
	void		*sc_intr_cookie;
	bool		sc_regs_mapped;		/* register mapping status */
};

int bman_portal_attach(device_t, int);
int bman_portal_detach(device_t);

int qman_portal_attach(device_t, int);
int qman_portal_detach(device_t);

int dpaa_portal_alloc_res(device_t, int);
void dpaa_portal_map_registers(struct dpaa_portal_softc *);
#endif
