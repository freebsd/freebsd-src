/*
 * Copyright (c) 1998 and 1999 HAYAKAWA Koichi.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by HAYAKAWA Koichi.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/* $Id: pccbb.c,v 1.12 1999/09/07 17:38:17 gehenna Exp $ */
/* FreeBSD/newconfig version UCHIYAMA Yasushi 1999 */
/* $FreeBSD: src/sys/pci/pccbb.c,v 1.1 1999/11/18 07:14:53 imp Exp $ */

#define CBB_DEBUG
#undef SHOW_REGS
#undef  PCCBB_PCMCIA_POLL

#define CB_PCMCIA_POLL
#define CB_PCMCIA_POLL_ONLY
#define LEVEL2
#undef CB_PCMCIA_POLL
#undef CB_PCMCIA_POLL_ONLY
#undef LEVEL2

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/sockio.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/kthread.h>
#include <vm/vm.h>

#include <machine/intr.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#define delay(arg) DELAY(arg)
#include <machine/clock.h>
#include <dev/pci/pcidevs.h>

#include <dev/cardbus/cardbusreg.h>
#include <dev/cardbus/cardbusvar.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>

#include <dev/ic/i82365reg.h>
#include <dev/ic/i82365reg.h>

#include <dev/pci/pccbbreg.h>
#include <dev/pci/pccbbvar.h>

#define	PCIC_FLAG_SOCKETP	0x0001
#define	PCIC_FLAG_CARDP		0x0002

/* Chipset ID */
#define CB_UNKNOWN  0		/* NOT Cardbus-PCI bridge */
#define CB_TI113X   1		/* TI PCI1130/1131 */
#define CB_TI12XX   2		/* TI PCI1250/1220 */
#define CB_RF5C47X  3		/* RICOH RF5C475/476/477 */
#define CB_RF5C46X  4		/* RICOH RF5C465/466/467 */
#define CB_TOPIC95  5		/* Toshiba ToPIC95 */
#define CB_TOPIC95B 6		/* Toshiba ToPIC95B */
#define CB_TOPIC97  7		/* Toshiba ToPIC97 */
#define CB_CHIPS_LAST  8	/* Sentinel */

#if defined CBB_DEBUG
#define DPRINTF(x) printf x
#define STATIC
#else
#define DPRINTF(x)
#define STATIC static
#endif

void
pccbb_event_thread (void *arg)
{
    struct pccbb_softc *sc = arg;
    bus_space_tag_t memt = sc->sc_base_memt;
    bus_space_handle_t memh = sc->sc_base_memh;
    u_int32_t sockstate;

    int s;

    while (1) {
	s = splhigh();
	if (!sc->sc_queued) {
	    splx (s);
	    tsleep (&sc->events, PWAIT, "pccbbev", 0);
	} else {
	    sockstate = bus_space_read_4(memt, memh, CB_SOCKET_STAT);
	    if (sc->sc_flags & CBB_CARDEXIST) {
		DPRINTF(("%s: card removed\n", sc->sc_dev.dv_xname));
		sc->sc_flags &= ~CBB_CARDEXIST;
		if (sockstate & CB_SOCKET_STAT_16BIT) {
		    struct cbb_pcic_handle *ph = &sc->sc_pcmcia_h;
		    struct pcmcia_softc *psc = (void*)ph->pcmcia;
		    if (!(ph->flags & PCIC_FLAG_CARDP)) {
			panic("pccbbintr: already detached");
		    }
		    psc->sc_if.if_card_deactivate(ph->pcmcia);
		    pccbb_pcmcia_socket_disable(ph);
		    pccbb_pcmcia_detach_card(ph, DETACH_FORCE);
		} else {
		    if (sc->sc_cbdev)
			config_detach (sc->sc_cbdev, DETACH_FORCE);
		    else 
			printf ("no corresponding device instance.\n"); /* XXX should panic */
		}
	    } else {
		DPRINTF(("%s: card inserted\n", sc->sc_dev.dv_xname));
		pccbb_insert (sc);
	    }
	    sc->sc_queued = 0;
	    sc->sc_flags &= ~CBB_CARDSTATUS_BUSY;
	    splx (s);
	}
    }
    kthread_exit(0);

}
void
pccbb_create_event_thread (void *arg)
{
    struct pccbb_softc *sc = arg;
    if (kthread_create1(pccbb_event_thread, sc, &sc->event_thread,
			"%s,%s", sc->sc_dev.dv_xname, "cardbus")) {
	printf ("%s: unable to create event thread.\n",
		sc->sc_dev.dv_xname);
	panic ("pccbb_create_event_thread");
    } else
	printf("%s: create event thread\n", sc->sc_dev.dv_xname);
}

void
pccbb_kthread_init (struct pccbb_softc *sc)
{
    kthread_create(pccbb_create_event_thread, sc);
}


int pccbbmatch __P((struct device *, struct cfdata *, void *));
void pccbbattach __P((struct device *, struct device *, void *));
int pccbbintr __P((void *));

static void pccbb_insert __P((void *));
static int pccbb_detect_card __P((struct pccbb_softc *));
static void pccbb_pcmcia_write __P((struct cbb_pcic_handle *, int, u_int8_t));
static u_int8_t pccbb_pcmcia_read __P((struct cbb_pcic_handle *, int));
#define Pcic_read(ph, reg) ((ph)->ph_read((ph), (reg)))
#define Pcic_write(ph, reg, val) ((ph)->ph_write((ph), (reg), (val)))

STATIC int cb_reset __P((struct pccbb_softc *));
STATIC int cb_detect_voltage __P((struct pccbb_softc *));
STATIC int cbbprint __P((void *, const char *));

static int cb_chipset __P((u_int32_t, char const **, int *));
static void pccbb_chipinit __P((struct pci_attach_args *, struct pccbb_softc *));
STATIC void pccbb_pcmcia_attach __P((struct pccbb_softc *));
STATIC void pccbb_pcmcia_attach_card __P((struct cbb_pcic_handle *));
STATIC void pccbb_pcmcia_detach_card __P((struct cbb_pcic_handle *, int));
STATIC void pccbb_pcmcia_deactivate_card __P((struct cbb_pcic_handle *));

STATIC int pccbb_ctrl __P((cardbus_chipset_tag_t, int));
STATIC int pccbb_power __P((cardbus_chipset_tag_t, int));
STATIC int pccbb_cardenable __P((struct pccbb_softc *sc, int function));
static int pccbb_io_open __P((cardbus_chipset_tag_t, int, u_int32_t, u_int32_t));
static int pccbb_io_close __P((cardbus_chipset_tag_t, int));
static int pccbb_mem_open __P((cardbus_chipset_tag_t, int, u_int32_t, u_int32_t));
static int pccbb_mem_close __P((cardbus_chipset_tag_t, int));
static void *pccbb_intr_establish __P((cardbus_chipset_tag_t, int irq, int level, int (* ih)(void *), void *sc));
static void pccbb_intr_disestablish __P((cardbus_chipset_tag_t ct, void *ih));
static cardbustag_t pccbb_make_tag __P((cardbus_chipset_tag_t, int, int, int));
static void pccbb_free_tag __P((cardbus_chipset_tag_t, cardbustag_t));
static cardbusreg_t pccbb_conf_read __P((cardbus_chipset_tag_t, cardbustag_t, int));
static void pccbb_conf_write __P((cardbus_chipset_tag_t, cardbustag_t, int, cardbusreg_t));

STATIC int pccbb_pcmcia_mem_alloc __P((pcmcia_chipset_handle_t, bus_size_t,
				       struct pcmcia_mem_handle *));
STATIC void pccbb_pcmcia_mem_free __P((pcmcia_chipset_handle_t,
				       struct pcmcia_mem_handle *));
STATIC int pccbb_pcmcia_mem_map __P((pcmcia_chipset_handle_t, int, bus_addr_t,
				     bus_size_t, struct pcmcia_mem_handle *, bus_addr_t *, int *));
STATIC void pccbb_pcmcia_mem_unmap __P((pcmcia_chipset_handle_t, int));
STATIC int pccbb_pcmcia_io_alloc __P((pcmcia_chipset_handle_t, bus_addr_t,
				      bus_size_t, bus_size_t, struct pcmcia_io_handle *));
STATIC void pccbb_pcmcia_io_free __P((pcmcia_chipset_handle_t,
				      struct pcmcia_io_handle *));
STATIC int pccbb_pcmcia_io_map __P((pcmcia_chipset_handle_t, int, bus_addr_t,
				    bus_size_t, struct pcmcia_io_handle *, int *));
STATIC void pccbb_pcmcia_io_unmap __P((pcmcia_chipset_handle_t, int));
STATIC void *pccbb_pcmcia_intr_establish __P((pcmcia_chipset_handle_t,
					      struct pcmcia_function *, int, int (*) (void *), void *));
STATIC void pccbb_pcmcia_intr_disestablish __P((pcmcia_chipset_handle_t, void *));
STATIC void pccbb_pcmcia_socket_enable __P((pcmcia_chipset_handle_t));
STATIC void pccbb_pcmcia_socket_disable __P((pcmcia_chipset_handle_t));

static void pccbb_pcmcia_do_io_map __P((struct cbb_pcic_handle *, int));
static void pccbb_pcmcia_wait_ready __P((struct cbb_pcic_handle *));
static void pccbb_pcmcia_do_mem_map __P((struct cbb_pcic_handle *, int));
static int pccbb_pcmcia_print __P((void *, const char *));
static int pccbb_pcmcia_submatch __P((struct device *, struct cfdata *, void *));

static int pccbb_cardbus_submatch __P((struct device *, struct cfdata *, void *));

#if defined SHOW_REGS
static void cb_show_regs __P((pci_chipset_tag_t pc, pcitag_t tag, bus_space_tag_t memt, bus_space_handle_t memh));
#endif

struct cfattach cbb_pci_ca = {
    sizeof(struct pccbb_softc), pccbbmatch, pccbbattach
};

static struct pcmcia_chip_functions pccbb_pcmcia_funcs = {
    pccbb_pcmcia_mem_alloc,
    pccbb_pcmcia_mem_free,
    pccbb_pcmcia_mem_map,
    pccbb_pcmcia_mem_unmap,
    pccbb_pcmcia_io_alloc,
    pccbb_pcmcia_io_free,
    pccbb_pcmcia_io_map,
    pccbb_pcmcia_io_unmap,
    pccbb_pcmcia_intr_establish,
    pccbb_pcmcia_intr_disestablish,
    pccbb_pcmcia_socket_enable,
    pccbb_pcmcia_socket_disable,
};

static struct cardbus_functions pccbb_funcs = {
    pccbb_ctrl,
    pccbb_power,
    pccbb_mem_open,
    pccbb_mem_close,
    pccbb_io_open,
    pccbb_io_close,
    pccbb_intr_establish,
    pccbb_intr_disestablish,
    pccbb_make_tag,
    pccbb_free_tag,
    pccbb_conf_read,
    pccbb_conf_write,
};


int
pccbbmatch(parent, match, aux)
    struct device *parent;
    struct cfdata *match;
    void *aux;
{
    struct pci_attach_args *pa = (struct pci_attach_args *)aux;

    if ((pa->pa_class & PCI_CLASS_INTERFACE_MASK) == PCI_CLASS_INTERFACE_YENTA) {
	/* OK, It must be YENTA PCI-CardBus bridge */
	return 2; /* beat chipset_match */
    }
  
    return 0;
}


#define MAKEID(vendor, prod) (((vendor) << PCI_VENDOR_SHIFT) \
                              | ((prod) << PCI_PRODUCT_SHIFT))

