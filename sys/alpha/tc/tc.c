/* $FreeBSD$ */
/*
 * Copyright (c) 1994, 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include "opt_cpu.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/malloc.h>

#include <machine/rpb.h>
#include <alpha/tc/tcreg.h>
#include <alpha/tc/tcvar.h>
#include <alpha/tc/tcdevs.h>
#include <alpha/tc/ioasicreg.h>

/*#include <alpha/tc/dwlpxreg.h>*/

#define KV(pa)			ALPHA_PHYS_TO_K0SEG(pa)

static devclass_t	tc_devclass;
device_t		tc0;		/* XXX only one for now */

struct tc_softc {
	device_t sc_dv;
	int     sc_speed;
        int     sc_nslots;
	int     nbuiltins;
	struct tc_builtin     *builtins;
        struct tc_slotdesc *sc_slots;
        void    (*sc_intr_establish) __P((struct device *, void *,
                    tc_intrlevel_t, int (*)(void *), void *));
        void    (*sc_intr_disestablish) __P((struct device *, void *));
/*        bus_dma_tag_t (*sc_get_dma_tag) __P((int));
*/
};
#define NTC_ROMOFFS     2
static tc_offset_t tc_slot_romoffs[NTC_ROMOFFS] = {
        TC_SLOT_ROM,
        TC_SLOT_PROTOROM,
};


#define TC_SOFTC(dev)	(struct tc_softc*) device_get_softc(dev)

static int tc_probe(device_t dev);
static int tc_attach(device_t dev);
int    tc_checkslot(        tc_addr_t slotbase, char *namep);

static device_method_t tc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		tc_probe),
	DEVMETHOD(device_attach,	tc_attach),
	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	{ 0, 0 },
};

static driver_t tc_driver = {
	"tc",
	tc_methods,
	sizeof(struct tc_softc),
};

#define C(x)    ((void *)(u_long)x)

int     tc_intrnull __P((void *));
struct tcintr {
        int     (*tci_func) __P((void *));
        void    *tci_arg;
};

#ifdef DEC_3000_300

void    tc_3000_300_intr_setup __P((void));
void    tc_3000_300_intr_establish __P((struct device *, void *,
            tc_intrlevel_t, int (*)(void *), void *));
void    tc_3000_300_intr_disestablish __P((struct device *, void *));
void    tc_3000_300_iointr __P((void *, unsigned long));



#define DEC_3000_300_IOASIC_ADDR        KV(0x1a0000000)

struct tc_slotdesc tc_3000_300_slots[] = {
        { KV(0x100000000), C(TC_3000_300_DEV_OPT0), },  /* 0 - opt slot 0 */
        { KV(0x120000000), C(TC_3000_300_DEV_OPT1), },  /* 1 - opt slot 1 */
        { KV(0x180000000), C(TC_3000_300_DEV_BOGUS), }, /* 2 - TCDS ASIC */
        { KV(0x1a0000000), C(TC_3000_300_DEV_BOGUS), }, /* 3 - IOCTL ASIC */
        { KV(0x1c0000000), C(TC_3000_300_DEV_CXTURBO), }, /* 4 - CXTurbo */
};
int tc_3000_300_nslots =
    sizeof(tc_3000_300_slots) / sizeof(tc_3000_300_slots[0]);

struct tc_builtin tc_3000_300_builtins[] = {
#ifdef notyet
        { "PMAGB-BA",   4, 0x02000000, C(TC_3000_300_DEV_CXTURBO),      },
#endif
        { "ioasic",   3, 0x00000000, C(TC_3000_300_DEV_IOASIC),       },
        { "tcds",   2, 0x00000000, C(TC_3000_300_DEV_TCDS),         },
};
int tc_3000_300_nbuiltins =
    sizeof(tc_3000_300_builtins) / sizeof(tc_3000_300_builtins[0]);

struct tcintr tc_3000_300_intr[TC_3000_300_NCOOKIES];


#endif /* DEC_3000_300 */

#ifdef DEC_3000_500
void    tc_3000_500_intr_setup __P((void));
void    tc_3000_500_intr_establish __P((struct device *, void *,
            tc_intrlevel_t, int (*)(void *), void *));
