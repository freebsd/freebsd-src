/*
 *   Copyright (c) 1997, 1998 Martin Husemann. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *   3. Neither the name of the author nor the names of any co-contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *   4. Altered versions must be plainly marked as such, and must not be
 *      misrepresented as being the original software and/or documentation.
 *   
 *   THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 *   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *   ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 *   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 *   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 *   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *   SUCH DAMAGE.
 *
 *---------------------------------------------------------------------------
 *
 *	isa_isic.c - ISA bus frontend for i4b_isic driver
 *	--------------------------------------------------
 *
 * $FreeBSD: src/sys/i4b/layer1/isa_isic.c,v 1.5 1999/08/28 00:45:43 peter Exp $ 
 *
 *      last edit-date: [Mon Jul 19 16:39:02 1999]
 *
 *	-mh	original implementation
 *      -hm     NetBSD patches from Martin
 *
 *---------------------------------------------------------------------------*/

#include <sys/types.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/systm.h>
#include <sys/malloc.h>

#include <machine/cpu.h>
#include <machine/intr.h>
#include <machine/bus.h>

#include <dev/isa/isavar.h>

#ifdef __FreeBSD__
#include <machine/i4b_ioctl.h>
#else
#include <i4b/i4b_ioctl.h>
#endif

#include <i4b/layer1/i4b_l1.h>

#if defined(__OpenBSD__)
#define __BROKEN_INDIRECT_CONFIG
#endif

/* local functions */
#ifdef __BROKEN_INDIRECT_CONFIG
static int isa_isic_probe __P((struct device *, void *, void *));
#else
static int isa_isic_probe __P((struct device *, struct cfdata *, void *));
#endif

static void isa_isic_attach __P((struct device *, struct device *, void *));
static int setup_io_map __P((int flags, bus_space_tag_t iot,
	bus_space_tag_t memt, bus_size_t iobase, bus_size_t maddr,
	int *num_mappings, struct isic_io_map *maps, int *iosize, 
	int *msize));
static void args_unmap __P((int *num_mappings, struct isic_io_map *maps));

struct cfattach isa_isic_ca = {
	sizeof(struct isic_softc), isa_isic_probe, isa_isic_attach
};


/*
 * Probe card
 */
static int
#ifdef __BROKEN_INDIRECT_CONFIG
isa_isic_probe(parent, match, aux)
#else
isa_isic_probe(parent, cf, aux)
#endif
	struct device *parent;
#ifdef __BROKEN_INDIRECT_CONFIG
	void *match;
#else
	struct cfdata *cf;