struct yenta_chipinfo {
    pcireg_t yc_id;		/* vendor tag | product tag */
    const char *yc_name;
    int yc_chiptype;
    int yc_flags;
} yc_chipsets[] = {
    /* Texas Instruments chips */
    {MAKEID(PCI_VENDOR_TI, PCI_PRODUCT_TI_PCI1130), "TI1130", CB_TI113X,
     PCCBB_PCMCIA_IO_RELOC | PCCBB_PCMCIA_MEM_32},
    {MAKEID(PCI_VENDOR_TI, PCI_PRODUCT_TI_PCI1131), "TI1131", CB_TI113X,
     PCCBB_PCMCIA_IO_RELOC | PCCBB_PCMCIA_MEM_32},

    {MAKEID(PCI_VENDOR_TI, PCI_PRODUCT_TI_PCI1250), "TI1250", CB_TI12XX,
     PCCBB_PCMCIA_IO_RELOC | PCCBB_PCMCIA_MEM_32},
    {MAKEID(PCI_VENDOR_TI, PCI_PRODUCT_TI_PCI1220), "TI1220", CB_TI12XX,
     PCCBB_PCMCIA_IO_RELOC | PCCBB_PCMCIA_MEM_32},
    {MAKEID(PCI_VENDOR_TI, PCI_PRODUCT_TI_PCI1221), "TI1221", CB_TI12XX,
     PCCBB_PCMCIA_IO_RELOC | PCCBB_PCMCIA_MEM_32},
    {MAKEID(PCI_VENDOR_TI, PCI_PRODUCT_TI_PCI1225), "TI1225", CB_TI12XX,
     PCCBB_PCMCIA_IO_RELOC | PCCBB_PCMCIA_MEM_32},
    {MAKEID(PCI_VENDOR_TI, PCI_PRODUCT_TI_PCI2030), "TI2030", CB_UNKNOWN,
     PCCBB_PCMCIA_IO_RELOC | PCCBB_PCMCIA_MEM_32},

    /* Ricoh chips */
    {MAKEID(PCI_VENDOR_RICOH, PCI_PRODUCT_RICOH_RF5C475), "RF5C475",
     CB_RF5C47X, PCCBB_PCMCIA_MEM_32},
    {MAKEID(PCI_VENDOR_RICOH, PCI_PRODUCT_RICOH_RF5C476), "RF5C476",
     CB_RF5C47X, PCCBB_PCMCIA_MEM_32},
    {MAKEID(PCI_VENDOR_RICOH, PCI_PRODUCT_RICOH_RF5C477), "RF5C477",
     CB_RF5C47X, PCCBB_PCMCIA_MEM_32},
    {MAKEID(PCI_VENDOR_RICOH, PCI_PRODUCT_RICOH_RF5C477), "RF5C478",
     CB_RF5C47X, PCCBB_PCMCIA_MEM_32},

    {MAKEID(PCI_VENDOR_RICOH, PCI_PRODUCT_RICOH_RF5C465), "RF5C465",
     CB_RF5C46X, PCCBB_PCMCIA_MEM_32},
    {MAKEID(PCI_VENDOR_RICOH, PCI_PRODUCT_RICOH_RF5C466), "RF5C466",
     CB_RF5C46X, PCCBB_PCMCIA_MEM_32},

    /* Toshiba products */
    {MAKEID(PCI_VENDOR_TOSHIBA2, PCI_PRODUCT_TOSHIBA2_ToPIC95), "ToPIC95",
     CB_TOPIC95, PCCBB_PCMCIA_MEM_32},
    {MAKEID(PCI_VENDOR_TOSHIBA2, PCI_PRODUCT_TOSHIBA2_ToPIC95B), "ToPIC95B",
     CB_TOPIC95B, PCCBB_PCMCIA_MEM_32},
    {MAKEID(PCI_VENDOR_TOSHIBA2, PCI_PRODUCT_TOSHIBA2_ToPIC95B), "ToPIC97",
     CB_TOPIC97, PCCBB_PCMCIA_MEM_32},

    /* sentinel */
    {0 /* null id */, "unknown",
     CB_UNKNOWN, 0},
};


static int
cb_chipset(pci_id, namep, flagp)
    u_int32_t pci_id;
    char const **namep;
    int *flagp;
{
    int loopend = sizeof(yc_chipsets)/sizeof(yc_chipsets[0]);
    struct yenta_chipinfo *ycp, *ycend;

    ycend = yc_chipsets + loopend;

    for (ycp =yc_chipsets; ycp < ycend && pci_id != ycp->yc_id; ++ycp);

    if (ycp == ycend) {
	/* not found */
	ycp = yc_chipsets + loopend - 1; /* to point the sentinel */
    }

    if (namep != NULL) {
	*namep = ycp->yc_name;
    }

    if (flagp != NULL) {
	*flagp = ycp->yc_flags;
    }

    return ycp->yc_chiptype;
}

void
pccbbattach(parent, self, aux)
    struct device *parent;
    struct device *self;
    void *aux;
{
    struct pccbb_softc *sc = (void *)self;
    struct pci_attach_args *pa = aux;
    pci_chipset_tag_t pc = pa->pa_pc;
    pcireg_t sock_base, cbctrl;
    bus_addr_t sockbase;
    bus_space_tag_t base_memt;
    bus_space_handle_t base_memh;
    u_int32_t maskreg;
    pci_intr_handle_t ih;
    const char *intrstr = NULL;
    char const *name;
    int flags;

    sc->sc_chipset = cb_chipset(pa->pa_id, &name, &flags);
    printf(" (%s), flags %d\n", name, flags);

    /* pccbb_machdep.c start */
#if 0
    pcbb_attach_machdef(pa, sc);
#endif

    /* MAP socket registers and ExCA registers on memory-space
       When no valid address is set on socket base registers (on pci
       config space), get it not polite way */
    sock_base = pci_conf_read(pc, pa->pa_tag, PCI_SOCKBASE);

    if (PCI_MAPREG_MEM_ADDR(sock_base) <= 0x100000 ||
	PCI_MAPREG_MEM_ADDR(sock_base) == 0xfffffff0) {
	/* The address may be invalid. */
	sc->sc_base_memt = pa->pa_memt;
#if !defined CBB_PCI_BASE
#define CBB_PCI_BASE 0x20000000
#endif
	if (bus_space_alloc(sc->sc_base_memt, CBB_PCI_BASE, 0xffffffff,
			    0x1000, /* size */
			    (sc->sc_chipset == CB_RF5C47X || sc->sc_chipset == CB_TI113X) ? 0x10000 : 0x1000, /* alignment */
			    0,      /* boundary */
			    0,      /* flags */
			    &sockbase, &sc->sc_base_memh)) {
	    /* cannot allocate memory space */
	    return;
	}
	pci_conf_write(pc, pa->pa_tag, PCI_SOCKBASE, sockbase);
	DPRINTF(("%s: CardBus resister address 0x%x -> 0x%lx\n",sc->sc_dev.dv_xname,
		 sock_base, pci_conf_read(pc, pa->pa_tag, PCI_SOCKBASE)));
    } else {
	/* The address must be valid. */
	if (pci_mapreg_map(pa, PCI_SOCKBASE, PCI_MAPREG_TYPE_MEM, 0,
			   &sc->sc_base_memt, &sc->sc_base_memh, &sockbase,
			   NULL)) {
	    printf("%s: can't map socket base address 0x%x\n", sc->sc_dev.dv_xname,
		   sock_base);
	    /* I think it's funny: socket base registers must be mapped on
	       memory space, but ... */
	    if (pci_mapreg_map(pa, PCI_SOCKBASE, PCI_MAPREG_TYPE_IO, 0,
			       &sc->sc_base_memt, &sc->sc_base_memh,
			       &sockbase, NULL)) {
		printf("%s: can't map socket base address 0x%lx: io mode\n",
		       sc->sc_dev.dv_xname, (u_long)sockbase);
		return;
	    }
	} else {
	    DPRINTF(("%s: socket base address 0x%lx",sc->sc_dev.dv_xname, (u_long)sockbase));
	}
    }

    sc->sc_mem_start = CBB_PCI_BASE; /* XXX */
    sc->sc_mem_end = 0xffffffff;	/* XXX */
  
    /* pccbb_machdep.c end */

#if defined CBB_DEBUG
    {
	static char *intrname[5] = {"NON", "A", "B", "C", "D"};
	printf(" intrpin %s, intrtag %d\n", intrname[pa->pa_intrpin],
	       pa->pa_intrline);
    }
#endif

    /****** setup softc ******/
    sc->sc_pc = pc;
    sc->sc_iot = pa->pa_iot;
    sc->sc_memt = pa->pa_memt;
    sc->sc_tag = pa->pa_tag;
    sc->sc_function = pa->pa_function;

    sc->sc_intrline = pa->pa_intrline;
    sc->sc_intrtag = pa->pa_intrtag;
    sc->sc_intrpin = pa->pa_intrpin;

    pccbb_chipinit(pa, sc);

    base_memt = sc->sc_base_memt;	/* socket regs memory tag */
    base_memh = sc->sc_base_memh;	/* socket regs memory handle */

    /* CSC Interrupt: Card detect interrupt on */
    maskreg = bus_space_read_4(base_memt, base_memh, CB_SOCKET_MASK);
    maskreg |= CB_SOCKET_MASK_CD;	/* Card detect intr is turned on. */
    bus_space_write_4(base_memt, base_memh, CB_SOCKET_MASK, maskreg);
    /* reset interrupt */
    bus_space_write_4(base_memt, base_memh, CB_SOCKET_EVENT,
		      bus_space_read_4(base_memt, base_memh, CB_SOCKET_EVENT));


    /* Map and establish the interrupt. */
    if (pci_intr_map(pc, pa->pa_intrtag, pa->pa_intrpin,
		     pa->pa_intrline, &ih)) {
	printf("%s: couldn't map interrupt\n", sc->sc_dev.dv_xname);
	return;
    }
    intrstr = pci_intr_string(pc, ih);
  
    sc->sc_ih = pci_intr_establish(pc, ih, IPL_BIO, (void(*)(void*))pccbbintr, sc);
    /*
     * queue creation of a kernel thread to handle insert/removal events.
     */
    pccbb_kthread_init (sc);

    if (sc->sc_ih == NULL) {
	printf("%s: couldn't establish interrupt", sc->sc_dev.dv_xname);
	if (intrstr != NULL) {
	    printf(" at %s", intrstr);
	}
	printf("\n");
	return;
    }

    printf("%s: interrupting at %s\n", sc->sc_dev.dv_xname, intrstr);

    /* Check card exists or not. if exists, cb_reset will reset card */
    {
	u_int32_t sockstat = bus_space_read_4(base_memt,base_memh, CB_SOCKET_STAT);
	if (0 == (sockstat & CB_SOCKET_STAT_CD)) { /* card exist */
	    sc->sc_flags |= CBB_CARDEXIST;
	}
    }
    /****** attach cardbus ******/
    {
	struct cbslot_attach_args cba;
	struct cardbus_softc *csc;	/* child softc */
	u_int32_t busreg = pci_conf_read(pc, pa->pa_tag, PCI_BUSNUM);

	/* initialise cbslot_attach */
	cba.cba_busname = "cardbus";
	cba.cba_iot = pa->pa_iot;
	cba.cba_memt = pa->pa_memt;
	cba.cba_dmat = pa->pa_dmat;
#if 1 /* XXX */
	cba.cba_function = pa->pa_function;
#else
	cba.cba_function = 0;
#endif
	cba.cba_bus = (busreg >> 8) & 0x0ff;
	cba.cba_cc = (void *)sc;
	cba.cba_cf = &pccbb_funcs;
	cba.cba_intrline = pci_intr_line (pa, ih);
#if defined SHOW_REGS
	cb_show_regs(sc->sc_pc, sc->sc_tag, sc->sc_base_memt, sc->sc_base_memh);
#endif
	if (NULL != (csc = (void *)config_found(self, &cba, cbbprint))) {
	    DPRINTF(("pccbbattach: found cardbus\n"));
	    sc->sc_csc = csc;
	}
    }

    /****** attach pccard bus ******/
    pccbb_pcmcia_attach(sc);

    return;
}