void    tc_3000_500_intr_disestablish __P((struct device *, void *));
void    tc_3000_500_iointr __P((void *, unsigned long));

struct tc_slotdesc tc_3000_500_slots[] = {
        { KV(0x100000000), C(TC_3000_500_DEV_OPT0), },  /* 0 - opt slot 0 */
        { KV(0x120000000), C(TC_3000_500_DEV_OPT1), },  /* 1 - opt slot 1 */
        { KV(0x140000000), C(TC_3000_500_DEV_OPT2), },  /* 2 - opt slot 2 */
        { KV(0x160000000), C(TC_3000_500_DEV_OPT3), },  /* 3 - opt slot 3 */
        { KV(0x180000000), C(TC_3000_500_DEV_OPT4), },  /* 4 - opt slot 4 */
        { KV(0x1a0000000), C(TC_3000_500_DEV_OPT5), },  /* 5 - opt slot 5 */
        { KV(0x1c0000000), C(TC_3000_500_DEV_BOGUS), }, /* 6 - TCDS ASIC */
        { KV(0x1e0000000), C(TC_3000_500_DEV_BOGUS), }, /* 7 - IOCTL ASIC */
};
int tc_3000_500_nslots =
    sizeof(tc_3000_500_slots) / sizeof(tc_3000_500_slots[0]);

struct tc_builtin tc_3000_500_builtins[] = {
        { "ioasic",   7, 0x00000000, C(TC_3000_500_DEV_IOASIC),       },
#ifdef notyet
        { "PMAGB-BA",   7, 0x02000000, C(TC_3000_500_DEV_CXTURBO),      },
#endif
        { "tcds",   6, 0x00000000, C(TC_3000_500_DEV_TCDS),         },
};
int tc_3000_500_nbuiltins = sizeof(tc_3000_500_builtins) /
    sizeof(tc_3000_500_builtins[0]);

u_int32_t tc_3000_500_intrbits[TC_3000_500_NCOOKIES] = {
        TC_3000_500_IR_OPT0,
        TC_3000_500_IR_OPT1,
        TC_3000_500_IR_OPT2,
        TC_3000_500_IR_OPT3,
        TC_3000_500_IR_OPT4,
        TC_3000_500_IR_OPT5,
        TC_3000_500_IR_TCDS,
        TC_3000_500_IR_IOASIC,
        TC_3000_500_IR_CXTURBO,
};

struct tcintr tc_3000_500_intr[TC_3000_500_NCOOKIES];

u_int32_t tc_3000_500_imask;    /* intrs we want to ignore; mirrors IMR. */


#endif /* DEC_3000_500 */


#ifdef DEC_3000_300

void
tc_3000_300_intr_setup()
{
        volatile u_int32_t *imskp;
        u_long i;

        /*
         * Disable all interrupts that we can (can't disable builtins).
         */
        imskp = (volatile u_int32_t *)IOASIC_REG_IMSK(DEC_3000_300_IOASIC_ADDR);
        *imskp &= ~(IOASIC_INTR_300_OPT0 | IOASIC_INTR_300_OPT1);

        /*
         * Set up interrupt handlers.
         */
        for (i = 0; i < TC_3000_300_NCOOKIES; i++) {
                tc_3000_300_intr[i].tci_func = tc_intrnull;
                tc_3000_300_intr[i].tci_arg = (void *)i;
        }
}

void
tc_3000_300_intr_establish(tcadev, cookie, level, func, arg)
        struct device *tcadev;
        void *cookie, *arg;
        tc_intrlevel_t level;
        int (*func) __P((void *));
{
        volatile u_int32_t *imskp;
        u_long dev = (u_long)cookie;

#ifdef DIAGNOSTIC
        /* XXX bounds-check cookie. */
#endif

        if (tc_3000_300_intr[dev].tci_func != tc_intrnull)
                panic("tc_3000_300_intr_establish: cookie %ld twice", dev);

        tc_3000_300_intr[dev].tci_func = func;
        tc_3000_300_intr[dev].tci_arg = arg;

        imskp = (volatile u_int32_t *)IOASIC_REG_IMSK(DEC_3000_300_IOASIC_ADDR);
        switch (dev) {
        case TC_3000_300_DEV_OPT0:
                *imskp |= IOASIC_INTR_300_OPT0;
                break;
        case TC_3000_300_DEV_OPT1:
                *imskp |= IOASIC_INTR_300_OPT1;
                break;
        default:
                /* interrupts for builtins always enabled */
                break;
        }
}