#endif
	void *aux;
{
#ifdef __BROKEN_INDIRECT_CONFIG
	struct cfdata *cf = ((struct device*)match)->dv_cfdata;
#endif
	struct isa_attach_args *ia = aux;
	bus_space_tag_t memt = ia->ia_memt, iot = ia->ia_iot;
	int flags = cf->cf_flags;
	struct isic_attach_args args;
	int ret = 0;

#if 0
	printf("isic%d: enter isa_isic_probe\n", cf->cf_unit);
#endif

	/* check irq */
	if (ia->ia_irq == IRQUNK) {
		printf("isic%d: config error: no IRQ specified\n", cf->cf_unit);
		return 0;
	}

	/* setup MI attach args */
	bzero(&args, sizeof(args));
	args.ia_flags = flags;

	/* if card type specified setup io map for that card */
	switch(flags)
	{
		case FLAG_TELES_S0_8:
		case FLAG_TELES_S0_16:
		case FLAG_TELES_S0_163:
		case FLAG_AVM_A1:
		case FLAG_USR_ISDN_TA_INT:
		case FLAG_ITK_IX1:
			if (setup_io_map(flags, iot, memt, ia->ia_iobase, ia->ia_maddr,
					&args.ia_num_mappings, &args.ia_maps[0],
					&(ia->ia_iosize), &ia->ia_msize)) {
				ret = 0;
				goto done;
			}
			break;

		default:
			/* no io map now, will figure card type later */
			break;
	}

	/* probe card */
	switch(flags)
	{
#ifdef DYNALINK
		case FLAG_DYNALINK:
			ret = isic_probe_Dyn(&args);
			break;
#endif

#ifdef TEL_S0_8
		case FLAG_TELES_S0_8:
			ret = isic_probe_s08(&args);
			break;
#endif

#ifdef TEL_S0_16
		case FLAG_TELES_S0_16:
			ret = isic_probe_s016(&args);
			break;
#endif

#ifdef TEL_S0_16_3
		case FLAG_TELES_S0_163:
			ret = isic_probe_s0163(&args);		
			break;
#endif

#ifdef AVM_A1
		case FLAG_AVM_A1:
			ret = isic_probe_avma1(&args);
			break;
#endif

#ifdef USR_STI
		case FLAG_USR_ISDN_TA_INT:
			ret = isic_probe_usrtai(&args);
			break;
#endif

#ifdef ITKIX1
		case FLAG_ITK_IX1:
			ret = isic_probe_itkix1(&args);
			break;
#endif

		default:
			/* No card type given, try to figure ... */
			if (ia->ia_iobase == IOBASEUNK) {
				ret = 0;
#ifdef TEL_S0_8
				/* only Teles S0/8 will work without IO */
				args.ia_flags = FLAG_TELES_S0_8;
				if (setup_io_map(args.ia_flags, iot, memt, ia->ia_iobase, ia->ia_maddr,
					&args.ia_num_mappings, &args.ia_maps[0],
					&(ia->ia_iosize), &(ia->ia_msize)) == 0)
				{
					ret = isic_probe_s08(&args);
				}
#endif /* TEL_S0_8 */				
			} else if (ia->ia_maddr == MADDRUNK) {
				ret = 0;
#ifdef TEL_S0_16_3
				/* no shared memory, only a 16.3 based card,
				   AVM A1, the usr sportster or an ITK would work */
				args.ia_flags = FLAG_TELES_S0_163;
				if (setup_io_map(args.ia_flags, iot, memt, ia->ia_iobase, ia->ia_maddr,
					&args.ia_num_mappings, &args.ia_maps[0],
					&(ia->ia_iosize), &(ia->ia_msize)) == 0)
				{
					ret = isic_probe_s0163(&args);
					if (ret)
						break;
				}
#endif /* TEL_S0_16_3 */
#ifdef	AVM_A1
				args_unmap(&args.ia_num_mappings, &args.ia_maps[0]);
				args.ia_flags = FLAG_AVM_A1;
				if (setup_io_map(args.ia_flags, iot, memt, ia->ia_iobase, ia->ia_maddr,
					&args.ia_num_mappings, &args.ia_maps[0],
					&(ia->ia_iosize), &(ia->ia_msize)) == 0)
				{
					ret = isic_probe_avma1(&args);
					if (ret)
						break;
				}
#endif /* AVM_A1 */
#ifdef USR_STI
				args_unmap(&args.ia_num_mappings, &args.ia_maps[0]);
				args.ia_flags = FLAG_USR_ISDN_TA_INT;
				if (setup_io_map(args.ia_flags, iot, memt, ia->ia_iobase, ia->ia_maddr,
					&args.ia_num_mappings, &args.ia_maps[0],
					&(ia->ia_iosize), &(ia->ia_msize)) == 0)
				{
					ret = isic_probe_usrtai(&args);
					if (ret)
						break;
				}
#endif /* USR_STI */				

#ifdef ITKIX1
				args_unmap(&args.ia_num_mappings, &args.ia_maps[0]);
				args.ia_flags = FLAG_ITK_IX1;
				if (setup_io_map(args.ia_flags, iot, memt, ia->ia_iobase, ia->ia_maddr,
					&args.ia_num_mappings, &args.ia_maps[0],
					&(ia->ia_iosize), &(ia->ia_msize)) == 0)
				{
					ret = isic_probe_itkix1(&args);
					if (ret)
						break;
				}
#endif /* ITKIX1 */				

			} else {
#ifdef TEL_S0_16_3
				/* could be anything */
				args.ia_flags = FLAG_TELES_S0_163;
				if (setup_io_map(args.ia_flags, iot, memt, ia->ia_iobase, ia->ia_maddr,
					&args.ia_num_mappings, &args.ia_maps[0],
					&(ia->ia_iosize), &(ia->ia_msize)) == 0)
				{
					ret = isic_probe_s0163(&args);
					if (ret)
						break;
				}
#endif /* TEL_S0_16_3 */
#ifdef TEL_S0_16
				args_unmap(&args.ia_num_mappings, &args.ia_maps[0]);
				args.ia_flags = FLAG_TELES_S0_16;
				if (setup_io_map(args.ia_flags, iot, memt, ia->ia_iobase, ia->ia_maddr,
					&args.ia_num_mappings, &args.ia_maps[0],
					&(ia->ia_iosize), &(ia->ia_msize)) == 0)
				{
					ret = isic_probe_s016(&args);
					if (ret)
						break;
				}
#endif /* TEL_S0_16 */
#ifdef AVM_A1
				args_unmap(&args.ia_num_mappings, &args.ia_maps[0]);
				args.ia_flags = FLAG_AVM_A1;
				if (setup_io_map(args.ia_flags, iot, memt, ia->ia_iobase, ia->ia_maddr,
					&args.ia_num_mappings, &args.ia_maps[0],
					&(ia->ia_iosize), &(ia->ia_msize)) == 0)
				{
					ret = isic_probe_avma1(&args);
					if (ret)
						break;
				}
#endif /* AVM_A1 */
#ifdef TEL_S0_8
				args_unmap(&args.ia_num_mappings, &args.ia_maps[0]);
				args.ia_flags = FLAG_TELES_S0_8;
				if (setup_io_map(args.ia_flags, iot, memt, ia->ia_iobase, ia->ia_maddr,
					&args.ia_num_mappings, &args.ia_maps[0],
					&(ia->ia_iosize), &(ia->ia_msize)) == 0)
				{
					ret = isic_probe_s08(&args);
				}
#endif /* TEL_S0_8 */
			}
			break;
	}

done:
	/* unmap resources */
	args_unmap(&args.ia_num_mappings, &args.ia_maps[0]);

#if 0
	printf("isic%d: exit isa_isic_probe, return = %d\n", cf->cf_unit, ret);
#endif

	return ret;
}