static void
pccbb_chipinit(pa, sc)
    struct pci_attach_args *pa;
    struct pccbb_softc *sc;
{
    pci_chipset_tag_t pc = pa->pa_pc;
    bus_space_tag_t base_memt = sc->sc_base_memt;	/* socket regs memory tag */
    bus_space_handle_t base_memh = sc->sc_base_memh; /* socket regs memory handle */
    pcireg_t cbctrl;

    /*
      Set CardBus latency timer
    */
    {
	pcireg_t pci_lscp = pci_conf_read(pc, pa->pa_tag, PCI_CB_LSCP_REG);
	if (PCI_CB_LATENCY(pci_lscp) < 0x20) {
	    pci_lscp &= ~(PCI_CB_LATENCY_MASK << PCI_CB_LATENCY_SHIFT);
	    pci_lscp |= (0x20 << PCI_CB_LATENCY_SHIFT);
	    pci_conf_write(pc, pa->pa_tag, PCI_CB_LSCP_REG, pci_lscp);
	}
	printf("CardBus latency time 0x%x\n", PCI_CB_LATENCY(pci_lscp));
    }

    /*
      Set PCI latency timer
    */
    {
	pcireg_t pci_bhlc = pci_conf_read(pc, pa->pa_tag, PCI_BHLC_REG);
	if (PCI_LATTIMER(pci_bhlc) < 0x20) {
	    pci_bhlc &= ~(PCI_LATTIMER_MASK << PCI_LATTIMER_SHIFT);
	    pci_bhlc |= (0x20 << PCI_LATTIMER_SHIFT);
	    pci_conf_write(pc, pa->pa_tag, PCI_BHLC_REG, pci_bhlc);
	}
	printf("PCI latency time 0x%x\n", PCI_LATTIMER(pci_bhlc));
    }

    /* disable Legacy IO */

    switch (sc->sc_chipset) {
    case CB_RF5C46X:
    {
	pcireg_t bcri = pci_conf_read(pc, pa->pa_tag, PCI_BCR_INTR);
	bcri &= ~(CB_BCRI_RL_3E0_ENA | CB_BCRI_RL_3E2_ENA);
	pci_conf_write(pc, pa->pa_tag, PCI_BCR_INTR, bcri);
    }
    break;
    default:
	/* XXX: I don't know how to kill Legacy IO properly. */
	pci_conf_write(pc, pa->pa_tag, PCI_LEGACY, 0x0);
	break;
    }

    /****** Interrupt routing ******/

    /* use PCI interrupt */
    {
	u_int32_t bcr = pci_conf_read(pc, pa->pa_tag, PCI_BCR_INTR);
	bcr &= ~CB_BCR_INTR_IREQ_ENABLE; /* use PCI Intr */
	bcr |= CB_BCR_WRITE_POST_ENABLE; /* enable write post */
	pci_conf_write(pc, pa->pa_tag, PCI_BCR_INTR, bcr);
    }

    if (CB_TI113X == sc->sc_chipset) {
	cbctrl = pci_conf_read(pc, pa->pa_tag, PCI_CBCTRL);
	if (0 == pa->pa_function) {
	    cbctrl |= PCI113X_CBCTRL_PCI_IRQ_ENA;
	}
	cbctrl |= PCI113X_CBCTRL_PCI_IRQ_ENA;	/* XXX: bug in PCI113X */
	cbctrl |= PCI113X_CBCTRL_PCI_CSC; /* CSC intr enable */
	cbctrl &= ~PCI113X_CBCTRL_PCI_INTR; /* functional intr prohibit */
	cbctrl &= ~PCI113X_CBCTRL_INT_MASK;	/* prohibit ISA routing */
	pci_conf_write(pc, pa->pa_tag, PCI_CBCTRL, cbctrl);

	/* set ExCA regs: PCI113X required to be set bit 4 at Interrupt
	   and General Register, which is IRQ Enable Register, and clear
	   bit 3:0 to zero in order to route CSC interrupt to PCI
	   interrupt pin. */
	bus_space_write_1(base_memt, base_memh, 0x0803, 0x10);
	/* set ExCA regs: prohibit all pcmcia-style CSC intr. */
	bus_space_write_1(base_memt, base_memh, 0x0805, 0x00);
#if 1
	DPRINTF(("ExCA regs:"));
	DPRINTF((" 0x803: %02x", bus_space_read_1(base_memt, base_memh, 0x803)));
	DPRINTF((" 0x805: %02x", bus_space_read_1(base_memt, base_memh, 0x805)));
	DPRINTF((" 0x81e: %02x\n", bus_space_read_1(base_memt,base_memh,0x81e)));
#endif
    } else if (sc->sc_chipset == CB_TI12XX) {
	cbctrl = pci_conf_read(pc, pa->pa_tag, PCI_CBCTRL);
	cbctrl &= ~PCI12XX_CBCTRL_INT_MASK;	/* intr routing reset */
	cbctrl |= PCI12XX_CBCTRL_INT_PCI;	/* PCI intr */
	pci_conf_write(pc, pa->pa_tag, PCI_CBCTRL, cbctrl);
	bus_space_write_1(base_memt, base_memh, 0x0803, 0x10);
	bus_space_write_1(base_memt, base_memh, 0x0805, 0x00);
    } else if (sc->sc_chipset == CB_TOPIC95B) {
	cardbusreg_t sock_ctrl, slot_ctrl;

	sock_ctrl = pci_conf_read(pc, pa->pa_tag, TOPIC_SOCKET_CTRL);
	pci_conf_write(pc, pa->pa_tag, TOPIC_SOCKET_CTRL,
		       sock_ctrl | TOPIC_SOCKET_CTRL_SCR_IRQSEL);

	slot_ctrl = pci_conf_read(pc, pa->pa_tag, TOPIC_SLOT_CTRL);
	DPRINTF(("%s: topic slot ctrl reg 0x%x -> ", sc->sc_dev.dv_xname,
		 slot_ctrl));
/*    slot_ctrl &= ~TOPIC_SLOT_CTRL_CLOCK_MASK;*/
	slot_ctrl |= (TOPIC_SLOT_CTRL_SLOTON | TOPIC_SLOT_CTRL_SLOTEN |
		      TOPIC_SLOT_CTRL_ID_LOCK);
	slot_ctrl |= TOPIC_SLOT_CTRL_CARDBUS;
	slot_ctrl &= ~TOPIC_SLOT_CTRL_SWDETECT;
	pci_conf_write(pc, pa->pa_tag, TOPIC_SLOT_CTRL, slot_ctrl);
	DPRINTF(("0x%x\n", slot_ctrl));
    }

    /* close all memory and io windows */
    pci_conf_write(pc, pa->pa_tag, PCI_CB_MEMBASE0, 0xffffffff);
    pci_conf_write(pc, pa->pa_tag, PCI_CB_MEMLIMIT0, 0);
    pci_conf_write(pc, pa->pa_tag, PCI_CB_MEMBASE1, 0xffffffff);
    pci_conf_write(pc, pa->pa_tag, PCI_CB_MEMLIMIT1, 0);
    pci_conf_write(pc, pa->pa_tag, PCI_CB_IOBASE0, 0xffffffff);
    pci_conf_write(pc, pa->pa_tag, PCI_CB_IOLIMIT0, 0);
    pci_conf_write(pc, pa->pa_tag, PCI_CB_IOBASE1, 0xffffffff);
    pci_conf_write(pc, pa->pa_tag, PCI_CB_IOLIMIT1, 0);

}


/****** attach pccard bus ******/
STATIC void
pccbb_pcmcia_attach(sc)
    struct pccbb_softc *sc;
{
    struct cbb_pcic_handle *ph = &sc->sc_pcmcia_h;
    struct pcmciabus_attach_args paa;

    sc->sc_pcmcia_flags |= (PCCBB_PCMCIA_IO_RELOC | PCCBB_PCMCIA_MEM_32);

    /* initialise pcmcia part in pccbb_softc */
    ph->sc = sc;
    ph->sock = sc->sc_function;
    ph->flags = 0;
    ph->shutdown = 0;
    ph->ih_irq = sc->sc_intrline;
    ph->ph_iot = sc->sc_base_memt;
    ph->ph_ioh = sc->sc_base_memh;
    ph->ph_read = pccbb_pcmcia_read;
    ph->ph_write = pccbb_pcmcia_write;
    sc->sc_pct = &pccbb_pcmcia_funcs;

    Pcic_write(ph, PCIC_CSC_INTR, 0);
    Pcic_read(ph, PCIC_CSC);

    /* initialise pcmcia bus attachment */
    paa.paa_busname = "pcmcia";
    paa.pct = sc->sc_pct;
    paa.pch = ph;
    paa.iobase = 0;		/* I don't use them */
    paa.iosize = 0;

    ph->pcmcia = config_found_sm(&ph->sc->sc_dev, &paa, pccbb_pcmcia_print,
				 pccbb_pcmcia_submatch);
    if (ph->pcmcia != NULL) {
	if (1 == pccbb_detect_card(sc)) {
	    printf("%s: a 16-bit pcmcia card found.\n", sc->sc_dev.dv_xname); /*XXX*/
	    pccbb_pcmcia_attach_card(ph);
	}
    }
    return;
}



STATIC void
pccbb_pcmcia_attach_card(ph)
    struct cbb_pcic_handle *ph;
{
    struct pcmcia_softc *psc = (void*)ph->pcmcia;
    if (ph->flags & PCIC_FLAG_CARDP) {
	panic("pccbb_pcmcia_attach_card: already attached");
    }

    /* call the MI attach function */
    psc->sc_if.if_card_attach(ph->pcmcia);
    ph->flags |= PCIC_FLAG_CARDP;
}


STATIC void
pccbb_pcmcia_detach_card(ph, flags)
    struct cbb_pcic_handle *ph;
    int flags;
{
    struct pcmcia_softc *psc = (void*)ph->pcmcia;
    if (!(ph->flags & PCIC_FLAG_CARDP)) {
	panic("pccbb_pcmcia_detach_card: already detached");
    }

    ph->flags &= ~PCIC_FLAG_CARDP;

    /* call the MI detach function */
    psc->sc_if.if_card_detach (ph->pcmcia, flags);
}

/**********************************************************************
* int pccbbintr(arg)
*    void *arg;
*   This routine handles the interrupt from Yenta PCI-CardBus bridge
*   itself.
**********************************************************************/
int
pccbbintr(arg)
    void *arg;
{
    struct pccbb_softc *sc = arg;
    bus_space_tag_t memt = sc->sc_base_memt;
    bus_space_handle_t memh = sc->sc_base_memh;
    u_int32_t sockevent, sockstate;
    int s;

    if (!(sockevent = bus_space_read_4 (memt, memh, CB_SOCKET_EVENT)))
	return 0; /* not for me */
    /* reset bit */
    bus_space_write_4 (memt, memh, CB_SOCKET_EVENT, sockevent); 
    
    if (sockevent & CB_SOCKET_EVENT_CD) {
	if (!(sc->sc_flags & CBB_CARDSTATUS_BUSY)) {
	    s  = splhigh (); /* lock softc */
	    sc->sc_queued = 1;
	    sc->sc_flags |= CBB_CARDSTATUS_BUSY;
	    splx (s); /* unlock softc */
	    wakeup (&sc->events);
	} else {
	    DPRINTF(("%s (pccbbintr): busy", sc->sc_dev.dv_xname));
	    sc->sc_flags &= ~CBB_CARDSTATUS_BUSY; /* XXX chatterling interrupts. Should change code like as i82365.c */
	}
    } else {
	sockstate = bus_space_read_4 (memt, memh, CB_SOCKET_STAT);
	DPRINTF(("%s (pccbbintr): 0x%08x", sc->sc_dev.dv_xname, sockevent));
	if (sockevent & CB_SOCKET_EVENT_CSTS) {
	    DPRINTF((" cstsevent occures, 0x%08x\n", sockstate));
	} else if (sockevent & CB_SOCKET_EVENT_POWER) {
	    DPRINTF((" pwrevent occures, 0x%08x\n", sockstate));
	} else {
	    DPRINTF((" unknown event, 0x%08x\n", sockstate));
	}
    }

    return 1;
}