void
tc_3000_300_intr_disestablish(tcadev, cookie)
        struct device *tcadev;
        void *cookie;
{
        volatile u_int32_t *imskp;
        u_long dev = (u_long)cookie;

#ifdef DIAGNOSTIC
        /* XXX bounds-check cookie. */
#endif

        if (tc_3000_300_intr[dev].tci_func == tc_intrnull)
                panic("tc_3000_300_intr_disestablish: cookie %ld bad intr",
                    dev);

        imskp = (volatile u_int32_t *)IOASIC_REG_IMSK(DEC_3000_300_IOASIC_ADDR);
        switch (dev) {
        case TC_3000_300_DEV_OPT0:
                *imskp &= ~IOASIC_INTR_300_OPT0;
                break;
        case TC_3000_300_DEV_OPT1:
                *imskp &= ~IOASIC_INTR_300_OPT1;
                break;
        default:
                /* interrupts for builtins always enabled */
                break;
        }

        tc_3000_300_intr[dev].tci_func = tc_intrnull;
        tc_3000_300_intr[dev].tci_arg = (void *)dev;
}

void
tc_3000_300_iointr(framep, vec)
        void *framep;
        unsigned long vec;
{
        u_int32_t tcir, ioasicir, ioasicimr;
        int ifound;

#ifdef DIAGNOSTIC
        int s;
        if (vec != 0x800)
                panic("INVALID ASSUMPTION: vec 0x%lx, not 0x800", vec);
        s = splhigh();
        if (s != ALPHA_PSL_IPL_IO)
                panic("INVALID ASSUMPTION: IPL %d, not %d", s,
                    ALPHA_PSL_IPL_IO);
        splx(s);
#endif

        do {
                tc_syncbus();

                /* find out what interrupts/errors occurred */
                tcir = *(volatile u_int32_t *)TC_3000_300_IR;
                ioasicir = *(volatile u_int32_t *)
                    IOASIC_REG_INTR(DEC_3000_300_IOASIC_ADDR);
                ioasicimr = *(volatile u_int32_t *)
                    IOASIC_REG_IMSK(DEC_3000_300_IOASIC_ADDR);
                tc_mb();

                /* Ignore interrupts that aren't enabled out. */
                ioasicir &= ioasicimr;

                /* clear the interrupts/errors we found. */
                *(volatile u_int32_t *)TC_3000_300_IR = tcir;
                /* XXX can't clear TC option slot interrupts here? */
                tc_wmb();

                ifound = 0;


#define CHECKINTR(slot, flag)                                           \
                if (flag) {                                             \
                        ifound = 1;                                     \
                        (*tc_3000_300_intr[slot].tci_func)              \
                            (tc_3000_300_intr[slot].tci_arg);           \
                }
                /* Do them in order of priority; highest slot # first. */
                CHECKINTR(TC_3000_300_DEV_CXTURBO,
                    tcir & TC_3000_300_IR_CXTURBO);
                CHECKINTR(TC_3000_300_DEV_IOASIC,
                    (tcir & TC_3000_300_IR_IOASIC) &&
                    (ioasicir & ~(IOASIC_INTR_300_OPT1|IOASIC_INTR_300_OPT0)));
                CHECKINTR(TC_3000_300_DEV_TCDS, tcir & TC_3000_300_IR_TCDS);
                CHECKINTR(TC_3000_300_DEV_OPT1,
                    ioasicir & IOASIC_INTR_300_OPT1);
                CHECKINTR(TC_3000_300_DEV_OPT0,
                    ioasicir & IOASIC_INTR_300_OPT0);
#undef CHECKINTR

#ifdef DIAGNOSTIC
#define PRINTINTR(msg, bits)                                            \
        if (tcir & bits)                                                \
                printf(msg);
                PRINTINTR("BCache tag parity error\n",
                    TC_3000_300_IR_BCTAGPARITY);
                PRINTINTR("TC overrun error\n", TC_3000_300_IR_TCOVERRUN);
                PRINTINTR("TC I/O timeout\n", TC_3000_300_IR_TCTIMEOUT);
                PRINTINTR("Bcache parity error\n",
                    TC_3000_300_IR_BCACHEPARITY);
                PRINTINTR("Memory parity error\n", TC_3000_300_IR_MEMPARITY);
#undef PRINTINTR
#endif
        } while (ifound);
}