/*
 * Attach the card
 */
static void
isa_isic_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct isic_softc *sc = (void *)self;
	struct isa_attach_args *ia = aux;
	int flags = sc->sc_dev.dv_cfdata->cf_flags;
	int ret = 0;
	struct isic_attach_args args;

	/* Setup parameters */
	sc->sc_unit = sc->sc_dev.dv_unit;
	sc->sc_irq = ia->ia_irq;
	sc->sc_maddr = ia->ia_maddr;
	sc->sc_num_mappings = 0;
	sc->sc_maps = NULL;
	switch(flags)
	{
		case FLAG_TELES_S0_8:
		case FLAG_TELES_S0_16:
		case FLAG_TELES_S0_163:
		case FLAG_AVM_A1:
		case FLAG_USR_ISDN_TA_INT:
			setup_io_map(flags, ia->ia_iot, ia->ia_memt, ia->ia_iobase, ia->ia_maddr,
					&(sc->sc_num_mappings), NULL, NULL, NULL);
			MALLOC_MAPS(sc);
			setup_io_map(flags, ia->ia_iot, ia->ia_memt, ia->ia_iobase, ia->ia_maddr,
					&(sc->sc_num_mappings), &(sc->sc_maps[0]), NULL, NULL);
			break;

		default:
			/* No card type given, try to figure ... */

			/* setup MI attach args */
			bzero(&args, sizeof(args));
			args.ia_flags = flags;

			/* Probe cards */
			if (ia->ia_iobase == IOBASEUNK) {
				ret = 0;
#ifdef TEL_S0_8
				/* only Teles S0/8 will work without IO */
				args.ia_flags = FLAG_TELES_S0_8;
				setup_io_map(args.ia_flags, ia->ia_iot, ia->ia_memt, ia->ia_iobase, ia->ia_maddr,
					&args.ia_num_mappings, &args.ia_maps[0], NULL, NULL);
				ret = isic_probe_s08(&args);
				if (ret)
					goto found;
				args_unmap(&args.ia_num_mappings, &args.ia_maps[0]);
#endif /* TEL_S0_8 */
			} else if (ia->ia_maddr == MADDRUNK) {
				/* no shared memory, only a 16.3 based card,
				   AVM A1, the usr sportster or an ITK would work */
				ret = 0;
#ifdef	TEL_S0_16_3
				args.ia_flags = FLAG_TELES_S0_163;
				setup_io_map(args.ia_flags, ia->ia_iot, ia->ia_memt, ia->ia_iobase, ia->ia_maddr,
					&args.ia_num_mappings, &args.ia_maps[0], NULL, NULL);
				ret = isic_probe_s0163(&args);
				if (ret)
					goto found;
				args_unmap(&args.ia_num_mappings, &args.ia_maps[0]);
#endif /* TEL_S0_16_3 */
#ifdef AVM_A1
				args.ia_flags = FLAG_AVM_A1;
				setup_io_map(args.ia_flags, ia->ia_iot, ia->ia_memt, ia->ia_iobase, ia->ia_maddr,
					&args.ia_num_mappings, &args.ia_maps[0], NULL, NULL);
				ret = isic_probe_avma1(&args);
 				if (ret)
 					goto found;
 				args_unmap(&args.ia_num_mappings, &args.ia_maps[0]);
#endif /* AVM_A1 */
#ifdef USR_STI
 				args.ia_flags = FLAG_USR_ISDN_TA_INT;
 				setup_io_map(args.ia_flags, ia->ia_iot, ia->ia_memt, ia->ia_iobase, ia->ia_maddr,
 					&args.ia_num_mappings, &args.ia_maps[0], NULL, NULL);
 				ret = isic_probe_usrtai(&args);
				if (ret)
					goto found;
				args_unmap(&args.ia_num_mappings, &args.ia_maps[0]);
#endif /* USR_STI */
#ifdef ITKIX1
 				args.ia_flags = FLAG_ITK_IX1;
 				setup_io_map(args.ia_flags, ia->ia_iot, ia->ia_memt, ia->ia_iobase, ia->ia_maddr,
 					&args.ia_num_mappings, &args.ia_maps[0], NULL, NULL);
 				ret = isic_probe_itkix1(&args);
				if (ret)
					goto found;
				args_unmap(&args.ia_num_mappings, &args.ia_maps[0]);
#endif /* ITKIX1 */
			} else {
				/* could be anything */
				ret = 0;
#ifdef	TEL_S0_16_3
				args.ia_flags = FLAG_TELES_S0_163;
				setup_io_map(args.ia_flags, ia->ia_iot, ia->ia_memt, ia->ia_iobase, ia->ia_maddr,
					&args.ia_num_mappings, &args.ia_maps[0], NULL, NULL);
				ret = isic_probe_s0163(&args);
				if (ret)
					goto found;
				args_unmap(&args.ia_num_mappings, &args.ia_maps[0]);
#endif	/* TEL_S0_16_3 */
#ifdef	TEL_S0_16
				args.ia_flags = FLAG_TELES_S0_16;
				setup_io_map(args.ia_flags, ia->ia_iot, ia->ia_memt, ia->ia_iobase, ia->ia_maddr,
					&args.ia_num_mappings, &args.ia_maps[0], NULL, NULL);
				ret = isic_probe_s016(&args);
				if (ret)
					goto found;
				args_unmap(&args.ia_num_mappings, &args.ia_maps[0]);
#endif /* TEL_S0_16 */
#ifdef AVM_A1
				args.ia_flags = FLAG_AVM_A1;
				setup_io_map(args.ia_flags, ia->ia_iot, ia->ia_memt, ia->ia_iobase, ia->ia_maddr,
					&args.ia_num_mappings, &args.ia_maps[0], NULL, NULL);
				ret = isic_probe_avma1(&args);
				if (ret)
					goto found;
				args_unmap(&args.ia_num_mappings, &args.ia_maps[0]);
#endif /* AVM_A1 */
#ifdef TEL_S0_8
				args.ia_flags = FLAG_TELES_S0_8;
				setup_io_map(args.ia_flags, ia->ia_iot, ia->ia_memt, ia->ia_iobase, ia->ia_maddr,
					&args.ia_num_mappings, &args.ia_maps[0], NULL, NULL);
				ret = isic_probe_s08(&args);
				if (ret)
					goto found;
				args_unmap(&args.ia_num_mappings, &args.ia_maps[0]);
#endif /* TEL_S0_8 */
			}
			break;

		found:
			flags = args.ia_flags;
			sc->sc_num_mappings = args.ia_num_mappings;
			args_unmap(&args.ia_num_mappings, &args.ia_maps[0]);
			if (ret) {
				MALLOC_MAPS(sc);
				setup_io_map(flags, ia->ia_iot, ia->ia_memt, ia->ia_iobase, ia->ia_maddr,
					&(sc->sc_num_mappings), &(sc->sc_maps[0]), NULL, NULL);
			} else {
				printf(": could not determine card type - not configured!\n");
				return;
			}
			break;
	}