static void
pccbb_insert(arg)
    void *arg;
{
    struct pccbb_softc *sc = (struct pccbb_softc *)arg;
    u_int32_t sockevent, sockstate;
    int timeout = 30;
    do {
	sockevent = bus_space_read_4(sc->sc_base_memt, sc->sc_base_memh,
				 CB_SOCKET_EVENT);
	sockstate = bus_space_read_4(sc->sc_base_memt, sc->sc_base_memh,
				 CB_SOCKET_STAT);
    } while (sockstate & CB_SOCKET_STAT_CD && --timeout > 0);
    if (timeout < 0) {
	printf ("%s: insert timeout", sc->sc_dev.dv_xname);
	return;
    }
    DPRINTF(("%s: 0x%08x", sc->sc_dev.dv_xname, sockevent));
    DPRINTF((" card inserted, 0x%08x\n", sockstate));
    sc->sc_flags |= CBB_CARDEXIST;
    /* call pccard intterupt handler here */
    if (sockstate & CB_SOCKET_STAT_16BIT) {
	/* 16-bit card */
	pccbb_pcmcia_attach_card(&sc->sc_pcmcia_h);
    } else if (sockstate & CB_SOCKET_STAT_CB) {
	/* 32-bit card */
	sc->sc_cbdev = sc->sc_csc->sc_if.if_card_attach (sc->sc_csc);
    } else {
	printf ("unknown card type.\n");
    }
}

#define PCCBB_PCMCIA_OFFSET 0x800
static u_int8_t
pccbb_pcmcia_read(ph, reg)
    struct cbb_pcic_handle *ph;
    int reg;
{
    return bus_space_read_1(ph->ph_iot, ph->ph_ioh, PCCBB_PCMCIA_OFFSET + reg);
}

static void
pccbb_pcmcia_write(ph, reg, val)
    struct cbb_pcic_handle *ph;
    int reg;
    u_int8_t val;
{
    bus_space_write_1(ph->ph_iot, ph->ph_ioh, PCCBB_PCMCIA_OFFSET + reg, val);

    return;
}

/**********************************************************************
* STATIC int pccbb_ctrl(cardbus_chipset_tag_t, int)
**********************************************************************/
STATIC int
pccbb_ctrl(ct, command)
    cardbus_chipset_tag_t ct;
    int command;
{
    struct pccbb_softc *sc = (struct pccbb_softc *)ct;

    switch(command) {
    case CARDBUS_CD:
	if (2 == pccbb_detect_card(sc)) {
	    int retval = 0;
	    int status = cb_detect_voltage(sc);
	    if (PCCARD_VCC_5V & status) {
		retval |= CARDBUS_5V_CARD;
	    }
	    if (PCCARD_VCC_3V & status) {
		retval |= CARDBUS_3V_CARD;
	    }
	    if (PCCARD_VCC_XV & status) {
		retval |= CARDBUS_XV_CARD;
	    }
	    if (PCCARD_VCC_YV & status) {
		retval |= CARDBUS_YV_CARD;
	    }
	    return retval;
	} else {
	    return 0;
	}
	break;
    case CARDBUS_RESET:
	return cb_reset(sc);
	break;
    case CARDBUS_IO_ENABLE:	/* fallthrough */
    case CARDBUS_IO_DISABLE:	/* fallthrough */
    case CARDBUS_MEM_ENABLE:	/* fallthrough */
    case CARDBUS_MEM_DISABLE:	/* fallthrough */
    case CARDBUS_BM_ENABLE:	/* fallthrough */
    case CARDBUS_BM_DISABLE:	/* fallthrough */
	return pccbb_cardenable(sc, command);
	break;
    }

    return 0;
}



/**********************************************************************
* STATIC int pccbb_power(cardbus_chipset_tag_t, int)
*   This function returns true when it succeeds and returns false when
*   it fails.
**********************************************************************/
STATIC int
pccbb_power(ct, command)
    cardbus_chipset_tag_t ct;
    int command;
{
    struct pccbb_softc *sc = (struct pccbb_softc *)ct;

    u_int32_t status, sock_ctrl;
    bus_space_tag_t memt = sc->sc_base_memt;
    bus_space_handle_t memh = sc->sc_base_memh;

    DPRINTF(("pccbb_power: %s and %s [%x]\n",
	     (command & CARDBUS_VCCMASK) == CARDBUS_VCC_UC ? "CARDBUS_VCC_UC" :
	     (command & CARDBUS_VCCMASK) == CARDBUS_VCC_5V ? "CARDBUS_VCC_5V" : 
	     (command & CARDBUS_VCCMASK) == CARDBUS_VCC_3V ? "CARDBUS_VCC_3V" : 
	     (command & CARDBUS_VCCMASK) == CARDBUS_VCC_XV ? "CARDBUS_VCC_XV" : 
	     (command & CARDBUS_VCCMASK) == CARDBUS_VCC_YV ? "CARDBUS_VCC_YV" : 
	     (command & CARDBUS_VCCMASK) == CARDBUS_VCC_0V ? "CARDBUS_VCC_0V" :
	     "UNKNOWN",
	     (command & CARDBUS_VPPMASK) == CARDBUS_VPP_UC ? "CARDBUS_VPP_UC" :
	     (command & CARDBUS_VPPMASK) == CARDBUS_VPP_12V ? "CARDBUS_VPP_12V" :
	     (command & CARDBUS_VPPMASK) == CARDBUS_VPP_VCC ? "CARDBUS_VPP_VCC" :
	     (command & CARDBUS_VPPMASK) == CARDBUS_VPP_0V ? "CARDBUS_VPP_0V" :
	     "UNKNOWN",
	     command));
  
    status = bus_space_read_4(memt, memh, CB_SOCKET_STAT);
    sock_ctrl = bus_space_read_4(memt, memh, CB_SOCKET_CTRL);

    switch (command & CARDBUS_VCCMASK) {
    case CARDBUS_VCC_UC:
	break;
    case CARDBUS_VCC_5V:
	if (CB_SOCKET_STAT_5VCARD & status) { /* check 5 V card */
	    sock_ctrl &= ~CB_SOCKET_CTRL_VCCMASK;
	    sock_ctrl |= CB_SOCKET_CTRL_VCC_5V;
	} else {
	    printf("%s: BAD voltage request: no 5 V card\n", sc->sc_dev.dv_xname);
	}
	break;
    case CARDBUS_VCC_3V:
	if (CB_SOCKET_STAT_3VCARD & status) {
	    sock_ctrl &= ~CB_SOCKET_CTRL_VCCMASK;
	    sock_ctrl |= CB_SOCKET_CTRL_VCC_3V;
	} else {
	    printf("%s: BAD voltage request: no 3.3 V card\n", sc->sc_dev.dv_xname);
	}
	break;
    case CARDBUS_VCC_0V:
	sock_ctrl &= ~CB_SOCKET_CTRL_VCCMASK;
	break;
    default:
	return 0;			/* power NEVER changed */
	break;
    }

    switch (command & CARDBUS_VPPMASK) {
    case CARDBUS_VPP_UC:
	break;
    case CARDBUS_VPP_0V:
	sock_ctrl &= ~CB_SOCKET_CTRL_VPPMASK;
	break;
    case CARDBUS_VPP_VCC:
	sock_ctrl &= ~CB_SOCKET_CTRL_VPPMASK;
	sock_ctrl |= ((sock_ctrl >> 4) & 0x07);
	break;
    case CARDBUS_VPP_12V:
	sock_ctrl &= ~CB_SOCKET_CTRL_VPPMASK;
	sock_ctrl |= CB_SOCKET_CTRL_VPP_12V;
	break;
    }

#if 0
    DPRINTF(("sock_ctrl: %x\n", sock_ctrl));
#endif
    bus_space_write_4(memt, memh, CB_SOCKET_CTRL, sock_ctrl);
    status = bus_space_read_4(memt, memh, CB_SOCKET_STAT);

    {
	int timeout = 20;
	u_int32_t sockevent;
	do {
	    delay(20*1000);			/* wait 20 ms: Vcc setup time */
	    sockevent = bus_space_read_4 (memt, memh, CB_SOCKET_EVENT);
	} while (!(sockevent & CB_SOCKET_EVENT_POWER) && --timeout > 0);
	/* reset event status */
	bus_space_write_4 (memt, memh, CB_SOCKET_EVENT, sockevent);
	if ( timeout < 0 ) {
	    printf ("VCC supply failed.\n");
	    return 0;
	}
    }
    /* XXX
       delay 400 ms: thgough the standard defines that the Vcc set-up time
       is 20 ms, some PC-Card bridge requires longer duration.
    */
    delay(400*1000);

    if (status & CB_SOCKET_STAT_BADVCC) {		/* bad Vcc request */
	printf("%s: bad Vcc request. sock_ctrl 0x%x, sock_status 0x%x\n",
	       sc->sc_dev.dv_xname, sock_ctrl ,status);
	printf("pccbb_power: %s and %s [%x]\n",
	       (command & CARDBUS_VCCMASK) == CARDBUS_VCC_UC ? "CARDBUS_VCC_UC" :
	       (command & CARDBUS_VCCMASK) == CARDBUS_VCC_5V ? "CARDBUS_VCC_5V" : 
	       (command & CARDBUS_VCCMASK) == CARDBUS_VCC_3V ? "CARDBUS_VCC_3V" : 
	       (command & CARDBUS_VCCMASK) == CARDBUS_VCC_XV ? "CARDBUS_VCC_XV" : 
	       (command & CARDBUS_VCCMASK) == CARDBUS_VCC_YV ? "CARDBUS_VCC_YV" : 
	       (command & CARDBUS_VCCMASK) == CARDBUS_VCC_0V ? "CARDBUS_VCC_0V" :
	       "UNKNOWN",
	       (command & CARDBUS_VPPMASK) == CARDBUS_VPP_UC ? "CARDBUS_VPP_UC" :
	       (command & CARDBUS_VPPMASK) == CARDBUS_VPP_12V ? "CARDBUS_VPP_12V" :
	       (command & CARDBUS_VPPMASK) == CARDBUS_VPP_VCC ? "CARDBUS_VPP_VCC" :
	       (command & CARDBUS_VPPMASK) == CARDBUS_VPP_0V ? "CARDBUS_VPP_0V" :
	       "UNKNOWN",
	       command);
#if defined DIAGNOSTIC
	if (command == (CARDBUS_VCC_0V | CARDBUS_VPP_0V)) {
	    u_int32_t force = bus_space_read_4(memt, memh, CB_SOCKET_FORCE);
	    /* Reset Bad Vcc request */
	    force &= ~CB_SOCKET_FORCE_BADVCC;
	    bus_space_write_4(memt, memh, CB_SOCKET_FORCE, force);
	    printf("new status 0x%x\n", bus_space_read_4(memt, memh,CB_SOCKET_STAT));
	    return 1;
	}
#endif
	return 0;
    }
    return 1;		/* power changed correctly */
}

/**********************************************************************
* static int pccbb_detect_card(struct pccbb_softc *sc)
*   return value:  0 if no card exists.
*                  1 if 16-bit card exists.
*                  2 if cardbus card exists.
**********************************************************************/
static int
pccbb_detect_card(sc)
    struct pccbb_softc *sc;
{
    bus_space_handle_t base_memh = sc->sc_base_memh;
    bus_space_tag_t base_memt = sc->sc_base_memt;
    u_int32_t sockstat = bus_space_read_4(base_memt,base_memh, CB_SOCKET_STAT);
    int retval = 0;