#endif /* DEC_3000_300 */

#ifdef DEC_3000_500 

void
tc_3000_500_intr_setup()
{
        u_long i;

        /*
         * Disable all slot interrupts.  Note that this cannot
         * actually disable CXTurbo, TCDS, and IOASIC interrupts.
         */
        tc_3000_500_imask = *(volatile u_int32_t *)TC_3000_500_IMR_READ;
        for (i = 0; i < TC_3000_500_NCOOKIES; i++)
                tc_3000_500_imask |= tc_3000_500_intrbits[i];
        *(volatile u_int32_t *)TC_3000_500_IMR_WRITE = tc_3000_500_imask;
        tc_mb();

        /*
         * Set up interrupt handlers.
         */
        for (i = 0; i < TC_3000_500_NCOOKIES; i++) {
                tc_3000_500_intr[i].tci_func = tc_intrnull;
                tc_3000_500_intr[i].tci_arg = (void *)i;
        }
}

void
tc_3000_500_intr_establish(tcadev, cookie, level, func, arg)
        struct device *tcadev;
        void *cookie, *arg;
        tc_intrlevel_t level;
        int (*func) __P((void *));
{
        u_long dev = (u_long)cookie;

#ifdef DIAGNOSTIC
        /* XXX bounds-check cookie. */
#endif

        if (tc_3000_500_intr[dev].tci_func != tc_intrnull)
                panic("tc_3000_500_intr_establish: cookie %ld twice", dev);

        tc_3000_500_intr[dev].tci_func = func;
        tc_3000_500_intr[dev].tci_arg = arg;

        tc_3000_500_imask &= ~tc_3000_500_intrbits[dev];
        *(volatile u_int32_t *)TC_3000_500_IMR_WRITE = tc_3000_500_imask;
        tc_mb();
}

void
tc_3000_500_intr_disestablish(tcadev, cookie)
        struct device *tcadev;
        void *cookie;
{
        u_long dev = (u_long)cookie;

#ifdef DIAGNOSTIC
        /* XXX bounds-check cookie. */
#endif

        if (tc_3000_500_intr[dev].tci_func == tc_intrnull)
                panic("tc_3000_500_intr_disestablish: cookie %ld bad intr",
                    dev);

        tc_3000_500_imask |= tc_3000_500_intrbits[dev];
        *(volatile u_int32_t *)TC_3000_500_IMR_WRITE = tc_3000_500_imask;
        tc_mb();

        tc_3000_500_intr[dev].tci_func = tc_intrnull;
        tc_3000_500_intr[dev].tci_arg = (void *)dev;
}