#if defined(__OpenBSD__)	
	isa_intr_establish(ia->ia_ic, ia->ia_irq, IST_EDGE,
		IPL_NET, isicintr, sc, sc->sc_dev.dv_xname);

	/* MI initialization of card */
	isicattach(flags, sc);

#else

	/* MI initialization of card */
	isicattach(flags, sc);

	/*
	 * Try to get a level-triggered interrupt first. If that doesn't
	 * work (like on NetBSD/Atari, try to establish an edge triggered
	 * interrupt.
	 */
	if (isa_intr_establish(ia->ia_ic, ia->ia_irq, IST_LEVEL,
				IPL_NET, isicintr, sc) == NULL) {
		if(isa_intr_establish(ia->ia_ic, ia->ia_irq, IST_EDGE,
				IPL_NET, isicintr, sc) == NULL) {
			args_unmap(&(sc->sc_num_mappings), &(sc->sc_maps[0]));
			free((sc)->sc_maps, M_DEVBUF);
		}
		else {
			/*
			 * XXX: This is a hack that probably needs to be
			 * solved by setting an interrupt type in the sc
			 * structure. I don't feel familiar enough with the
			 * code to do this currently. Feel free to contact
			 * me about it (leo@netbsd.org).
			 */
			isicintr(sc);
		}
	}