    if (0x00 == (sockstat & CB_SOCKET_STAT_CD)) { /* CD1 and CD2 asserted */
	/* card must be present */
	if (!(CB_SOCKET_STAT_NOTCARD & sockstat)) {	/* NOTACARD DEASSERTED */
	    if (CB_SOCKET_STAT_CB & sockstat) {	/* CardBus mode */
		retval = 2;
	    } else if (CB_SOCKET_STAT_16BIT & sockstat) { /* 16-bit mode */
		retval = 1;
	    }
	}
    }
    return retval;
}




/**********************************************************************
* STATIC int cb_reset(struct pccbb_softc *sc)
*   This function resets the card.
**********************************************************************/
STATIC int
cb_reset(sc)
    struct pccbb_softc *sc;
{
    u_int32_t bcr = pci_conf_read(sc->sc_pc, sc->sc_tag, PCI_BCR_INTR);
    int delay_us;
    delay_us = sc->sc_chipset == CB_RF5C47X ? 400*1000 : 20*1000;

    bcr |= (0x40 << 16);		/* Reset bit Assert (bit 6 at 0x3E) */
    pci_conf_write(sc->sc_pc, sc->sc_tag, PCI_BCR_INTR, bcr);
    /* Reset Assert at least 20 ms */
    delay(delay_us);

    if (CBB_CARDEXIST & sc->sc_flags) { /* A card exists.  Reset it! */
	bcr &= ~(0x40 << 16);	/* Reset bit Deassert (bit 6 at 0x3E) */
	pci_conf_write(sc->sc_pc, sc->sc_tag, PCI_BCR_INTR, bcr);
	delay(delay_us);
    }
				/* No card found on the slot. Keep Reset. */
    return 1;
}

/**********************************************************************
* STATIC int cb_detect_voltage(struct pccbb_softc *sc)
*  This function detect card Voltage.
**********************************************************************/
STATIC int
cb_detect_voltage(sc)
    struct pccbb_softc *sc;
{
    u_int32_t psr;		/* socket present-state reg */
    bus_space_tag_t iot = sc->sc_base_memt;
    bus_space_handle_t ioh = sc->sc_base_memh;
    int vol = PCCARD_VCC_UKN;	/* set 0 */
  
    psr = bus_space_read_4(iot, ioh, CB_SOCKET_STAT);

    if (0x400u & psr) {
	vol |= PCCARD_VCC_5V;
    }
    if (0x800u & psr) {
	vol |= PCCARD_VCC_3V;
    }

    return vol;
}

/**********************************************************************
* STATIC int pccbb_cardenable(struct pccbb_softc *sc, int function)
*   This function enables and disables the card
**********************************************************************/
STATIC int
pccbb_cardenable(sc, function)
    struct pccbb_softc *sc;
    int function;
{
    u_int32_t command = pci_conf_read(sc->sc_pc, sc->sc_tag, PCI_COMMAND_STATUS_REG);

    DPRINTF(("pccbb_cardenable:"));
    switch (function) {
    case CARDBUS_IO_ENABLE:
	command |= PCI_COMMAND_IO_ENABLE;
	break;
    case CARDBUS_IO_DISABLE:
	command &= ~PCI_COMMAND_IO_ENABLE;
	break;
    case CARDBUS_MEM_ENABLE:
	command |= PCI_COMMAND_MEM_ENABLE;
	break;
    case CARDBUS_MEM_DISABLE:
	command &= ~PCI_COMMAND_MEM_ENABLE;
	break;
    case CARDBUS_BM_ENABLE:
	command |= PCI_COMMAND_MASTER_ENABLE;
	break;
    case CARDBUS_BM_DISABLE:
	command &= ~PCI_COMMAND_MASTER_ENABLE;
	break;
    default:
	return 0;
    }

    pci_conf_write(sc->sc_pc, sc->sc_tag, PCI_COMMAND_STATUS_REG, command);
    DPRINTF((" command reg 0x%x\n", command));
    return 1;
}







/**********************************************************************
* int pccbb_io_open(cardbus_chipset_tag_t, int, u_int32_t, u_int32_t)
**********************************************************************/
static int
pccbb_io_open(ct, win, start, end)
    cardbus_chipset_tag_t ct;
    int win;
    u_int32_t start, end;
{
    struct pccbb_softc *sc = (struct pccbb_softc *)ct;
    int basereg;
    int limitreg;

    if ((win < 0) || (win > 2)) {
#if defined DIAGNOSTIC
	printf("cardbus_io_open: window out of range %d\n", win);
#endif
	return 0;
    }

    basereg = win*8 + 0x2c;
    limitreg = win*8 + 0x30;

    DPRINTF(("pccbb_io_open: 0x%x[0x%x] - 0x%x[0x%x]\n",
	     start, basereg, end, limitreg));

    pci_conf_write(sc->sc_pc, sc->sc_tag, basereg, start);
    pci_conf_write(sc->sc_pc, sc->sc_tag, limitreg, end);
    return 1;
}
     
/**********************************************************************
* int pccbb_io_close(cardbus_chipset_tag_t, int)
**********************************************************************/
static int
pccbb_io_close(ct, win)
    cardbus_chipset_tag_t ct;
    int win;
{
    struct pccbb_softc *sc = (struct pccbb_softc *)ct;
    int basereg;
    int limitreg;

    if ((win < 0) || (win > 2)) {
#if defined DIAGNOSTIC
	printf("cardbus_io_close: window out of range %d\n", win);
#endif
	return 0;
    }

    basereg = win*8 + 0x2c;
    limitreg = win*8 + 0x30;

    pci_conf_write(sc->sc_pc, sc->sc_tag, basereg, 0);
    pci_conf_write(sc->sc_pc, sc->sc_tag, limitreg, 0);
    return 1;
}

/**********************************************************************
* int pccbb_mem_open(cardbus_chipset_tag_t, int, u_int32_t, u_int32_t)
**********************************************************************/
static int
pccbb_mem_open(ct, win, start, end)
    cardbus_chipset_tag_t ct;
    int win;
    u_int32_t start, end;
{
    struct pccbb_softc *sc = (struct pccbb_softc *)ct;
    int basereg;
    int limitreg;

    if ((win < 0) || (win > 2)) {
#if defined DIAGNOSTIC
	printf("cardbus_mem_open: window out of range %d\n", win);
#endif
	return 0;
    }

    basereg = win*8 + 0x1c;
    limitreg = win*8 + 0x20;

    pci_conf_write(sc->sc_pc, sc->sc_tag, basereg, start);
    pci_conf_write(sc->sc_pc, sc->sc_tag, limitreg, end);
    return 1;
}


/**********************************************************************
* int pccbb_mem_close(cardbus_chipset_tag_t, int);
**********************************************************************/
static int
pccbb_mem_close(ct, win)
    cardbus_chipset_tag_t ct;
    int win;
{
    struct pccbb_softc *sc = (struct pccbb_softc *)ct;
    int basereg;
    int limitreg;

    if ((win < 0) || (win > 2)) {
#if defined DIAGNOSTIC
	printf("cardbus_mem_close: window out of range %d\n", win);
#endif
	return 0;
    }

    basereg = win*8 + 0x1c;
    limitreg = win*8 + 0x20;

    pci_conf_write(sc->sc_pc, sc->sc_tag, basereg, 0);
    pci_conf_write(sc->sc_pc, sc->sc_tag, limitreg, 0);
    return 1;
}





static void *
pccbb_intr_establish(ct, irq, level, func, arg)
    cardbus_chipset_tag_t ct;
    int irq, level;
    int (* func) __P((void *));
    void *arg;
{
    struct pccbb_softc *sc = (struct pccbb_softc *)ct;

    switch (sc->sc_chipset) {
    case CB_TI113X:
    {
	pcireg_t cbctrl = pci_conf_read(sc->sc_pc, sc->sc_tag, PCI_CBCTRL);
	cbctrl |= PCI113X_CBCTRL_PCI_INTR; /* functional intr enabled */
	pci_conf_write(sc->sc_pc, sc->sc_tag, PCI_CBCTRL, cbctrl);
    }
    break;
    default:
	break;
    }

    return pci_intr_establish(sc->sc_pc, irq, level, (void(*)(void*))func, arg);
}




static void
pccbb_intr_disestablish(ct, ih)
    cardbus_chipset_tag_t ct;
    void *ih;
{
    struct pccbb_softc *sc = (struct pccbb_softc *)ct;
  
    switch (sc->sc_chipset) {
    case CB_TI113X:
    {
	pcireg_t cbctrl = pci_conf_read(sc->sc_pc, sc->sc_tag, PCI_CBCTRL);
	cbctrl &= ~PCI113X_CBCTRL_PCI_INTR; /* functional intr disabled */
	pci_conf_write(sc->sc_pc, sc->sc_tag, PCI_CBCTRL, cbctrl);
    }
    break;
    default:
	break;
    }
  
    pci_intr_disestablish(sc->sc_pc, ih);
}





#if defined SHOW_REGS
static void
cb_show_regs(pc, tag, memt, memh)
    pci_chipset_tag_t pc;
    pcitag_t tag;
    bus_space_tag_t memt;
    bus_space_handle_t memh;
{
    int i;
    printf("PCI config regs:");
    for (i = 0; i < 0x50; i += 4) {
	if (i % 16 == 0) {
	    printf("\n 0x%02x:", i);
	}
	printf(" %08lx", pci_conf_read(pc, tag, i));
    }
    for (i = 0x80; i < 0xb0; i += 4) {
	if (i % 16 == 0) {
	    printf("\n 0x%02x:", i);
	}
	printf(" %08lx", pci_conf_read(pc, tag, i));
    }

    if (memh.addr == 0) {/* XXX */
	printf("\n");
	return;
    }
    
    printf("\nsocket regs:");
    for (i = 0; i <= 0x10; i += 0x04) {
	printf(" %08x", bus_space_read_4(memt, memh, i));
    }
    printf("\nExCA regs:");
    for (i = 0; i < 0x08; ++i) {
	printf(" %02x", bus_space_read_1(memt, memh, 0x800 + i));
    }
    printf("\n");
    return;
}
#endif


/**********************************************************************
* static cardbustag_t pccbb_make_tag(cardbus_chipset_tag_t cc,
*                                    int busno, int devno, int function);
*   This is the function to make a tag to access config space of
*  a CardBus Card.  It works same as pci_conf_read.
**********************************************************************/
static cardbustag_t
pccbb_make_tag(cc, busno, devno, function)
    cardbus_chipset_tag_t cc;
    int busno, devno, function;
{
    struct pccbb_softc *sc = (struct pccbb_softc *)cc;

    return pci_make_tag(sc->sc_pc, busno, devno, function);
}
static void
pccbb_free_tag (cardbus_chipset_tag_t cc, cardbustag_t tag)
{
    struct pccbb_softc *sc = (struct pccbb_softc *)cc;
    pci_free_tag (tag);
    return;
}

/**********************************************************************
* static cardbusreg_t pccbb_conf_read(cardbus_chipset_tag_t cc,
*                                     cardbustag_t tag, int offset)
*   This is the function to read the config space of a CardBus Card.
*  It works same as pci_conf_read.
**********************************************************************/
static cardbusreg_t
pccbb_conf_read(ct, tag, offset)
    cardbus_chipset_tag_t ct;
    cardbustag_t tag;
    int offset;		/* register offset */
{
    struct pccbb_softc *sc = (struct pccbb_softc *)ct;

    return pci_conf_read(sc->sc_pc, tag, offset);
}



/**********************************************************************
* static void pccbb_conf_write(cardbus_chipset_tag_t cc, cardbustag_t tag,
*                              int offs, cardbusreg_t val)
*   This is the function to write the config space of a CardBus Card.
*  It works same as pci_conf_write.
**********************************************************************/
static void
pccbb_conf_write(ct, tag, reg, val)
    cardbus_chipset_tag_t ct;
    cardbustag_t tag;
    int reg;			/* register offset */
    cardbusreg_t val;
{
    struct pccbb_softc *sc = (struct pccbb_softc *)ct;

    pci_conf_write(sc->sc_pc, tag, reg, val);
}