void
tc_3000_500_iointr(framep, vec)
        void *framep;
        unsigned long vec;
{
        u_int32_t ir;
        int ifound;

#ifdef DIAGNOSTIC
        int s;
        if (vec != 0x800)
                panic("INVALID ASSUMPTION: vec 0x%lx, not 0x800", vec);
        s = splhigh();
        if (s != ALPHA_PSL_IPL_IO)
                panic("INVALID ASSUMPTION: IPL %d, not %d", s,
                    ALPHA_PSL_IPL_IO);
        splx(s);
#endif

        do {
                tc_syncbus();
                ir = *(volatile u_int32_t *)TC_3000_500_IR_CLEAR;

                /* Ignore interrupts that we haven't enabled. */
                ir &= ~(tc_3000_500_imask & 0x1ff);

                ifound = 0;

#define CHECKINTR(slot)                                                 \
                if (ir & tc_3000_500_intrbits[slot]) {                  \
                        ifound = 1;                                     \
                        (*tc_3000_500_intr[slot].tci_func)              \
                            (tc_3000_500_intr[slot].tci_arg);           \
                }
                /* Do them in order of priority; highest slot # first. */
                CHECKINTR(TC_3000_500_DEV_CXTURBO);
                CHECKINTR(TC_3000_500_DEV_IOASIC);
                CHECKINTR(TC_3000_500_DEV_TCDS);
                CHECKINTR(TC_3000_500_DEV_OPT5);
                CHECKINTR(TC_3000_500_DEV_OPT4);
                CHECKINTR(TC_3000_500_DEV_OPT3);
                CHECKINTR(TC_3000_500_DEV_OPT2);
                CHECKINTR(TC_3000_500_DEV_OPT1);
                CHECKINTR(TC_3000_500_DEV_OPT0);
#undef CHECKINTR

#ifdef DIAGNOSTIC
#define PRINTINTR(msg, bits)                                            \
        if (ir & bits)                                                  \
                printf(msg);
                PRINTINTR("Second error occurred\n", TC_3000_500_IR_ERR2);
                PRINTINTR("DMA buffer error\n", TC_3000_500_IR_DMABE);
                PRINTINTR("DMA cross 2K boundary\n", TC_3000_500_IR_DMA2K);
                PRINTINTR("TC reset in progress\n", TC_3000_500_IR_TCRESET);
                PRINTINTR("TC parity error\n", TC_3000_500_IR_TCPAR);
                PRINTINTR("DMA tag error\n", TC_3000_500_IR_DMATAG);
                PRINTINTR("Single-bit error\n", TC_3000_500_IR_DMASBE);
                PRINTINTR("Double-bit error\n", TC_3000_500_IR_DMADBE);
                PRINTINTR("TC I/O timeout\n", TC_3000_500_IR_TCTIMEOUT);
                PRINTINTR("DMA block too long\n", TC_3000_500_IR_DMABLOCK);
                PRINTINTR("Invalid I/O address\n", TC_3000_500_IR_IOADDR);
                PRINTINTR("DMA scatter/gather invalid\n", TC_3000_500_IR_DMASG);
                PRINTINTR("Scatter/gather parity error\n",
                    TC_3000_500_IR_SGPAR);
#undef PRINTINTR
#endif
        } while (ifound);
}

#if 0
/*
 * tc_3000_500_ioslot --
 *      Set the PBS bits for devices on the TC.
 */
void
tc_3000_500_ioslot(slot, flags, set)
        u_int32_t slot, flags;
        int set;
{
        volatile u_int32_t *iosp;
        u_int32_t ios;
        int s;
        
        iosp = (volatile u_int32_t *)TC_3000_500_IOSLOT;
        ios = *iosp;
        flags <<= (slot * 3);
        if (set)
                ios |= flags;
        else
                ios &= ~flags;
        s = splhigh();
        *iosp = ios;
        tc_mb();
        splx(s);
}
#endif

#endif /* DEC_3000_500 */

int
tc_intrnull(val)
        void *val;
{

        panic("tc_intrnull: uncaught TC intr for cookie %ld\n",
            (u_long)val);
}


static int
tc_probe(device_t dev)
{
	if((hwrpb->rpb_type != ST_DEC_3000_300) &&
	   (hwrpb->rpb_type != ST_DEC_3000_500))
		return ENXIO;
	tc0 = dev;
	if(hwrpb->rpb_type == ST_DEC_3000_300) {
		device_set_desc(dev, "12.5 Mhz Turbochannel Bus");
	} else {
		device_set_desc(dev, "25 Mhz Turbochannel Bus");

	}
	return 0;
}

