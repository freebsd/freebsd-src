/*	$NetBSD$	*/
/*
 * [NetBSD for NEC PC98 series]
 *  Copyright (c) 1995, 1996 NetBSD/pc98 porting staff.
 *  All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
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

#include <dev/isa/bs/bsif.h>

static int bs_pisa_probe __P((struct device *, void *, void *));
static void bs_pisa_attach __P((struct device *, struct device *, void *));
static int bs_deactivate __P((pisa_device_args_t));
static int bs_activate __P((pisa_device_args_t));

struct cfattach bs_pisa_ca = {
	sizeof(struct bs_softc), bs_pisa_probe, bs_pisa_attach
};

struct pisa_driver bs_pd = {
	bs_activate, bs_deactivate,
};

static int
bs_pisa_probe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct bs_softc *sc = match;
	struct pisa_attach_args *pa = aux;
	struct isa_attach_args *ia = &pa->pa_ia;

	if (ia->ia_iobase == IOBASEUNK ||
	    ia->ia_irq == IRQUNK || ia->ia_drq == DRQUNK)
		return 0;

	sc->sc_pdv = pa->pa_pdv;

	return bsprobe(parent, match, ia);
}

static void
bs_pisa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct bs_softc *sc = (void *) self;
	struct pisa_attach_args *pa = aux;

	sc->sc_pdv = PISAMSG_BIND(pa->pa_pdv, sc, &bs_pd);

	bsattach(parent, self, (void *) &pa->pa_ia);

	PISA_INTR_REGISTER(sc->sc_pdv, sc->sc_ih);
}

static int
bs_deactivate(arg)
	pisa_device_args_t arg;
{
	struct bs_softc *bsc = arg->id;

	bsc->sc_flags |= BSINACTIVE;
	bs_terminate_timeout(bsc);

	return 0;
}

#define	SCSIBUS_RESCAN

static int
bs_activate(arg)
	pisa_device_args_t arg;
{
	struct bs_softc *bsc = arg->id;
	struct isa_attach_args *ia = arg->ia;
	struct targ_info *ti;
	int i;

	bsc->sc_irqmasks = (1 << ia->ia_irq);

	while((ti = bsc->sc_titab.tqh_first) != NULL)
		TAILQ_REMOVE(&bsc->sc_titab, ti, ti_tchain);

	bsc->sc_openf = 0;
	for (i = 0; i < NTARGETS; i ++)
		if (i != bsc->sc_hostid && (ti = bsc->sc_ti[i]) != NULL)
		{
			TAILQ_INSERT_TAIL(&bsc->sc_titab, ti, ti_tchain);
			bsc->sc_openf |= (1 << i);
		}

	bsc->sc_hstate = BSC_BOOTUP;
	bsc->sc_flags &= ~BSINACTIVE;
	bs_reset_nexus(bsc);

#ifdef	SCSIBUS_RESCAN
	if (bsc->sc_nexus == NULL)
		scsi_probe_busses((int) bsc->sc_link.scsibus, -1, -1);
#endif

	bs_start_timeout(bsc);
	return 0;
}