/**********************************************************************
* STATIC int pccbb_pcmcia_io_alloc(pcmcia_chipset_handle_t pch,
*                                  bus_addr_t start, bus_size_t size,
*                                  bus_size_t align,
*                                  struct pcmcia_io_handle *pcihp
*
* This function only allocates I/O region for pccard. This function
* never maps the allcated region to pccard I/O area.
*
* XXX: The interface of this function is not very good, I believe.
**********************************************************************/
    STATIC int 
pccbb_pcmcia_io_alloc(pch, start, size, align, pcihp)
    pcmcia_chipset_handle_t pch;
    bus_addr_t start;	/* start address */
    bus_size_t size;
    bus_size_t align;
    struct pcmcia_io_handle *pcihp;
{
    struct cbb_pcic_handle *ph = (struct cbb_pcic_handle *)pch;
    bus_addr_t ioaddr;
    int flags = 0;
    bus_space_tag_t iot;
    bus_space_handle_t ioh;

    /*
     * Allocate some arbitrary I/O space.
     */

    iot = ph->sc->sc_iot;

    if (start) {
	ioaddr = start;
	if (bus_space_map(iot, start, size, 0, &ioh)) {
	    return 1;
	}
	DPRINTF(("pccbb_pcmcia_io_alloc map port %lx+%lx\n",
		 (u_long) ioaddr, (u_long) size));
    } else {
	flags |= PCMCIA_IO_ALLOCATED;
	if (bus_space_alloc(iot, 0x700/* ph->sc->sc_iobase */,
			    0x800/* ph->sc->sc_iobase + ph->sc->sc_iosize*/,
			    size, align, 0, 0, &ioaddr, &ioh)) {
	    /* No room be able to be get. */
	    return 1;
	}
	DPRINTF(("pccbb_pcmmcia_io_alloc alloc port 0x%lx+0x%lx\n",
		 (u_long) ioaddr, (u_long) size));
    }

    pcihp->iot = iot;
    pcihp->ioh = ioh;
    pcihp->addr = ioaddr;
    pcihp->size = size;
    pcihp->flags = flags;

    return 0;
}

/**********************************************************************
* STATIC int pccbb_pcmcia_io_free(pcmcia_chipset_handle_t pch,
*                                 struct pcmcia_io_handle *pcihp)
*
* This function only frees I/O region for pccard.
*
* XXX: The interface of this function is not very good, I believe.
**********************************************************************/
void 
pccbb_pcmcia_io_free(pch, pcihp)
    pcmcia_chipset_handle_t pch;
    struct pcmcia_io_handle *pcihp;
{
    bus_space_tag_t iot = pcihp->iot;
    bus_space_handle_t ioh = pcihp->ioh;
    bus_size_t size = pcihp->size;

    if (pcihp->flags & PCMCIA_IO_ALLOCATED)
	bus_space_free(iot, ioh, size);
    else
	bus_space_unmap(iot, ioh, size);
}

/**********************************************************************
* STATIC int pccbb_pcmcia_io_map(pcmcia_chipset_handle_t pch, int width,
*                                bus_addr_t offset, bus_size_t size,
*                                struct pcmcia_io_handle *pcihp,
*                                int *windowp)
*
* This function maps the allocated I/O region to pccard. This function
* never allocates any I/O region for pccard I/O area.  I don't
* understand why the original authors of pcmciabus separated alloc and
* map.  I believe the two must be unite.
*
* XXX: no wait timing control?
**********************************************************************/
int 
pccbb_pcmcia_io_map(pch, width, offset, size, pcihp, windowp)
    pcmcia_chipset_handle_t pch;
    int width;
    bus_addr_t offset;
    bus_size_t size;
    struct pcmcia_io_handle *pcihp;
    int *windowp;
{
    struct cbb_pcic_handle *ph = (struct cbb_pcic_handle *) pch;
    bus_addr_t ioaddr = pcihp->addr + offset;
    int i, win;
#if defined CBB_DEBUG
    static char *width_names[] = { "dynamic", "io8", "io16" };
#endif

    /* Sanity check I/O handle. */

    if (ph->sc->sc_iot != pcihp->iot) {
	panic("pccbb_pcmcia_io_map iot is bogus");
    }

    /* XXX Sanity check offset/size. */

    win = -1;
    for (i = 0; i < PCIC_IO_WINS; i++) {
	if ((ph->ioalloc & (1 << i)) == 0) {
	    win = i;
	    ph->ioalloc |= (1 << i);
	    break;
	}
    }

    if (win == -1) {
	return 1;
    }

    *windowp = win;

    /* XXX this is pretty gross */

    DPRINTF(("pccbb_pcmcia_io_map window %d %s port %lx+%lx\n",
	     win, width_names[width], (u_long) ioaddr, (u_long) size));

    /* XXX wtf is this doing here? */

#if 0
    printf(" port 0x%lx", (u_long) ioaddr);
    if (size > 1) {
	printf("-0x%lx", (u_long) ioaddr + (u_long) size - 1);
    }
#endif

    ph->io[win].addr = ioaddr;
    ph->io[win].size = size;
    ph->io[win].width = width;

    /* actual dirty register-value changing in the function below. */
    pccbb_pcmcia_do_io_map(ph, win);

    return 0;
}

/**********************************************************************
* STATIC void pccbb_pcmcia_do_io_map(struct pcic_handle *h, int win)
*
* This function changes register-value to map I/O region for pccard.
**********************************************************************/
static void 
pccbb_pcmcia_do_io_map(ph, win)
    struct cbb_pcic_handle *ph;
    int win;
{
    static u_int8_t pcic_iowidth[3] = {
	PCIC_IOCTL_IO0_IOCS16SRC_CARD,
	PCIC_IOCTL_IO0_IOCS16SRC_DATASIZE | PCIC_IOCTL_IO0_DATASIZE_8BIT,
	PCIC_IOCTL_IO0_IOCS16SRC_DATASIZE | PCIC_IOCTL_IO0_DATASIZE_16BIT,
    };

#define PCIC_SIA_START_LOW 0
#define PCIC_SIA_START_HIGH 1
#define PCIC_SIA_STOP_LOW 2
#define PCIC_SIA_STOP_HIGH 3

    int regbase_win = 0x8 + win*0x04;
    u_int8_t ioctl, enable;

    DPRINTF(("pccbb_pcmcia_do_io_map win %d addr 0x%lx size 0x%lx width %d\n",
	     win, (long) ph->io[win].addr, (long) ph->io[win].size,
	     ph->io[win].width * 8));

    Pcic_write(ph, regbase_win + PCIC_SIA_START_LOW,
	       ph->io[win].addr & 0xff);
    Pcic_write(ph, regbase_win + PCIC_SIA_START_HIGH,
	       (ph->io[win].addr >> 8) & 0xff);

    Pcic_write(ph, regbase_win + PCIC_SIA_STOP_LOW,
	       (ph->io[win].addr + ph->io[win].size - 1) & 0xff);
    Pcic_write(ph, regbase_win + PCIC_SIA_STOP_HIGH,
	       ((ph->io[win].addr + ph->io[win].size - 1) >> 8) & 0xff);

    ioctl = Pcic_read(ph, PCIC_IOCTL);
    enable = Pcic_read(ph, PCIC_ADDRWIN_ENABLE);
    switch (win) {
    case 0:
	ioctl &= ~(PCIC_IOCTL_IO0_WAITSTATE | PCIC_IOCTL_IO0_ZEROWAIT |
		   PCIC_IOCTL_IO0_IOCS16SRC_MASK | PCIC_IOCTL_IO0_DATASIZE_MASK);
	ioctl |= pcic_iowidth[ph->io[win].width];
	enable |= PCIC_ADDRWIN_ENABLE_IO0;
	break;
    case 1:
	ioctl &= ~(PCIC_IOCTL_IO1_WAITSTATE | PCIC_IOCTL_IO1_ZEROWAIT |
		   PCIC_IOCTL_IO1_IOCS16SRC_MASK | PCIC_IOCTL_IO1_DATASIZE_MASK);
	ioctl |= (pcic_iowidth[ph->io[win].width] << 4);
	enable |= PCIC_ADDRWIN_ENABLE_IO1;
	break;
    }
    Pcic_write(ph, PCIC_IOCTL, ioctl);
    Pcic_write(ph, PCIC_ADDRWIN_ENABLE, enable);
#if defined CBB_DEBUG
    {
	u_int8_t start_low = Pcic_read(ph, regbase_win + PCIC_SIA_START_LOW);
	u_int8_t start_high = Pcic_read(ph, regbase_win + PCIC_SIA_START_HIGH);
	u_int8_t stop_low = Pcic_read(ph, regbase_win + PCIC_SIA_STOP_LOW);
	u_int8_t stop_high = Pcic_read(ph, regbase_win + PCIC_SIA_STOP_HIGH);
	printf(" start %02x %02x, stop %02x %02x, ioctl %02x enable %02x\n",
	       start_low, start_high, stop_low, stop_high, ioctl, enable);
    }
#endif
}

/**********************************************************************
* STATIC void pccbb_pcmcia_io_unmap(pcmcia_chipset_handle_t *h, int win)
*
* This function unmapss I/O region.  No return value.
**********************************************************************/
STATIC void 
pccbb_pcmcia_io_unmap(pch, win)
    pcmcia_chipset_handle_t pch;
    int win;
{
    struct cbb_pcic_handle *ph = (struct cbb_pcic_handle *)pch;
    int reg;

    if (win >= PCIC_IO_WINS || win < 0) {
	panic("pccbb_pcmcia_io_unmap: window out of range");
    }

    reg = Pcic_read(ph, PCIC_ADDRWIN_ENABLE);
    switch (win) {
    case 0:
	reg &= ~PCIC_ADDRWIN_ENABLE_IO0;
	break;
    case 1:
	reg &= ~PCIC_ADDRWIN_ENABLE_IO1;
	break;
    }
    Pcic_write(ph, PCIC_ADDRWIN_ENABLE, reg);

    ph->ioalloc &= ~(1 << win);
}

/**********************************************************************
* static void pccbb_pcmcia_wait_ready(struct cbb_pcic_handle *ph)
*
* This function enables the card.  All information is stored in
* the first argument, pcmcia_chipset_handle_t.
**********************************************************************/
static void
pccbb_pcmcia_wait_ready(ph)
    struct cbb_pcic_handle *ph;
{
    int i;

    DPRINTF(("pccbb_pcmcia_wait_ready: status 0x%02x\n",
	     Pcic_read(ph, PCIC_IF_STATUS)));

    for (i = 0; i < 10000; i++) {
	if (Pcic_read(ph, PCIC_IF_STATUS) & PCIC_IF_STATUS_READY) {
	    return;
	}
	delay(500);
#ifdef CBB_DEBUG
	if ((i > 5000) && (i%100 == 99))
	    printf(".");
#endif
    }

#ifdef DIAGNOSTIC
    printf("pcic_wait_ready: ready never happened, status = %02x\n",
	   Pcic_read(ph, PCIC_IF_STATUS));
#endif
}