static int
tc_attach(device_t dev)
{
	struct tc_softc* sc = TC_SOFTC(dev);
	tc_addr_t tcaddr;
	const struct tc_builtin *builtin;
	struct tc_attach_args *ta;
	int i;
	device_t child = NULL;

	tc0 = dev;

	switch(hwrpb->rpb_type){
#ifdef DEC_3000_300
	case ST_DEC_3000_300:
		sc->sc_speed = TC_SPEED_12_5_MHZ;
		sc->sc_nslots = tc_3000_300_nslots;
		sc->sc_slots  = tc_3000_300_slots;
		sc->nbuiltins = tc_3000_300_nbuiltins;
		sc->builtins = tc_3000_300_builtins;
		tc_3000_300_intr_setup();
		set_iointr(tc_3000_300_iointr);
		sc->sc_intr_establish = tc_3000_300_intr_establish;
		sc->sc_intr_disestablish = tc_3000_300_intr_disestablish;
		break;
#endif /* DEC_3000_500 */
#ifdef DEC_3000_500
	case ST_DEC_3000_500:
		sc->sc_speed = TC_SPEED_25_MHZ;
		sc->sc_nslots = tc_3000_500_nslots;
		sc->sc_slots  = tc_3000_500_slots;
		sc->nbuiltins = tc_3000_500_nbuiltins;
		sc->builtins = tc_3000_500_builtins;
		tc_3000_500_intr_setup();
		set_iointr(tc_3000_500_iointr);
		sc->sc_intr_establish = tc_3000_500_intr_establish;
		sc->sc_intr_disestablish = tc_3000_500_intr_disestablish;
		break;
#endif /* DEC_3000_500 */

        default:
                panic("tcattach: bad cpu type");
        }
	/*
         * Try to configure each built-in device
         */

	for (i = 0; i < sc->nbuiltins; i++) {
		builtin = &sc->builtins[i];
		tcaddr = sc->sc_slots[builtin->tcb_slot].tcs_addr +
			builtin->tcb_offset;
		if (tc_badaddr(tcaddr))
                        continue;
		ta = malloc(sizeof(struct tc_attach_args), M_DEVBUF, M_NOWAIT);
		if (!ta)
			continue;
		ta->ta_slot = builtin->tcb_slot;
                ta->ta_offset = builtin->tcb_offset;
                ta->ta_addr = tcaddr;
                ta->ta_cookie = builtin->tcb_cookie;
                ta->ta_busspeed = sc->sc_speed;

		child = device_add_child(dev, builtin->tcb_modname, 0);
		device_set_ivars(child, ta);
		device_probe_and_attach(child);
	}

	return 0;
}


int
tc_checkslot(slotbase, namep)
        tc_addr_t slotbase;
        char *namep;
{
        struct tc_rommap *romp;
        int i, j;

        for (i = 0; i < NTC_ROMOFFS; i++) {
                romp = (struct tc_rommap *)
                    (slotbase + tc_slot_romoffs[i]);

                switch (romp->tcr_width.v) {
                case 1:
                case 2:
                case 4:
                        break;

                default:
                        continue;
                }

                if (romp->tcr_stride.v != 4)
                        continue;

                for (j = 0; j < 4; j++)
                        if (romp->tcr_test[j+0*romp->tcr_stride.v] != 0x55 ||
                            romp->tcr_test[j+1*romp->tcr_stride.v] != 0x00 ||
                            romp->tcr_test[j+2*romp->tcr_stride.v] != 0xaa ||
                            romp->tcr_test[j+3*romp->tcr_stride.v] != 0xff)
                                continue;

                for (j = 0; j < TC_ROM_LLEN; j++)
                        namep[j] = romp->tcr_modname[j].v;
                namep[j] = '\0';
                return (1);
        }
        return (0);
}

void
tc_intr_establish(dev, cookie, level, handler, arg)
        struct device *dev;
        void *cookie, *arg;
        tc_intrlevel_t level;
        int (*handler) __P((void *));
{
        struct tc_softc *sc = (struct tc_softc *)device_get_softc(dev);

        (*sc->sc_intr_establish)(device_get_parent(dev), cookie, level,
            handler, arg);
}

void
tc_intr_disestablish(dev, cookie)
        struct device *dev;
        void *cookie;
{
        struct tc_softc *sc = (struct tc_softc *)device_get_softc(dev);

        (*sc->sc_intr_disestablish)(device_get_parent(dev), cookie);
}

DRIVER_MODULE(tc, tcasic, tc_driver, tc_devclass, 0, 0);