#endif
}

/*
 * Setup card specific io mapping. Return 0 on success,
 * any other value on config error.
 * Be prepared to get NULL as maps array.
 * Make sure to keep *num_mappings in sync with the real
 * mappings already setup when returning!
 */
static int
setup_io_map(flags, iot, memt, iobase, maddr, num_mappings, maps, iosize, msize)
	int flags, *num_mappings, *iosize, *msize;
	bus_size_t iobase, maddr;
	bus_space_tag_t iot, memt;
	struct isic_io_map *maps;
{
	/* nothing mapped yet */
	*num_mappings = 0;

	/* which resources do we need? */
	switch(flags)
	{
		case FLAG_TELES_S0_8:
			if (maddr == MADDRUNK) {
				printf("isic: config error: no shared memory specified for Teles S0/8!\n");
				return 1;
			}
			if (iosize) *iosize = 0;	/* no i/o ports */
			if (msize) *msize = 0x1000;	/* shared memory size */

			/* this card uses a single memory mapping */
			if (maps == NULL) {
				*num_mappings = 1;
				return 0;
			}
			*num_mappings = 0;
			maps[0].t = memt;
			maps[0].offset = 0;
			maps[0].size = 0x1000;
			if (bus_space_map(maps[0].t, maddr, 
				maps[0].size, 0, &maps[0].h)) {
				return 1;
			}
			(*num_mappings)++;
			break;

		case FLAG_TELES_S0_16:
			if (iobase == IOBASEUNK) {
				printf("isic: config error: no i/o address specified for Teles S0/16!\n");
				return 1;
			}
			if (maddr == MADDRUNK) {
				printf("isic: config error: no shared memory specified for Teles S0/16!\n");
				return 1;
			}
			if (iosize) *iosize = 8;	/* i/o ports */
			if (msize) *msize = 0x1000;	/* shared memory size */

			/* one io and one memory mapping */
			if (maps == NULL) {
				*num_mappings = 2;
				return 0;
			}
			*num_mappings = 0;
			maps[0].t = iot;
			maps[0].offset = 0;
			maps[0].size = 8;
			if (bus_space_map(maps[0].t, iobase, 
				maps[0].size, 0, &maps[0].h)) {
				return 1;
			}
			(*num_mappings)++;
			maps[1].t = memt;
			maps[1].offset = 0;
			maps[1].size = 0x1000;
			if (bus_space_map(maps[1].t, maddr, 
				maps[1].size, 0, &maps[1].h)) {
				return 1;
			}
			(*num_mappings)++;
			break;

		case FLAG_TELES_S0_163:
			if (iobase == IOBASEUNK) {
				printf("isic: config error: no i/o address specified for Teles S0/16!\n");
				return 1;
			}
			if (iosize) *iosize = 8;	/* only some i/o ports shown */
			if (msize) *msize = 0;		/* no shared memory */

			/* Four io mappings: config, isac, 2 * hscx */
			if (maps == NULL) {
				*num_mappings = 4;
				return 0;
			}
			*num_mappings = 0;
			maps[0].t = iot;
			maps[0].offset = 0;
			maps[0].size = 8;
			if (bus_space_map(maps[0].t, iobase, 
				maps[0].size, 0, &maps[0].h)) {
				return 1;
			}
			(*num_mappings)++;
			maps[1].t = iot;
			maps[1].offset = 0;
			maps[1].size = 0x40;	/* XXX - ??? */
			if ((iobase - 0xd80 + 0x980) < 0 || (iobase - 0xd80 + 0x980) > 0x0ffff)
				return 1;
			if (bus_space_map(maps[1].t, iobase - 0xd80 + 0x980, 
				maps[1].size, 0, &maps[1].h)) {
				return 1;
			}
			(*num_mappings)++;
			maps[2].t = iot;
			maps[2].offset = 0;
			maps[2].size = 0x40;	/* XXX - ??? */
			if ((iobase - 0xd80 + 0x180) < 0 || (iobase - 0xd80 + 0x180) > 0x0ffff)
				return 1;
			if (bus_space_map(maps[2].t, iobase - 0xd80 + 0x180, 
				maps[2].size, 0, &maps[2].h)) {
				return 1;
			}
			(*num_mappings)++;
			maps[3].t = iot;
			maps[3].offset = 0;
			maps[3].size = 0x40;	/* XXX - ??? */
			if ((iobase - 0xd80 + 0x580) < 0 || (iobase - 0xd80 + 0x580) > 0x0ffff)
				return 1;
			if (bus_space_map(maps[3].t, iobase - 0xd80 + 0x580, 
				maps[3].size, 0, &maps[3].h)) {
				return 1;
			}
			(*num_mappings)++;
			break;

		case FLAG_AVM_A1:
			if (iobase == IOBASEUNK) {
				printf("isic: config error: no i/o address specified for AVM A1/Fritz! card!\n");
				return 1;
			}
			if (iosize) *iosize = 8;	/* only some i/o ports shown */
			if (msize) *msize = 0;		/* no shared memory */

			/* Seven io mappings: config, isac, 2 * hscx,
			   isac-fifo, 2 * hscx-fifo */
			if (maps == NULL) {
				*num_mappings = 7;
				return 0;
			}
			*num_mappings = 0;
			maps[0].t = iot;	/* config */
			maps[0].offset = 0;
			maps[0].size = 8;
			if ((iobase + 0x1800) < 0 || (iobase + 0x1800) > 0x0ffff)
				return 1;
			if (bus_space_map(maps[0].t, iobase + 0x1800, maps[0].size, 0, &maps[0].h))
				return 1;
			(*num_mappings)++;
			maps[1].t = iot;	/* isac */
			maps[1].offset = 0;
			maps[1].size = 0x80;	/* XXX - ??? */
			if ((iobase + 0x1400 - 0x20) < 0 || (iobase + 0x1400 - 0x20) > 0x0ffff)
				return 1;
			if (bus_space_map(maps[1].t, iobase + 0x1400 - 0x20, maps[1].size, 0, &maps[1].h))
				return 1;
			(*num_mappings)++;
			maps[2].t = iot;	/* hscx 0 */
			maps[2].offset = 0;
			maps[2].size = 0x40;	/* XXX - ??? */
			if ((iobase + 0x400 - 0x20) < 0 || (iobase + 0x400 - 0x20) > 0x0ffff)
				return 1;
			if (bus_space_map(maps[2].t, iobase + 0x400 - 0x20, maps[2].size, 0, &maps[2].h))
				return 1;
			(*num_mappings)++;
			maps[3].t = iot;	/* hscx 1 */
			maps[3].offset = 0;
			maps[3].size = 0x40;	/* XXX - ??? */
			if ((iobase + 0xc00 - 0x20) < 0 || (iobase + 0xc00 - 0x20) > 0x0ffff)
				return 1;
			if (bus_space_map(maps[3].t, iobase + 0xc00 - 0x20, maps[3].size, 0, &maps[3].h))
				return 1;
			(*num_mappings)++;
			maps[4].t = iot;	/* isac-fifo */
			maps[4].offset = 0;
			maps[4].size = 1;
			if ((iobase + 0x1400 - 0x20 -0x3e0) < 0 || (iobase + 0x1400 - 0x20 -0x3e0) > 0x0ffff)
				return 1;
			if (bus_space_map(maps[4].t, iobase + 0x1400 - 0x20 -0x3e0, maps[4].size, 0, &maps[4].h))
				return 1;
			(*num_mappings)++;
			maps[5].t = iot;	/* hscx 0 fifo */
			maps[5].offset = 0;
			maps[5].size = 1;
			if ((iobase + 0x400 - 0x20 -0x3e0) < 0 || (iobase + 0x400 - 0x20 -0x3e0) > 0x0ffff)
				return 1;
			if (bus_space_map(maps[5].t, iobase + 0x400 - 0x20 -0x3e0, maps[5].size, 0, &maps[5].h))
				return 1;
			(*num_mappings)++;
			maps[6].t = iot;	/* hscx 1 fifo */
			maps[6].offset = 0;
			maps[6].size = 1;
			if ((iobase + 0xc00 - 0x20 -0x3e0) < 0 || (iobase + 0xc00 - 0x20 -0x3e0) > 0x0ffff)
				return 1;
			if (bus_space_map(maps[6].t, iobase + 0xc00 - 0x20 -0x3e0, maps[6].size, 0, &maps[6].h))
				return 1;
			(*num_mappings)++;
			break;

		case FLAG_USR_ISDN_TA_INT:
			if (iobase == IOBASEUNK) {
				printf("isic: config error: no I/O base specified for USR Sportster TA intern!\n");
				return 1;
			}
			if (iosize) *iosize = 8;	/* scattered ports, only some shown */
			if (msize) *msize = 0;		/* no shared memory */

			/* 49 io mappings: 1 config and 48x8 registers */
			if (maps == NULL) {
				*num_mappings = 49;
				return 0;
			}
			*num_mappings = 0;
			{
				int i, num;
				bus_size_t base;

				/* config at offset 0x8000 */
				base = iobase + 0x8000;
				maps[0].size = 1;
				maps[0].t = iot;
				maps[0].offset = 0;
				if (base < 0 || base > 0x0ffff)
					return 1;
				if (bus_space_map(iot, base, 1, 0, &maps[0].h)) {
					return 1;
				}
				*num_mappings = num = 1;

				/* HSCX A at offset 0 */
				base = iobase;
				for (i = 0; i < 16; i++) {
					maps[num].size = 8;
					maps[num].offset = 0;
					maps[num].t = iot;
					if (base+i*1024 < 0 || base+i*1024+8 > 0x0ffff)
						return 1;
					if (bus_space_map(iot, base+i*1024, 8, 0, &maps[num].h)) {
						return 1;
					}
					*num_mappings = ++num;
				}
				/* HSCX B at offset 0x4000 */
				base = iobase + 0x4000;
				for (i = 0; i < 16; i++) {
					maps[num].size = 8;
					maps[num].offset = 0;
					maps[num].t = iot;
					if (base+i*1024 < 0 || base+i*1024+8 > 0x0ffff)
						return 1;
					if (bus_space_map(iot, base+i*1024, 8, 0, &maps[num].h)) {
						return 1;
					}
					*num_mappings = ++num;
				}
				/* ISAC at offset 0xc000 */
				base = iobase + 0xc000;
				for (i = 0; i < 16; i++) {
					maps[num].size = 8;
					maps[num].offset = 0;
					maps[num].t = iot;
					if (base+i*1024 < 0 || base+i*1024+8 > 0x0ffff)
						return 1;
					if (bus_space_map(iot, base+i*1024, 8, 0, &maps[num].h)) {
						return 1;
					}
					*num_mappings = ++num;
				}
			}
			break;

		case FLAG_ITK_IX1:
			if (iobase == IOBASEUNK) {
				printf("isic: config error: no I/O base specified for ITK ix1 micro!\n");
				return 1;
			}
			if (iosize) *iosize = 4;
			if (msize) *msize = 0;
			if (maps == NULL) {
				*num_mappings = 1;
				return 0;
			}
			*num_mappings = 0;
			maps[0].size = 4;
			maps[0].t = iot;
			maps[0].offset = 0;
			if (bus_space_map(iot, iobase, 4, 0, &maps[0].h)) {
				return 1;
			}
			*num_mappings = 1;
  			break;

		default:
			printf("isic: config error: flags do not specify any known card!\n");
			return 1;
			break;
	}

	return 0;
}

static void
args_unmap(num_mappings, maps)
	int *num_mappings;
	struct isic_io_map *maps;
{
	int i, n;
	for (i = 0, n = *num_mappings; i < n; i++)
        	if (maps[i].size)
			bus_space_unmap(maps[i].t, maps[i].h, maps[i].size);
	*num_mappings = 0;
}