/**********************************************************************
* STATIC void pccbb_pcmcia_socket_enable(pcmcia_chipset_handle_t pch)
*
* This function enables the card.  All information is stored in
* the first argument, pcmcia_chipset_handle_t.
**********************************************************************/
STATIC void
pccbb_pcmcia_socket_enable(pch)
    pcmcia_chipset_handle_t pch;
{
    struct cbb_pcic_handle *ph = (struct cbb_pcic_handle *)pch;
    struct pccbb_softc *sc = ph->sc;
    struct pcmcia_softc *psc = (void*)ph->pcmcia;
    int cardtype, win;
    u_int8_t power, intr;
    pcireg_t spsr;
    int voltage;
#define PCIC_INTR_PCI PCIC_INTR_ENABLE

    /* this bit is mostly stolen from pcic_attach_card */

    DPRINTF(("pccbb_pcmcia_socket_enable:\n"));

    /* get card Vcc info */

    spsr = bus_space_read_4(sc->sc_base_memt, sc->sc_base_memh, CB_SOCKET_STAT);
    if (spsr & CB_SOCKET_STAT_5VCARD) {
	printf("5V card\n");	/* XXX */
	voltage = CARDBUS_VCC_5V | CARDBUS_VPP_VCC;
    } else if (spsr & CB_SOCKET_STAT_3VCARD) {
	printf("3V card\n");	/* XXX */
	voltage = CARDBUS_VCC_3V | CARDBUS_VPP_VCC;
    } else {
	printf("?V card, 0x%x\n", spsr);	/* XXX */
	return;
    }

    /* assert reset bit */

    intr = Pcic_read(ph, PCIC_INTR);
    intr &= ~PCIC_INTR_RESET;
    intr |= PCIC_INTR_PCI;	/* XXX */
    Pcic_write(ph, PCIC_INTR, intr);

    /* disable socket i/o: negate output enable bit */

    power = Pcic_read(ph, PCIC_PWRCTL);
    power &= ~PCIC_PWRCTL_OE;
    Pcic_write(ph, PCIC_PWRCTL, power);

    /* power down the socket to reset it, clear the card reset pin */

    pccbb_power(sc, CARDBUS_VCC_0V | CARDBUS_VPP_0V);

    /* 
     * wait 300ms until power fails (Tpf).  Then, wait 100ms since
     * we are changing Vcc (Toff).
     */
    delay((300 + 100)*1000);

    /* power up the socket */
    pccbb_power(sc, voltage);

    /*
     * wait 100ms until power raise (Tpr) and 20ms to become
     * stable (Tsu(Vcc)).
     *
     * some machines require some more time to be settled
     * (another 200ms is added here).
     */
    delay((100 + 20 + 200)*1000);

    power = Pcic_read(ph, PCIC_PWRCTL);
    Pcic_write(ph, PCIC_PWRCTL, power | PCIC_PWRCTL_OE);

    /*
     * hold RESET at least 10us.
     */
    delay(10);
    delay(2*1000);		/* XXX: TI1130 requires it. */

    /* clear the reset flag */

    intr = Pcic_read(ph, PCIC_INTR);
    Pcic_write(ph, PCIC_INTR, intr | PCIC_INTR_RESET);

    /* wait 20ms as per pc card standard (r2.01) section 4.3.6 */

    delay(20000);

    /* wait for the chip to finish initializing */

    pccbb_pcmcia_wait_ready(ph);

    /* zero out the address windows */

    Pcic_write(ph, PCIC_ADDRWIN_ENABLE, 0);

    /* set the card type */

    cardtype = psc->sc_if.if_card_gettype(ph->pcmcia);

    intr = Pcic_read(ph, PCIC_INTR);
    intr &= ~PCIC_INTR_CARDTYPE_MASK;
    intr |= ((cardtype == PCMCIA_IFTYPE_IO) ?
	     PCIC_INTR_CARDTYPE_IO :
	     PCIC_INTR_CARDTYPE_MEM);
    Pcic_write(ph, PCIC_INTR, intr);

    DPRINTF(("%s: pccbb_pcmcia_socket_enable %02x cardtype %s %02x\n",
	     ph->sc->sc_dev.dv_xname, ph->sock,
	     ((cardtype == PCMCIA_IFTYPE_IO) ? "io" : "mem"), intr));

    /* reinstall all the memory and io mappings */

    for (win = 0; win < PCIC_MEM_WINS; ++win) {
	if (ph->memalloc & (1 << win)) {
	    pccbb_pcmcia_do_mem_map(ph, win);
	}
    }

    for (win = 0; win < PCIC_IO_WINS; ++win) {
	if (ph->ioalloc & (1 << win)) {
	    pccbb_pcmcia_do_io_map(ph, win);
	}
    }
}

/**********************************************************************
* STATIC void pccbb_pcmcia_socket_disable(pcmcia_chipset_handle_t *ph)
*
* This function disables the card.  All information is stored in
* the first argument, pcmcia_chipset_handle_t.
**********************************************************************/
STATIC void
pccbb_pcmcia_socket_disable(pch)
    pcmcia_chipset_handle_t pch;
{
    struct cbb_pcic_handle *ph = (struct cbb_pcic_handle *)pch;
    struct pccbb_softc *sc = ph->sc;
    u_int8_t power, intr;

    DPRINTF(("pccbb_pcmcia_socket_disable\n"));

    /* reset signal asserting... */

    intr = Pcic_read(ph, PCIC_INTR);
    intr &= ~PCIC_INTR_RESET;
    Pcic_write(ph, PCIC_INTR, intr);
    delay(2*1000);

    /* power down the socket */
    power = Pcic_read(ph, PCIC_PWRCTL);
    power &= ~PCIC_PWRCTL_OE;
    Pcic_write(ph, PCIC_PWRCTL, power);
    pccbb_power(sc, CARDBUS_VCC_0V | CARDBUS_VPP_0V);

    /*
     * wait 300ms until power fails (Tpf).
     */
    delay(300 * 1000);
}

/**********************************************************************
* STATIC int pccbb_pcmcia_mem_alloc(pcmcia_chipset_handle_t pch,
*                                   bus_size_t size,
*                                   struct pcmcia_mem_handle *pcmhp)
*
* This function only allocates memory region for pccard. This
* function never maps the allcated region to pccard memory area.
*
* XXX: Why the argument of start address is not in?
**********************************************************************/
STATIC int 
pccbb_pcmcia_mem_alloc(pch, size, pcmhp)
    pcmcia_chipset_handle_t pch;
    bus_size_t size;
    struct pcmcia_mem_handle *pcmhp;
{
    struct cbb_pcic_handle *ph = (struct cbb_pcic_handle *)pch;
    bus_space_handle_t memh;
    bus_addr_t addr;
    bus_size_t sizepg;
    struct pccbb_softc *sc = ph->sc;

    /* out of sc->memh, allocate as many pages as necessary */

    /* convert size to PCIC pages */
    /*
      This is not enough; when the requested region is on the
      page boundaries, this may calculate wrong result.
    */
    sizepg = (size + (PCIC_MEM_PAGESIZE - 1)) / PCIC_MEM_PAGESIZE;
#if 0
    if (sizepg > PCIC_MAX_MEM_PAGES) {
	return 1;
    }
#endif

    if (!(sc->sc_pcmcia_flags & PCCBB_PCMCIA_MEM_32)) {
	return 1;
    }

    addr = 0;		/* XXX gcc -Wuninitialized */

    if (bus_space_alloc(sc->sc_memt, sc->sc_mem_start, sc->sc_mem_end,
			sizepg*PCIC_MEM_PAGESIZE, PCIC_MEM_PAGESIZE,
			0 /* boundary */, 0 /* flags */,
			&addr, &memh)) {
	return 1;
    }

    DPRINTF(("pccbb_pcmcia_alloc_mem: addr 0x%lx size 0x%lx, realsize 0x%lx\n",
	     (u_long)addr, (u_long)size, (u_long)sizepg*PCIC_MEM_PAGESIZE));

    pcmhp->memt = sc->sc_memt;
    pcmhp->memh = memh;
    pcmhp->addr = addr;
    pcmhp->size = size;
    pcmhp->realsize = sizepg * PCIC_MEM_PAGESIZE;
    /* What is mhandle?  I feel it is very dirty and it must go trush. */
    pcmhp->mhandle = 0;
    /* No offset???  Funny. */

    return 0;
}

/**********************************************************************
* STATIC void pccbb_pcmcia_mem_free(pcmcia_chipset_handle_t pch,
*                                   struct pcmcia_mem_handle *pcmhp)
*
* This function release the memory space allocated by the fuction
* pccbb_pcmcia_mem_alloc().
**********************************************************************/
STATIC void 
pccbb_pcmcia_mem_free(pch, pcmhp)
    pcmcia_chipset_handle_t pch;
    struct pcmcia_mem_handle *pcmhp;
{
    bus_space_free(pcmhp->memt, pcmhp->memh, pcmhp->realsize);
}

/**********************************************************************
* STATIC void pccbb_pcmcia_do_mem_map(struct cbb_pcic_handle *ph,
*                                     int win)
*
* This function release the memory space allocated by the fuction
* pccbb_pcmcia_mem_alloc().
**********************************************************************/
STATIC void 
pccbb_pcmcia_do_mem_map(ph, win)
    struct cbb_pcic_handle *ph;
    int win;
{
    int regbase_win;
    bus_addr_t phys_addr;
    bus_addr_t phys_end;

#define PCIC_SMM_START_LOW 0
#define PCIC_SMM_START_HIGH 1
#define PCIC_SMM_STOP_LOW 2
#define PCIC_SMM_STOP_HIGH 3
#define PCIC_CMA_LOW 4
#define PCIC_CMA_HIGH 5

    u_int8_t start_low, start_high = 0;
    u_int8_t stop_low, stop_high;
    u_int8_t off_low, off_high;
    u_int8_t mem_window;
    int reg;

    regbase_win = 0x10 + win*0x08;

    phys_addr = ph->mem[win].addr;
    phys_end = phys_addr + ph->mem[win].size;

    DPRINTF(("pccbb_pcmcia_do_mem_map: start 0x%lx end 0x%lx off 0x%lx\n",
	     (u_long)phys_addr, (u_long)phys_end, ph->mem[win].offset));

#define PCIC_MEMREG_LSB_SHIFT PCIC_SYSMEM_ADDRX_SHIFT
#define PCIC_MEMREG_MSB_SHIFT (PCIC_SYSMEM_ADDRX_SHIFT + 8)
#define PCIC_MEMREG_WIN_SHIFT (PCIC_SYSMEM_ADDRX_SHIFT + 12)

    start_low = (phys_addr >> PCIC_MEMREG_LSB_SHIFT) & 0xff; /* bit 19:12 */
    start_high = ((phys_addr >> PCIC_MEMREG_MSB_SHIFT) & 0x0f) /* bit 23:20 */
	| PCIC_SYSMEM_ADDRX_START_MSB_DATASIZE_16BIT; /* bit 7 on */
    /* bit 31:24, for 32-bit address */
    mem_window = (phys_addr >> PCIC_MEMREG_WIN_SHIFT) & 0xff; /* bit 31:24 */

    Pcic_write(ph, regbase_win + PCIC_SMM_START_LOW, start_low);
    Pcic_write(ph, regbase_win + PCIC_SMM_START_HIGH, start_high);
  
    if (ph->sc->sc_pcmcia_flags & PCCBB_PCMCIA_MEM_32) {
	Pcic_write(ph, 0x40 + win, mem_window);
    }


#if 0
    /* XXX do I want 16 bit all the time? */
    PCIC_SYSMEM_ADDRX_START_MSB_DATASIZE_16BIT;
#endif


    stop_low = (phys_end >> PCIC_MEMREG_LSB_SHIFT) & 0xff;
    stop_high = ((phys_end >> PCIC_MEMREG_MSB_SHIFT) & 0x0f)
	| PCIC_SYSMEM_ADDRX_STOP_MSB_WAIT2;	/* wait 2 cycles */
    /* XXX Geee, WAIT2!! Crazy!!  I must rewrite this routine. */

    Pcic_write(ph, regbase_win + PCIC_SMM_STOP_LOW, stop_low);
    Pcic_write(ph, regbase_win + PCIC_SMM_STOP_HIGH, stop_high);

    off_low = (ph->mem[win].offset >> PCIC_CARDMEM_ADDRX_SHIFT) & 0xff;
    off_high = ((ph->mem[win].offset >> (PCIC_CARDMEM_ADDRX_SHIFT + 8))
		& PCIC_CARDMEM_ADDRX_MSB_ADDR_MASK)
	| ((ph->mem[win].kind == PCMCIA_MEM_ATTR) ?
	   PCIC_CARDMEM_ADDRX_MSB_REGACTIVE_ATTR : 0);

    Pcic_write(ph, regbase_win + PCIC_CMA_LOW, off_low);
    Pcic_write(ph, regbase_win + PCIC_CMA_HIGH, off_high);

    reg = Pcic_read(ph, PCIC_ADDRWIN_ENABLE);
    reg |= ((1 << win) | PCIC_ADDRWIN_ENABLE_MEMCS16);
    Pcic_write(ph, PCIC_ADDRWIN_ENABLE, reg);

#if defined CBB_DEBUG
    {
	int r1, r2, r3, r4, r5, r6, r7 = 0;

	r1 = Pcic_read(ph, regbase_win + PCIC_SMM_START_LOW);
	r2 = Pcic_read(ph, regbase_win + PCIC_SMM_START_HIGH);
	r3 = Pcic_read(ph, regbase_win + PCIC_SMM_STOP_LOW);
	r4 = Pcic_read(ph, regbase_win + PCIC_SMM_STOP_HIGH);
	r5 = Pcic_read(ph, regbase_win + PCIC_CMA_LOW);
	r6 = Pcic_read(ph, regbase_win + PCIC_CMA_HIGH);
	if (ph->sc->sc_pcmcia_flags & PCCBB_PCMCIA_MEM_32) {
	    r7 = Pcic_read(ph, 0x40 + win);
	}

	DPRINTF(("pccbb_pcmcia_do_mem_map window %d: %02x%02x %02x%02x "
		 "%02x%02x", win, r1, r2, r3, r4, r5, r6));
	if (ph->sc->sc_pcmcia_flags & PCCBB_PCMCIA_MEM_32) {
	    DPRINTF((" %02x",r7));
	}
	DPRINTF(("\n"));
    }
#endif
}

/**********************************************************************
* STATIC int pccbb_pcmcia_mem_map(pcmcia_chipset_handle_t pch, int kind,
*                                 bus_addr_t card_addr, bus_size_t size,
*                                 struct pcmcia_mem_handle *pcmhp,
*                                 bus_addr_t *offsetp, int *windowp)
*
* This function maps memory space allocated by the fuction
* pccbb_pcmcia_mem_alloc().
**********************************************************************/
STATIC int 
pccbb_pcmcia_mem_map(pch, kind, card_addr, size, pcmhp, offsetp, windowp)
    pcmcia_chipset_handle_t pch;
    int kind;
    bus_addr_t card_addr;
    bus_size_t size;
    struct pcmcia_mem_handle *pcmhp;
    bus_addr_t *offsetp;
    int *windowp;
{
    struct cbb_pcic_handle *ph = (struct cbb_pcic_handle *)pch;
    bus_addr_t busaddr;
    long card_offset;
    int win;

    for (win = 0; win < PCIC_MEM_WINS; ++win) {
	if ((ph->memalloc & (1 << win)) == 0) {
	    ph->memalloc |= (1 << win);
	    break;
	}
    }

    if (win == PCIC_MEM_WINS) {
	return 1;
    }

    *windowp = win;

    /* XXX this is pretty gross */

    if (ph->sc->sc_memt != pcmhp->memt) {
	panic("pccbb_pcmcia_mem_map memt is bogus");
    }

    busaddr = pcmhp->addr;

    /*
     * compute the address offset to the pcmcia address space for the
     * pcic.  this is intentionally signed.  The masks and shifts below
     * will cause TRT to happen in the pcic registers.  Deal with making
     * sure the address is aligned, and return the alignment offset.
     */

    *offsetp = card_addr % PCIC_MEM_PAGESIZE;
    card_addr -= *offsetp;

    DPRINTF(("pccbb_pcmcia_mem_map window %d bus %lx+%lx+%lx at card addr "
	     "%lx\n", win, (u_long)busaddr, (u_long)*offsetp, (u_long)size,
	     (u_long)card_addr));

    /*
     * include the offset in the size, and decrement size by one, since
     * the hw wants start/stop
     */
    size += *offsetp - 1;

    card_offset = (((long) card_addr) - ((long) busaddr));

    ph->mem[win].addr = busaddr;
    ph->mem[win].size = size;
    ph->mem[win].offset = card_offset;
    ph->mem[win].kind = kind;

    pccbb_pcmcia_do_mem_map(ph, win);

    return 0;
}

/**********************************************************************
* STATIC int pccbb_pcmcia_mem_unmap(pcmcia_chipset_handle_t pch,
*                                   int window)
*
* This function unmaps memory space which mapped by the fuction
* pccbb_pcmcia_mem_map().
**********************************************************************/
STATIC void 
pccbb_pcmcia_mem_unmap(pch, window)
    pcmcia_chipset_handle_t pch;
    int window;
{
    struct cbb_pcic_handle *ph = (struct cbb_pcic_handle *)pch;
    int reg;

    if (window >= PCIC_MEM_WINS) {
	panic("pccbb_pcmcia_mem_unmap: window out of range");
    }

    reg = Pcic_read(ph, PCIC_ADDRWIN_ENABLE);
    reg &= ~(1 << window);
    Pcic_write(ph, PCIC_ADDRWIN_ENABLE, reg);

    ph->memalloc &= ~(1 << window);
}



#if defined PCCBB_PCMCIA_POLL
struct pccbb_poll_str {
    void *arg;
    int (* func) __P((void *));
    int level;
    struct cbb_pcic_handle *ph;
    int count;
    int num;
};

static struct pccbb_poll_str pccbb_poll[10];
static int pccbb_poll_n = 0;

static void pccbb_pcmcia_poll __P((void *arg));

static void
pccbb_pcmcia_poll(arg)
    void *arg;
{
    struct pccbb_poll_str *poll = arg;
    struct cbb_pcic_handle *ph = poll->ph;
    struct pccbb_softc *sc = ph->sc;
    int s;
    u_int32_t spsr;		/* socket present-state reg */

    timeout(pccbb_pcmcia_poll, arg, hz*2);
    switch (poll->level) {
    case IPL_NET:
	s = splnet();
	break;
    case IPL_BIO:
	s = splbio();
	break;
    case IPL_TTY:			/* fallthrough */
    default:
	s = spltty();
	break;
    }
    
    spsr = bus_space_read_4(sc->sc_base_memt, sc->sc_base_memh, CB_SOCKET_STAT);
//  printf("pccbb_pcmcia_poll: socket 0x%08x\n", spsr);

#if defined PCCBB_PCMCIA_POLL_ONLY && defined LEVEL2
    if (!(spsr & 0x40))		/* CINT low */
#else
	if (1)
#endif
	{
	    if ((*poll->func)(poll->arg) > 0) {
		++poll->count;
//	printf("intr: reported from poller, 0x%x\n", spsr);
#if defined LEVEL2
	    } else {
		printf("intr: miss! 0x%x\n", spsr);
#endif
	    }
	}
    splx(s);
}
#endif /* defined CB_PCMCIA_POLL */

/**********************************************************************
* STATIC void *pccbb_pcmcia_intr_establish(pcmcia_chipset_handle_t pch,
*                                          struct pcmcia_function *pf,
*                                          int ipl,
*                                          int (*func)(void *),
*                                          void *arg);
*
* This function enables PC-Card interrupt.  PCCBB uses PCI interrupt line.
**********************************************************************/
STATIC void *
pccbb_pcmcia_intr_establish(pch, pf, ipl, func, arg)
    pcmcia_chipset_handle_t pch;
    struct pcmcia_function *pf;
    int ipl;
    int (*func) __P((void *));
    void *arg;
{
    struct cbb_pcic_handle *ph = (struct cbb_pcic_handle *)pch;
    struct pccbb_softc *sc = ph->sc;
    pci_intr_handle_t handle;
    void *ih;

    if (!(pf->cfe->flags & PCMCIA_CFE_IRQLEVEL)) {
	/* what should I do? */
	if ((pf->cfe->flags & PCMCIA_CFE_IRQLEVEL)) {
	    DPRINTF(("%s does not provide edge nor pulse interrupt\n",
		     sc->sc_dev.dv_xname));
	    return NULL;
	}
	/* XXX Noooooo!  The interrupt flag must set properly!! */
	/* dumb pcmcia driver!! */
    }

    if (pci_intr_map(sc->sc_pc, sc->sc_intrtag, sc->sc_intrpin,
		     sc->sc_intrline, &handle)) {
	printf("%s: couldn't map interrupt\n", sc->sc_dev.dv_xname);
	return NULL;
    }

    DPRINTF(("pccbb_pcmcia_intr_establish: line %d, handle %d\n",
	     sc->sc_intrline, handle));

    if (NULL != (ih = pci_intr_establish(sc->sc_pc, handle, ipl, (void(*)(void*))func, arg)))
    {
	u_int32_t cbctrl;

	if ((CB_TI113X == sc->sc_chipset)) {
	    cbctrl = pci_conf_read(sc->sc_pc, sc->sc_tag, PCI_CBCTRL);
	    cbctrl |= PCI113X_CBCTRL_PCI_INTR; /* PCI functional intr req */
	    pci_conf_write(sc->sc_pc, sc->sc_tag, PCI_CBCTRL, cbctrl);
	}
    }
#if defined PCCBB_PCMCIA_POLL
    if (pccbb_poll_n < 10) {
	pccbb_poll[pccbb_poll_n].arg = arg;
	pccbb_poll[pccbb_poll_n].func = func;
	pccbb_poll[pccbb_poll_n].level = ipl;
	pccbb_poll[pccbb_poll_n].count = 0;
	pccbb_poll[pccbb_poll_n].num = pccbb_poll_n;
	pccbb_poll[pccbb_poll_n].ph = ph;
	timeout(pccbb_pcmcia_poll, &pccbb_poll[pccbb_poll_n++], hz*2);
	printf("polling set\n");
    }
#endif
#if defined SHOW_REGS
    cb_show_regs(sc->sc_pc, sc->sc_tag, sc->sc_base_memt, sc->sc_base_memh);
#endif

    return ih;
}

/**********************************************************************
* STATIC void pccbb_pcmcia_intr_disestablish(pcmcia_chipset_handle_t pch,
*                                            void *ih)
*
* This function disables PC-Card interrupt.
**********************************************************************/
STATIC void
pccbb_pcmcia_intr_disestablish(pch, ih)
    pcmcia_chipset_handle_t pch;
    void *ih;
{
    struct cbb_pcic_handle *ph = (struct cbb_pcic_handle *)pch;
    struct pccbb_softc *sc = ph->sc;

    pci_intr_disestablish(sc->sc_pc, ih);
}

static int
pccbb_pcmcia_submatch(parent, cf, aux)
    struct device *parent;
    struct cfdata *cf;
    void *aux;
{
    struct pcmciabus_attach_args *paa = aux;
    struct cbb_pcic_handle *ph = (struct cbb_pcic_handle *)paa->pch;

    if (cf->cf_loc[PCMCIABUSCF_CONTROLLER] != PCMCIABUSCF_CONTROLLER_DEFAULT
	&& cf->cf_loc[PCMCIABUSCF_CONTROLLER] != 0) {
	return 0;
    }

    if ((cf->cf_loc[PCMCIABUSCF_CONTROLLER] == PCMCIABUSCF_CONTROLLER_DEFAULT)
	|| cf->cf_loc[PCMCIABUSCF_CONTROLLER] != ph->sock) {
	return ((*cf->cf_attach->ca_match)(parent, cf, aux));
    }

    return 0;
}

static int
pccbb_pcmcia_print(arg, pnp)
    void *arg;
    const char *pnp;
{
    struct pcmciabus_attach_args *paa = arg;
    struct cbb_pcic_handle *ph = (struct cbb_pcic_handle *)paa->pch;

    if (pnp) {
	printf("pcmcia at %s", pnp);
    }

    printf(" slot %d", ph->sock);

    return UNCONF;
}
STATIC int
cbbprint(aux, pcic)
    void *aux;
    const char *pcic;
{
/*
  struct cbslot_attach_args *cba = aux;

  if (cba->cba_slot >= 0) {
  printf(" slot %d", cba->cba_slot);
  }
*/
    return UNCONF;
}
