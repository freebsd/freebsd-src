/*-
 * Copyright (c) 2002-2004 M. Warner Losh.
 * Copyright (c) 2000-2001 Jonathan Chen.
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
 */

/*-
 * Copyright (c) 1998, 1999 and 2000
 *      HAYAKAWA Koichi.  All rights reserved.
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

/*
 * Driver for PCI to CardBus Bridge chips
 * and PCI to PCMCIA Bridge chips
 * and ISA to PCMCIA host adapters
 * and C Bus to PCMCIA host adapters
 *
 * References:
 *  TI Datasheets:
 *   http://www-s.ti.com/cgi-bin/sc/generic2.cgi?family=PCI+CARDBUS+CONTROLLERS
 *
 * Written by Jonathan Chen <jon@freebsd.org>
 * The author would like to acknowledge:
 *  * HAYAKAWA Koichi: Author of the NetBSD code for the same thing
 *  * Warner Losh: Newbus/newcard guru and author of the pccard side of things
 *  * YAMAMOTO Shigeru: Author of another FreeBSD cardbus driver
 *  * David Cross: Author of the initial ugly hack for a specific cardbus card
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/condvar.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/rman.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <machine/clock.h>

#include <dev/pccard/pccardreg.h>
#include <dev/pccard/pccardvar.h>

#include <dev/exca/excareg.h>
#include <dev/exca/excavar.h>

#include <dev/pccbb/pccbbreg.h>
#include <dev/pccbb/pccbbvar.h>

#include "power_if.h"
#include "card_if.h"
#include "pcib_if.h"

#define	DPRINTF(x) do { if (cbb_debug) printf x; } while (0)
#define	DEVPRINTF(x) do { if (cbb_debug) device_printf x; } while (0)

#define	PCI_MASK_CONFIG(DEV,REG,MASK,SIZE)				\
	pci_write_config(DEV, REG, pci_read_config(DEV, REG, SIZE) MASK, SIZE)
#define	PCI_MASK2_CONFIG(DEV,REG,MASK1,MASK2,SIZE)			\
	pci_write_config(DEV, REG, (					\
		pci_read_config(DEV, REG, SIZE) MASK1) MASK2, SIZE)

#define CBB_CARD_PRESENT(s) ((s & CBB_STATE_CD) == 0)

#define CBB_START_MEM	0x88000000
#define CBB_START_32_IO 0x1000
#define CBB_START_16_IO 0x100

devclass_t cbb_devclass;

/* sysctl vars */
SYSCTL_NODE(_hw, OID_AUTO, cbb, CTLFLAG_RD, 0, "CBB parameters");

/* There's no way to say TUNEABLE_LONG to get the right types */
u_long cbb_start_mem = CBB_START_MEM;
TUNABLE_INT("hw.cbb.start_memory", (int *)&cbb_start_mem);
SYSCTL_ULONG(_hw_cbb, OID_AUTO, start_memory, CTLFLAG_RW,
    &cbb_start_mem, CBB_START_MEM,
    "Starting address for memory allocations");

u_long cbb_start_16_io = CBB_START_16_IO;
TUNABLE_INT("hw.cbb.start_16_io", (int *)&cbb_start_16_io);
SYSCTL_ULONG(_hw_cbb, OID_AUTO, start_16_io, CTLFLAG_RW,
    &cbb_start_16_io, CBB_START_16_IO,
    "Starting ioport for 16-bit cards");

u_long cbb_start_32_io = CBB_START_32_IO;
TUNABLE_INT("hw.cbb.start_32_io", (int *)&cbb_start_32_io);
SYSCTL_ULONG(_hw_cbb, OID_AUTO, start_32_io, CTLFLAG_RW,
    &cbb_start_32_io, CBB_START_32_IO,
    "Starting ioport for 32-bit cards");

int cbb_debug = 0;
TUNABLE_INT("hw.cbb.debug", &cbb_debug);
SYSCTL_ULONG(_hw_cbb, OID_AUTO, debug, CTLFLAG_RW, &cbb_debug, 0,
    "Verbose cardbus bridge debugging");

static void	cbb_insert(struct cbb_softc *sc);
static void	cbb_removal(struct cbb_softc *sc);
static uint32_t	cbb_detect_voltage(device_t brdev);
static void	cbb_cardbus_reset(device_t brdev);
static int	cbb_cardbus_io_open(device_t brdev, int win, uint32_t start,
		    uint32_t end);
static int	cbb_cardbus_mem_open(device_t brdev, int win,
		    uint32_t start, uint32_t end);
static void	cbb_cardbus_auto_open(struct cbb_softc *sc, int type);
static int	cbb_cardbus_activate_resource(device_t brdev, device_t child,
		    int type, int rid, struct resource *res);
static int	cbb_cardbus_deactivate_resource(device_t brdev,
		    device_t child, int type, int rid, struct resource *res);
static struct resource	*cbb_cardbus_alloc_resource(device_t brdev,
		    device_t child, int type, int *rid, u_long start,
		    u_long end, u_long count, u_int flags);
static int	cbb_cardbus_release_resource(device_t brdev, device_t child,
		    int type, int rid, struct resource *res);
static int	cbb_cardbus_power_enable_socket(device_t brdev,
		    device_t child);
static void	cbb_cardbus_power_disable_socket(device_t brdev,
		    device_t child);
static void	cbb_func_intr(void *arg);

static void
cbb_remove_res(struct cbb_softc *sc, struct resource *res)
{
	struct cbb_reslist *rle;

	SLIST_FOREACH(rle, &sc->rl, link) {
		if (rle->res == res) {
			SLIST_REMOVE(&sc->rl, rle, cbb_reslist, link);
			free(rle, M_DEVBUF);
			return;
		}
	}
}

static struct resource *
cbb_find_res(struct cbb_softc *sc, int type, int rid)
{
	struct cbb_reslist *rle;
	
	SLIST_FOREACH(rle, &sc->rl, link)
		if (SYS_RES_MEMORY == rle->type && rid == rle->rid)
			return (rle->res);
	return (NULL);
}

static void
cbb_insert_res(struct cbb_softc *sc, struct resource *res, int type,
    int rid)
{
	struct cbb_reslist *rle;

	/*
	 * Need to record allocated resource so we can iterate through
	 * it later.
	 */
	rle = malloc(sizeof(struct cbb_reslist), M_DEVBUF, M_NOWAIT);
	if (rle == NULL)
		panic("cbb_cardbus_alloc_resource: can't record entry!");
	rle->res = res;
	rle->type = type;
	rle->rid = rid;
	SLIST_INSERT_HEAD(&sc->rl, rle, link);
}

static void
cbb_destroy_res(struct cbb_softc *sc)
{
	struct cbb_reslist *rle;

	while ((rle = SLIST_FIRST(&sc->rl)) != NULL) {
		device_printf(sc->dev, "Danger Will Robinson: Resource "
		    "left allocated!  This is a bug... "
		    "(rid=%x, type=%d, addr=%lx)\n", rle->rid, rle->type,
		    rman_get_start(rle->res));
		SLIST_REMOVE_HEAD(&sc->rl, link);
		free(rle, M_DEVBUF);
	}
}

/*
 * Disable function interrupts by telling the bridge to generate IRQ1
 * interrupts.  These interrupts aren't really generated by the chip, since
 * IRQ1 is reserved.  Some chipsets assert INTA# inappropriately during
 * initialization, so this helps to work around the problem.
 *
 * XXX We can't do this workaround for all chipsets, because this
 * XXX causes interference with the keyboard because somechipsets will
 * XXX actually signal IRQ1 over their serial interrupt connections to
 * XXX the south bridge.  Disable it it for now.
 */
void
cbb_disable_func_intr(struct cbb_softc *sc)
{
#if 0
	uint8_t reg;

	reg = (exca_getb(&sc->exca[0], EXCA_INTR) & ~EXCA_INTR_IRQ_MASK) | 
	    EXCA_INTR_IRQ_RESERVED1;
	exca_putb(&sc->exca[0], EXCA_INTR, reg);
#endif
}

/*
 * Enable function interrupts.  We turn on function interrupts when the card
 * requests an interrupt.  The PCMCIA standard says that we should set
 * the lower 4 bits to 0 to route via PCI.  Note: we call this for both
 * CardBus and R2 (PC Card) cases, but it should have no effect on CardBus
 * cards.
 */
static void
cbb_enable_func_intr(struct cbb_softc *sc)
{
	uint8_t reg;

	reg = (exca_getb(&sc->exca[0], EXCA_INTR) & ~EXCA_INTR_IRQ_MASK) | 
	    EXCA_INTR_IRQ_NONE;
	exca_putb(&sc->exca[0], EXCA_INTR, reg);
}

int
cbb_detach(device_t brdev)
{
	struct cbb_softc *sc = device_get_softc(brdev);
	int numdevs;
	device_t *devlist;
	int tmp;
	int error;

	device_get_children(brdev, &devlist, &numdevs);

	error = 0;
	for (tmp = 0; tmp < numdevs; tmp++) {
		if (device_detach(devlist[tmp]) == 0)
			device_delete_child(brdev, devlist[tmp]);
		else
			error++;
	}
	free(devlist, M_TEMP);
	if (error > 0)
		return (ENXIO);

	mtx_lock(&sc->mtx);
	/* 
	 * XXX do we teardown all the ones still registered to guard against
	 * XXX buggy client drivers?
	 */
	bus_teardown_intr(brdev, sc->irq_res, sc->intrhand);
	sc->flags |= CBB_KTHREAD_DONE;
	if (sc->flags & CBB_KTHREAD_RUNNING) {
		cv_broadcast(&sc->cv);
		msleep(sc->event_thread, &sc->mtx, PWAIT, "cbbun", 0);
	}
	mtx_unlock(&sc->mtx);

	bus_release_resource(brdev, SYS_RES_IRQ, 0, sc->irq_res);
	bus_release_resource(brdev, SYS_RES_MEMORY, CBBR_SOCKBASE,
	    sc->base_res);
	mtx_destroy(&sc->mtx);
	cv_destroy(&sc->cv);
	cv_destroy(&sc->powercv);
	return (0);
}

int
cbb_shutdown(device_t brdev)
{
	struct cbb_softc *sc = (struct cbb_softc *)device_get_softc(brdev);
	/* properly reset everything at shutdown */

	PCI_MASK_CONFIG(brdev, CBBR_BRIDGECTRL, |CBBM_BRIDGECTRL_RESET, 2);
	exca_clrb(&sc->exca[0], EXCA_INTR, EXCA_INTR_RESET);

	cbb_set(sc, CBB_SOCKET_MASK, 0);

	cbb_power(brdev, CARD_OFF);

	exca_putb(&sc->exca[0], EXCA_ADDRWIN_ENABLE, 0);
	pci_write_config(brdev, CBBR_MEMBASE0, 0, 4);
	pci_write_config(brdev, CBBR_MEMLIMIT0, 0, 4);
	pci_write_config(brdev, CBBR_MEMBASE1, 0, 4);
	pci_write_config(brdev, CBBR_MEMLIMIT1, 0, 4);
	pci_write_config(brdev, CBBR_IOBASE0, 0, 4);
	pci_write_config(brdev, CBBR_IOLIMIT0, 0, 4);
	pci_write_config(brdev, CBBR_IOBASE1, 0, 4);
	pci_write_config(brdev, CBBR_IOLIMIT1, 0, 4);
	pci_write_config(brdev, PCIR_COMMAND, 0, 2);
	return (0);
}

int
cbb_setup_intr(device_t dev, device_t child, struct resource *irq,
  int flags, driver_intr_t *intr, void *arg, void **cookiep)
{
	struct cbb_intrhand *ih;
	struct cbb_softc *sc = device_get_softc(dev);
	int err;

	/*
	 * Well, this is no longer strictly true.  You can have multiple
	 * FAST ISRs, but can't mix fast and slow, so we have to assume
	 * least common denominator until the base system supports mixing
	 * and matching better.
	 */
	if ((flags & INTR_FAST) != 0)
		return (EINVAL);
	ih = malloc(sizeof(struct cbb_intrhand), M_DEVBUF, M_NOWAIT);
	if (ih == NULL)
		return (ENOMEM);
	*cookiep = ih;
	ih->intr = intr;
	ih->arg = arg;
	ih->sc = sc;
	/*
	 * XXX need to turn on ISA interrupts, if we ever support them, but
	 * XXX for now that's all we need to do.
	 */
	err = BUS_SETUP_INTR(device_get_parent(dev), child, irq, flags,
	    cbb_func_intr, ih, &ih->cookie);
	if (err != 0) {
		free(ih, M_DEVBUF);
		return (err);
	}
	STAILQ_INSERT_TAIL(&sc->intr_handlers, ih, entries);
	cbb_enable_func_intr(sc);
	sc->flags |= CBB_CARD_OK;
	return 0;
}

int
cbb_teardown_intr(device_t dev, device_t child, struct resource *irq,
    void *cookie)
{
	struct cbb_intrhand *ih;
	struct cbb_softc *sc = device_get_softc(dev);
	int err;

	/* XXX Need to do different things for ISA interrupts. */
	ih = (struct cbb_intrhand *) cookie;
	err = BUS_TEARDOWN_INTR(device_get_parent(dev), child, irq,
	    ih->cookie);
	if (err != 0)
		return (err);
	STAILQ_REMOVE(&sc->intr_handlers, ih, cbb_intrhand, entries);
	free(ih, M_DEVBUF);
	return (0);
}


void
cbb_driver_added(device_t brdev, driver_t *driver)
{
	struct cbb_softc *sc = device_get_softc(brdev);
	device_t *devlist;
	device_t dev;
	int tmp;
	int numdevs;
	int wake = 0;

	DEVICE_IDENTIFY(driver, brdev);
	device_get_children(brdev, &devlist, &numdevs);
	for (tmp = 0; tmp < numdevs; tmp++) {
		dev = devlist[tmp];
		if (device_get_state(dev) == DS_NOTPRESENT &&
		    device_probe_and_attach(dev) == 0)
			wake++;
	}
	free(devlist, M_TEMP);

	if (wake > 0) {
		mtx_lock(&sc->mtx);
		cv_signal(&sc->cv);
		mtx_unlock(&sc->mtx);
	}
}

void
cbb_child_detached(device_t brdev, device_t child)
{
	struct cbb_softc *sc = device_get_softc(brdev);

	if (child != sc->cbdev && child != sc->exca[0].pccarddev)
		device_printf(brdev, "Unknown child detached: %s\n",
		    device_get_nameunit(child));
}

/************************************************************************/
/* Kthreads								*/
/************************************************************************/

void
cbb_event_thread(void *arg)
{
	struct cbb_softc *sc = arg;
	uint32_t status;
	int err;
	int not_a_card = 0;

	sc->flags |= CBB_KTHREAD_RUNNING;
	while ((sc->flags & CBB_KTHREAD_DONE) == 0) {
		/*
		 * We take out Giant here because we need it deep,
		 * down in the bowels of the vm system for mapping the
		 * memory we need to read the CIS.  In addition, since
		 * we are adding/deleting devices from the dev tree,
		 * and that code isn't MP safe, we have to hold Giant.
		 */
		mtx_lock(&Giant);
		status = cbb_get(sc, CBB_SOCKET_STATE);
		DPRINTF(("Status is 0x%x\n", status));
		if (!CBB_CARD_PRESENT(status)) {
			not_a_card = 0;		/* We know card type */
			cbb_removal(sc);
		} else if (status & CBB_STATE_NOT_A_CARD) {
			/*
			 * Up to 20 times, try to rescan the card when we
			 * see NOT_A_CARD.
			 */
			if (not_a_card++ < 20) {
				DEVPRINTF((sc->dev,
				    "Not a card bit set, rescanning\n"));
				cbb_setb(sc, CBB_SOCKET_FORCE, CBB_FORCE_CV_TEST);
			} else {
				device_printf(sc->dev,
				    "Can't determine card type\n");
			}
		} else {
			not_a_card = 0;		/* We know card type */
			cbb_insert(sc);
		}
		mtx_unlock(&Giant);

		/*
		 * Wait until it has been 1s since the last time we
		 * get an interrupt.  We handle the rest of the interrupt
		 * at the top of the loop.  Although we clear the bit in the
		 * ISR, we signal sc->cv from the detach path after we've
		 * set the CBB_KTHREAD_DONE bit, so we can't do a simple
		 * 1s sleep here.
		 *
		 * In our ISR, we turn off the card changed interrupt.  Turn
		 * them back on here before we wait for them to happen.  We
		 * turn them on/off so that we can tolerate a large latency
		 * between the time we signal cbb_event_thread and it gets
		 * a chance to run.
		 */
		mtx_lock(&sc->mtx);
		cbb_setb(sc, CBB_SOCKET_MASK, CBB_SOCKET_MASK_CD);
		cv_wait(&sc->cv, &sc->mtx);
		err = 0;
		while (err != EWOULDBLOCK &&
		    (sc->flags & CBB_KTHREAD_DONE) == 0)
			err = cv_timedwait(&sc->cv, &sc->mtx, 1 * hz);
		mtx_unlock(&sc->mtx);
	}
	sc->flags &= ~CBB_KTHREAD_RUNNING;
	kthread_exit(0);
}

/************************************************************************/
/* Insert/removal							*/
/************************************************************************/

static void
cbb_insert(struct cbb_softc *sc)
{
	uint32_t sockevent, sockstate;

	sockevent = cbb_get(sc, CBB_SOCKET_EVENT);
	sockstate = cbb_get(sc, CBB_SOCKET_STATE);

	DEVPRINTF((sc->dev, "card inserted: event=0x%08x, state=%08x\n",
	    sockevent, sockstate));

	if (sockstate & CBB_STATE_R2_CARD) {
		if (sc->exca[0].pccarddev) {
			sc->flags |= CBB_16BIT_CARD;
			exca_insert(&sc->exca[0]);
		} else {
			device_printf(sc->dev,
			    "16-bit card inserted, but no pccard bus.\n");
		}
	} else if (sockstate & CBB_STATE_CB_CARD) {
		if (sc->cbdev != NULL) {
			sc->flags &= ~CBB_16BIT_CARD;
			CARD_ATTACH_CARD(sc->cbdev);
		} else {
			device_printf(sc->dev,
			    "CardBus card inserted, but no cardbus bus.\n");
		}
	} else {
		/*
		 * We should power the card down, and try again a couple of
		 * times if this happens. XXX
		 */
		device_printf(sc->dev, "Unsupported card type detected\n");
	}
}

static void
cbb_removal(struct cbb_softc *sc)
{
	sc->flags &= ~CBB_CARD_OK;
	if (sc->flags & CBB_16BIT_CARD) {
		exca_removal(&sc->exca[0]);
	} else {
		if (sc->cbdev != NULL)
			CARD_DETACH_CARD(sc->cbdev);
	}
	cbb_destroy_res(sc);
}

/************************************************************************/
/* Interrupt Handler							*/
/************************************************************************/

/*
 * Since we touch hardware in the worst case, we don't need to use atomic
 * ops on the CARD_OK tests.  They would save us a trip to the hardware
 * if CARD_OK was recently cleared and the caches haven't updated yet.
 * However, an atomic op costs between 100-200 CPU cycles.  On a 3GHz
 * machine, this is about 33-66ns, whereas a trip the the hardware
 * is about that.  On slower machines, the cost is even higher, so the
 * trip to the hardware is cheaper and achieves the same ends that
 * a fully locked operation would give us.
 *
 * This is a separate routine because we'd have to use locking and/or
 * other synchronization in cbb_intr to do this there.  That would be
 * even more expensive.
 *
 * I need to investigate what this means for a SMP machine with multiple
 * CPUs servicing the ISR when an eject happens.  In the case of a dirty
 * eject, CD glitches and we might read 'card present' from the hardware
 * due to this jitter.  If we assumed that cbb_intr() ran before
 * cbb_func_intr(), we could just check the SOCKET_MASK register and if
 * CD changes were clear there, then we'd know the card was gone.
 */
static void
cbb_func_intr(void *arg)
{
	struct cbb_intrhand *ih = (struct cbb_intrhand *)arg;
	struct cbb_softc *sc = ih->sc;

	/*
	 * Make sure that the card is really there.
	 */
	if ((sc->flags & CBB_CARD_OK) == 0)
		return;
	if (!CBB_CARD_PRESENT(cbb_get(sc, CBB_SOCKET_STATE))) {
		sc->flags &= ~CBB_CARD_OK;
		return;
	}

	/*
	 * nb: don't have to check for giant or not, since that's done
	 * in the ISR dispatch
	 */
	(*ih->intr)(ih->arg);
}

void
cbb_intr(void *arg)
{
	struct cbb_softc *sc = arg;
	uint32_t sockevent;

	sockevent = cbb_get(sc, CBB_SOCKET_EVENT);
	if (sockevent != 0) {
		/* ack the interrupt */
		cbb_set(sc, CBB_SOCKET_EVENT, sockevent);

		/*
		 * If anything has happened to the socket, we assume that
		 * the card is no longer OK, and we shouldn't call its
		 * ISR.  We set CARD_OK as soon as we've attached the
		 * card.  This helps in a noisy eject, which happens
		 * all too often when users are ejecting their PC Cards.
		 *
		 * We use this method in preference to checking to see if
		 * the card is still there because the check suffers from
		 * a race condition in the bouncing case.  Prior versions
		 * of the pccard software used a similar trick and achieved
		 * excellent results.
		 */
		if (sockevent & CBB_SOCKET_EVENT_CD) {
			mtx_lock(&sc->mtx);
			cbb_clrb(sc, CBB_SOCKET_MASK, CBB_SOCKET_MASK_CD);
			sc->flags &= ~CBB_CARD_OK;
			cbb_disable_func_intr(sc);
			cv_signal(&sc->cv);
			mtx_unlock(&sc->mtx);
		}
		/*
		 * If we get a power interrupt, wakeup anybody that might
		 * be waiting for one.
		 */
		if (sockevent & CBB_SOCKET_EVENT_POWER) {
			mtx_lock(&sc->mtx);
			sc->powerintr++;
			cv_signal(&sc->powercv);
			mtx_unlock(&sc->mtx);
		}
	}
	/*
	 * Some chips also require us to read the old ExCA registe for
	 * card status change when we route CSC vis PCI.  This isn't supposed
	 * to be required, but it clears the interrupt state on some chipsets.
	 * Maybe there's a setting that would obviate its need.  Maybe we
	 * should test the status bits and deal with them, but so far we've
	 * not found any machines that don't also give us the socket status
	 * indication above.
	 *
	 * We have to call this unconditionally because some bridges deliver
	 * the even independent of the CBB_SOCKET_EVENT_CD above.
	 */
	exca_getb(&sc->exca[0], EXCA_CSC);
}

/************************************************************************/
/* Generic Power functions						*/
/************************************************************************/

static uint32_t
cbb_detect_voltage(device_t brdev)
{
	struct cbb_softc *sc = device_get_softc(brdev);
	uint32_t psr;
	uint32_t vol = CARD_UKN_CARD;

	psr = cbb_get(sc, CBB_SOCKET_STATE);

	if (psr & CBB_STATE_5VCARD)
		vol |= CARD_5V_CARD;
	if (psr & CBB_STATE_3VCARD)
		vol |= CARD_3V_CARD;
	if (psr & CBB_STATE_XVCARD)
		vol |= CARD_XV_CARD;
	if (psr & CBB_STATE_YVCARD)
		vol |= CARD_YV_CARD;

	return (vol);
}

static uint8_t
cbb_o2micro_power_hack(struct cbb_softc *sc)
{
	uint8_t reg;

	/*
	 * Issue #2: INT# not qualified with IRQ Routing Bit.  An
	 * unexpected PCI INT# may be generated during PC-Card
	 * initialization even with the IRQ Routing Bit Set with some
	 * PC-Cards.
	 *
	 * This is a two part issue.  The first part is that some of
	 * our older controllers have an issue in which the slot's PCI
	 * INT# is NOT qualified by the IRQ routing bit (PCI reg. 3Eh
	 * bit 7).  Regardless of the IRQ routing bit, if NO ISA IRQ
	 * is selected (ExCA register 03h bits 3:0, of the slot, are
	 * cleared) we will generate INT# if IREQ# is asserted.  The
	 * second part is because some PC-Cards prematurally assert
	 * IREQ# before the ExCA registers are fully programmed.  This
	 * in turn asserts INT# because ExCA register 03h bits 3:0
	 * (ISA IRQ Select) are not yet programmed.
	 *
	 * The fix for this issue, which will work for any controller
	 * (old or new), is to set ExCA register 03h bits 3:0 = 0001b
	 * (select IRQ1), of the slot, before turning on slot power.
	 * Selecting IRQ1 will result in INT# NOT being asserted
	 * (because IRQ1 is selected), and IRQ1 won't be asserted
	 * because our controllers don't generate IRQ1.
	 *
	 * Other, non O2Micro controllers will generate irq 1 in some
	 * situations, so we can't do this hack for everybody.  Reports of
	 * keyboard controller's interrupts being suppressed occurred when
	 * we did this.
	 */
	reg = exca_getb(&sc->exca[0], EXCA_INTR);
	exca_putb(&sc->exca[0], EXCA_INTR, (reg & 0xf0) | 1);
	return (reg);
}

/*
 * Restore the damage that cbb_o2micro_power_hack does to EXCA_INTR so
 * we don't have an interrupt storm on power on.  This has the efect of
 * disabling card status change interrupts for the duration of poweron.
 */
static void
cbb_o2micro_power_hack2(struct cbb_softc *sc, uint8_t reg)
{
	exca_putb(&sc->exca[0], EXCA_INTR, reg);
}

int
cbb_power(device_t brdev, int volts)
{
	uint32_t status, sock_ctrl, mask;
	struct cbb_softc *sc = device_get_softc(brdev);
	int cnt, sane;
	int retval = 0;
	int on = 0;
	uint8_t reg = 0;

	sock_ctrl = cbb_get(sc, CBB_SOCKET_CONTROL);

	sock_ctrl &= ~CBB_SOCKET_CTRL_VCCMASK;
	switch (volts & CARD_VCCMASK) {
	case 5:
		sock_ctrl |= CBB_SOCKET_CTRL_VCC_5V;
		on++;
		break;
	case 3:
		sock_ctrl |= CBB_SOCKET_CTRL_VCC_3V;
		on++;
		break;
	case XV:
		sock_ctrl |= CBB_SOCKET_CTRL_VCC_XV;
		on++;
		break;
	case YV:
		sock_ctrl |= CBB_SOCKET_CTRL_VCC_YV;
		on++;
		break;
	case 0:
		break;
	default:
		return (0);			/* power NEVER changed */
	}

	/* VPP == VCC */
	sock_ctrl &= ~CBB_SOCKET_CTRL_VPPMASK;
	sock_ctrl |= ((sock_ctrl >> 4) & 0x07);

	if (cbb_get(sc, CBB_SOCKET_CONTROL) == sock_ctrl)
		return (1); /* no change necessary */
	DEVPRINTF((sc->dev, "cbb_power: %dV\n", volts));
	if (volts != 0 && sc->chipset == CB_O2MICRO)
		reg = cbb_o2micro_power_hack(sc);

	/*
	 * We have to mask the card change detect interrupt while we're
	 * messing with the power.  It is allowed to bounce while we're
	 * messing with power as things settle down.  In addition, we mask off
	 * the card's function interrupt by routing it via the ISA bus.  This
	 * bit generally only affects 16bit cards.  Some bridges allow one to
	 * set another bit to have it also affect 32bit cards.  Since 32bit
	 * cards are required to be better behaved, we don't bother to get
	 * into those bridge specific features.
	 */
	mask = cbb_get(sc, CBB_SOCKET_MASK);
	mask |= CBB_SOCKET_MASK_POWER;
	mask &= ~CBB_SOCKET_MASK_CD;
	cbb_set(sc, CBB_SOCKET_MASK, mask);
	PCI_MASK_CONFIG(brdev, CBBR_BRIDGECTRL,
	    |CBBM_BRIDGECTRL_INTR_IREQ_ISA_EN, 2);
	cbb_set(sc, CBB_SOCKET_CONTROL, sock_ctrl);
	if (on) {
		mtx_lock(&sc->mtx);
		cnt = sc->powerintr;
		sane = 200;
		while (!(cbb_get(sc, CBB_SOCKET_STATE) & CBB_STATE_POWER_CYCLE) &&
		    cnt == sc->powerintr && sane-- > 0)
			cv_timedwait(&sc->powercv, &sc->mtx, hz / 10);
		mtx_unlock(&sc->mtx);
		if (sane <= 0)
			device_printf(sc->dev, "power timeout, doom?\n");
	}

	/*
	 * After the power is good, we can turn off the power interrupt.
	 * However, the PC Card standard says that we must delay turning the
	 * CD bit back on for a bit to allow for bouncyness on power down
	 * (recall that we don't wait above for a power down, since we don't
	 * get an interrupt for that).  We're called either from the suspend
	 * code in which case we don't want to turn card change on again, or
	 * we're called from the card insertion code, in which case the cbb
	 * thread will turn it on for us before it waits to be woken by a
	 * change event.
	 */
	cbb_clrb(sc, CBB_SOCKET_MASK, CBB_SOCKET_MASK_POWER);
	status = cbb_get(sc, CBB_SOCKET_STATE);
	if (on) {
		if ((status & CBB_STATE_POWER_CYCLE) == 0)
			device_printf(sc->dev, "Power not on?\n");
	}
	if (status & CBB_STATE_BAD_VCC_REQ) {
		device_printf(sc->dev, "Bad Vcc requested\n");	
		/* XXX Do we want to do something to mitigate things here? */
		goto done;
	}
	PCI_MASK_CONFIG(brdev, CBBR_BRIDGECTRL,
	    & ~CBBM_BRIDGECTRL_INTR_IREQ_ISA_EN, 2);
	retval = 1;
done:;
	if (volts != 0 && sc->chipset == CB_O2MICRO)
		cbb_o2micro_power_hack2(sc, reg);
	return (retval);
}

static int
cbb_current_voltage(device_t brdev)
{
	struct cbb_softc *sc = device_get_softc(brdev);
	uint32_t ctrl;
	
	ctrl = cbb_get(sc, CBB_SOCKET_CONTROL);
	switch (ctrl & CBB_SOCKET_CTRL_VCCMASK) {
	case CBB_SOCKET_CTRL_VCC_5V:
		return CARD_5V_CARD;
	case CBB_SOCKET_CTRL_VCC_3V:
		return CARD_3V_CARD;
	case CBB_SOCKET_CTRL_VCC_XV:
		return CARD_XV_CARD;
	case CBB_SOCKET_CTRL_VCC_YV:
		return CARD_YV_CARD;
	}
	return 0;
}

/*
 * detect the voltage for the card, and set it.  Since the power
 * used is the square of the voltage, lower voltages is a big win
 * and what Windows does (and what Microsoft prefers).  The MS paper
 * also talks about preferring the CIS entry as well, but that has
 * to be done elsewhere.  We also optimize power sequencing here
 * and don't change things if we're already powered up at a supported
 * voltage.
 *
 * In addition, we power up with OE disabled.  We'll set it later
 * in the power up sequence.
 */
static int
cbb_do_power(device_t brdev)
{
	struct cbb_softc *sc = device_get_softc(brdev);
	uint32_t voltage, curpwr;
	uint32_t status;

	/* Don't enable OE (output enable) until power stable */
	exca_clrb(&sc->exca[0], EXCA_PWRCTL, EXCA_PWRCTL_OE);

	voltage = cbb_detect_voltage(brdev);
	curpwr = cbb_current_voltage(brdev);
	status = cbb_get(sc, CBB_SOCKET_STATE);
	if ((status & CBB_STATE_POWER_CYCLE) && (voltage & curpwr))
		return 0;
	/* Prefer lowest voltage supported */
	cbb_power(brdev, CARD_OFF);
	if (voltage & CARD_YV_CARD)
		cbb_power(brdev, CARD_VCC(YV));
	else if (voltage & CARD_XV_CARD)
		cbb_power(brdev, CARD_VCC(XV));
	else if (voltage & CARD_3V_CARD)
		cbb_power(brdev, CARD_VCC(3));
	else if (voltage & CARD_5V_CARD)
		cbb_power(brdev, CARD_VCC(5));
	else {
		device_printf(brdev, "Unknown card voltage\n");
		return (ENXIO);
	}
	return (0);
}

/************************************************************************/
/* CardBus power functions						*/
/************************************************************************/

static void
cbb_cardbus_reset(device_t brdev)
{
	struct cbb_softc *sc = device_get_softc(brdev);
	int delay;

	/*
	 * 20ms is necessary for most bridges.  For some reason, the Ricoh
	 * RF5C47x bridges need 400ms.
	 */
	delay = sc->chipset == CB_RF5C47X ? 400 : 20;

	PCI_MASK_CONFIG(brdev, CBBR_BRIDGECTRL, |CBBM_BRIDGECTRL_RESET, 2);

	tsleep(sc, PZERO, "cbbP3", hz * delay / 1000);

	/* If a card exists, unreset it! */
	if (CBB_CARD_PRESENT(cbb_get(sc, CBB_SOCKET_STATE))) {
		PCI_MASK_CONFIG(brdev, CBBR_BRIDGECTRL,
		    &~CBBM_BRIDGECTRL_RESET, 2);
		tsleep(sc, PZERO, "cbbP3", hz * delay / 1000);
	}
}

static int
cbb_cardbus_power_enable_socket(device_t brdev, device_t child)
{
	struct cbb_softc *sc = device_get_softc(brdev);
	int err;

	if (!CBB_CARD_PRESENT(cbb_get(sc, CBB_SOCKET_STATE)))
		return (ENODEV);

	err = cbb_do_power(brdev);
	if (err)
		return (err);
	cbb_cardbus_reset(brdev);
	return (0);
}

static void
cbb_cardbus_power_disable_socket(device_t brdev, device_t child)
{
	cbb_power(brdev, CARD_OFF);
	cbb_cardbus_reset(brdev);
}

/************************************************************************/
/* CardBus Resource							*/
/************************************************************************/

static int
cbb_cardbus_io_open(device_t brdev, int win, uint32_t start, uint32_t end)
{
	int basereg;
	int limitreg;

	if ((win < 0) || (win > 1)) {
		DEVPRINTF((brdev,
		    "cbb_cardbus_io_open: window out of range %d\n", win));
		return (EINVAL);
	}

	basereg = win * 8 + CBBR_IOBASE0;
	limitreg = win * 8 + CBBR_IOLIMIT0;

	pci_write_config(brdev, basereg, start, 4);
	pci_write_config(brdev, limitreg, end, 4);
	return (0);
}

static int
cbb_cardbus_mem_open(device_t brdev, int win, uint32_t start, uint32_t end)
{
	int basereg;
	int limitreg;

	if ((win < 0) || (win > 1)) {
		DEVPRINTF((brdev,
		    "cbb_cardbus_mem_open: window out of range %d\n", win));
		return (EINVAL);
	}

	basereg = win*8 + CBBR_MEMBASE0;
	limitreg = win*8 + CBBR_MEMLIMIT0;

	pci_write_config(brdev, basereg, start, 4);
	pci_write_config(brdev, limitreg, end, 4);
	return (0);
}

/*
 * XXX The following function belongs in the pci bus layer.
 */
static void
cbb_cardbus_auto_open(struct cbb_softc *sc, int type)
{
	uint32_t starts[2];
	uint32_t ends[2];
	struct cbb_reslist *rle;
	int align;
	int prefetchable[2];
	uint32_t reg;

	starts[0] = starts[1] = 0xffffffff;
	ends[0] = ends[1] = 0;

	if (type == SYS_RES_MEMORY)
		align = CBB_MEMALIGN;
	else if (type == SYS_RES_IOPORT)
		align = CBB_IOALIGN;
	else
		align = 1;

	/*
	 * This looks somewhat bogus, and doesn't seem to really respect
	 * alignment.  The alignment stuff is happening too late (it
	 * should happen at allocation time, not activation time) and
	 * this code looks generally to be too complex for the purpose
	 * it surves.
	 */
	SLIST_FOREACH(rle, &sc->rl, link) {
		if (rle->type != type)
			;
		else if (rle->res == NULL) {
			device_printf(sc->dev, "WARNING: Resource not reserved?  "
			    "(type=%d, addr=%lx)\n",
			    rle->type, rman_get_start(rle->res));
		} else if (!(rman_get_flags(rle->res) & RF_ACTIVE)) {
			/* XXX */
		} else if (starts[0] == 0xffffffff) {
			starts[0] = rman_get_start(rle->res);
			ends[0] = rman_get_end(rle->res);
			prefetchable[0] =
			    rman_get_flags(rle->res) & RF_PREFETCHABLE;
		} else if (rman_get_end(rle->res) > ends[0] &&
		    rman_get_start(rle->res) - ends[0] <
		    CBB_AUTO_OPEN_SMALLHOLE && prefetchable[0] ==
		    (rman_get_flags(rle->res) & RF_PREFETCHABLE)) {
			ends[0] = rman_get_end(rle->res);
		} else if (rman_get_start(rle->res) < starts[0] &&
		    starts[0] - rman_get_end(rle->res) <
		    CBB_AUTO_OPEN_SMALLHOLE && prefetchable[0] ==
		    (rman_get_flags(rle->res) & RF_PREFETCHABLE)) {
			starts[0] = rman_get_start(rle->res);
		} else if (starts[1] == 0xffffffff) {
			starts[1] = rman_get_start(rle->res);
			ends[1] = rman_get_end(rle->res);
			prefetchable[1] =
			    rman_get_flags(rle->res) & RF_PREFETCHABLE;
		} else if (rman_get_end(rle->res) > ends[1] &&
		    rman_get_start(rle->res) - ends[1] <
		    CBB_AUTO_OPEN_SMALLHOLE && prefetchable[1] ==
		    (rman_get_flags(rle->res) & RF_PREFETCHABLE)) {
			ends[1] = rman_get_end(rle->res);
		} else if (rman_get_start(rle->res) < starts[1] &&
		    starts[1] - rman_get_end(rle->res) <
		    CBB_AUTO_OPEN_SMALLHOLE && prefetchable[1] ==
		    (rman_get_flags(rle->res) & RF_PREFETCHABLE)) {
			starts[1] = rman_get_start(rle->res);
		} else {
			uint32_t diffs[2];
			int win;

			diffs[0] = diffs[1] = 0xffffffff;
			if (rman_get_start(rle->res) > ends[0])
				diffs[0] = rman_get_start(rle->res) - ends[0];
			else if (rman_get_end(rle->res) < starts[0])
				diffs[0] = starts[0] - rman_get_end(rle->res);
			if (rman_get_start(rle->res) > ends[1])
				diffs[1] = rman_get_start(rle->res) - ends[1];
			else if (rman_get_end(rle->res) < starts[1])
				diffs[1] = starts[1] - rman_get_end(rle->res);

			win = (diffs[0] <= diffs[1])?0:1;
			if (rman_get_start(rle->res) > ends[win])
				ends[win] = rman_get_end(rle->res);
			else if (rman_get_end(rle->res) < starts[win])
				starts[win] = rman_get_start(rle->res);
			if (!(rman_get_flags(rle->res) & RF_PREFETCHABLE))
				prefetchable[win] = 0;
		}

		if (starts[0] != 0xffffffff)
			starts[0] -= starts[0] % align;
		if (starts[1] != 0xffffffff)
			starts[1] -= starts[1] % align;
		if (ends[0] % align != 0)
			ends[0] += align - ends[0] % align - 1;
		if (ends[1] % align != 0)
			ends[1] += align - ends[1] % align - 1;
	}

	if (type == SYS_RES_MEMORY) {
		cbb_cardbus_mem_open(sc->dev, 0, starts[0], ends[0]);
		cbb_cardbus_mem_open(sc->dev, 1, starts[1], ends[1]);
		reg = pci_read_config(sc->dev, CBBR_BRIDGECTRL, 2);
		reg &= ~(CBBM_BRIDGECTRL_PREFETCH_0|
		    CBBM_BRIDGECTRL_PREFETCH_1);
		reg |= (prefetchable[0]?CBBM_BRIDGECTRL_PREFETCH_0:0)|
		    (prefetchable[1]?CBBM_BRIDGECTRL_PREFETCH_1:0);
		pci_write_config(sc->dev, CBBR_BRIDGECTRL, reg, 2);
	} else if (type == SYS_RES_IOPORT) {
		cbb_cardbus_io_open(sc->dev, 0, starts[0], ends[0]);
		cbb_cardbus_io_open(sc->dev, 1, starts[1], ends[1]);
	}
}

static int
cbb_cardbus_activate_resource(device_t brdev, device_t child, int type,
    int rid, struct resource *res)
{
	int ret;

	ret = BUS_ACTIVATE_RESOURCE(device_get_parent(brdev), child,
	    type, rid, res);
	if (ret != 0)
		return (ret);
	cbb_cardbus_auto_open(device_get_softc(brdev), type);
	return (0);
}

static int
cbb_cardbus_deactivate_resource(device_t brdev, device_t child, int type,
    int rid, struct resource *res)
{
	int ret;

	ret = BUS_DEACTIVATE_RESOURCE(device_get_parent(brdev), child,
	    type, rid, res);
	if (ret != 0)
		return (ret);
	cbb_cardbus_auto_open(device_get_softc(brdev), type);
	return (0);
}

static struct resource *
cbb_cardbus_alloc_resource(device_t brdev, device_t child, int type,
    int *rid, u_long start, u_long end, u_long count, u_int flags)
{
	struct cbb_softc *sc = device_get_softc(brdev);
	int tmp;
	struct resource *res;
	u_long align;

	switch (type) {
	case SYS_RES_IRQ:
		tmp = rman_get_start(sc->irq_res);
		if (start > tmp || end < tmp || count != 1) {
			device_printf(child, "requested interrupt %ld-%ld,"
			    "count = %ld not supported by cbb\n",
			    start, end, count);
			return (NULL);
		}
		start = end = tmp;
		flags |= RF_SHAREABLE;
		break;
	case SYS_RES_IOPORT:
		if (start <= cbb_start_32_io)
			start = cbb_start_32_io;
		if (end < start)
			end = start;
		break;
	case SYS_RES_MEMORY:
		if (start <= cbb_start_mem)
			start = cbb_start_mem;
		if (end < start)
			end = start;
		if (count < CBB_MEMALIGN)
			align = CBB_MEMALIGN;
		else
			align = count;
		if (align > (1 << RF_ALIGNMENT(flags)))
			flags = (flags & ~RF_ALIGNMENT_MASK) | 
			    rman_make_alignment_flags(align);
		break;
	}

	res = BUS_ALLOC_RESOURCE(device_get_parent(brdev), child, type, rid,
	    start, end, count, flags & ~RF_ACTIVE);
	if (res == NULL) {
		printf("cbb alloc res fail\n");
		return (NULL);
	}
	cbb_insert_res(sc, res, type, *rid);
	if (flags & RF_ACTIVE)
		if (bus_activate_resource(child, type, *rid, res) != 0) {
			bus_release_resource(child, type, *rid, res);
			return (NULL);
		}

	return (res);
}

static int
cbb_cardbus_release_resource(device_t brdev, device_t child, int type,
    int rid, struct resource *res)
{
	struct cbb_softc *sc = device_get_softc(brdev);
	int error;

	if (rman_get_flags(res) & RF_ACTIVE) {
		error = bus_deactivate_resource(child, type, rid, res);
		if (error != 0)
			return (error);
	}
	cbb_remove_res(sc, res);
	return (BUS_RELEASE_RESOURCE(device_get_parent(brdev), child,
	    type, rid, res));
}

/************************************************************************/
/* PC Card Power Functions						*/
/************************************************************************/

static int
cbb_pcic_power_enable_socket(device_t brdev, device_t child)
{
	struct cbb_softc *sc = device_get_softc(brdev);
	int err;

	DPRINTF(("cbb_pcic_socket_enable:\n"));

	/* power down/up the socket to reset */
	err = cbb_do_power(brdev);
	if (err)
		return (err);
	exca_reset(&sc->exca[0], child);

	return (0);
}

static void
cbb_pcic_power_disable_socket(device_t brdev, device_t child)
{
	struct cbb_softc *sc = device_get_softc(brdev);

	DPRINTF(("cbb_pcic_socket_disable\n"));

	/* reset signal asserting... */
	exca_clrb(&sc->exca[0], EXCA_INTR, EXCA_INTR_RESET);
	tsleep(sc, PZERO, "cbbP1", hz / 100);

	/* power down the socket */
	exca_clrb(&sc->exca[0], EXCA_PWRCTL, EXCA_PWRCTL_OE);
	cbb_power(brdev, CARD_OFF);

	/* wait 300ms until power fails (Tpf). */
	tsleep(sc, PZERO, "cbbP1", hz * 300 / 1000);
}

/************************************************************************/
/* POWER methods							*/
/************************************************************************/

int
cbb_power_enable_socket(device_t brdev, device_t child)
{
	struct cbb_softc *sc = device_get_softc(brdev);

	if (sc->flags & CBB_16BIT_CARD)
		return (cbb_pcic_power_enable_socket(brdev, child));
	else
		return (cbb_cardbus_power_enable_socket(brdev, child));
}

void
cbb_power_disable_socket(device_t brdev, device_t child)
{
	struct cbb_softc *sc = device_get_softc(brdev);
	if (sc->flags & CBB_16BIT_CARD)
		cbb_pcic_power_disable_socket(brdev, child);
	else
		cbb_cardbus_power_disable_socket(brdev, child);
}

static int
cbb_pcic_activate_resource(device_t brdev, device_t child, int type, int rid,
    struct resource *res)
{
	struct cbb_softc *sc = device_get_softc(brdev);
	return (exca_activate_resource(&sc->exca[0], child, type, rid, res));
}

static int
cbb_pcic_deactivate_resource(device_t brdev, device_t child, int type,
    int rid, struct resource *res)
{
	struct cbb_softc *sc = device_get_softc(brdev);
	return (exca_deactivate_resource(&sc->exca[0], child, type, rid, res));
}

static struct resource *
cbb_pcic_alloc_resource(device_t brdev, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct resource *res = NULL;
	struct cbb_softc *sc = device_get_softc(brdev);
	int align;
	int tmp;

	switch (type) {
	case SYS_RES_MEMORY:
		if (start < cbb_start_mem)
			start = cbb_start_mem;
		if (end < start)
			end = start;
		if (count < CBB_MEMALIGN)
			align = CBB_MEMALIGN;
		else
			align = count;
		if (align > (1 << RF_ALIGNMENT(flags)))
			flags = (flags & ~RF_ALIGNMENT_MASK) | 
			    rman_make_alignment_flags(align);
		break;
	case SYS_RES_IOPORT:
		if (start < cbb_start_16_io)
			start = cbb_start_16_io;
		if (end < start)
			end = start;
		break;
	case SYS_RES_IRQ:
		tmp = rman_get_start(sc->irq_res);
		if (start > tmp || end < tmp || count != 1) {
			device_printf(child, "requested interrupt %ld-%ld,"
			    "count = %ld not supported by cbb\n",
			    start, end, count);
			return (NULL);
		}
		flags |= RF_SHAREABLE;
		start = end = rman_get_start(sc->irq_res);
		break;
	}
	res = BUS_ALLOC_RESOURCE(device_get_parent(brdev), child, type, rid,
	    start, end, count, flags & ~RF_ACTIVE);
	if (res == NULL)
		return (NULL);
	cbb_insert_res(sc, res, type, *rid);
	if (flags & RF_ACTIVE) {
		if (bus_activate_resource(child, type, *rid, res) != 0) {
			bus_release_resource(child, type, *rid, res);
			return (NULL);
		}
	}

	return (res);
}

static int
cbb_pcic_release_resource(device_t brdev, device_t child, int type,
    int rid, struct resource *res)
{
	struct cbb_softc *sc = device_get_softc(brdev);
	int error;

	if (rman_get_flags(res) & RF_ACTIVE) {
		error = bus_deactivate_resource(child, type, rid, res);
		if (error != 0)
			return (error);
	}
	cbb_remove_res(sc, res);
	return (BUS_RELEASE_RESOURCE(device_get_parent(brdev), child,
	    type, rid, res));
}

/************************************************************************/
/* PC Card methods							*/
/************************************************************************/

int
cbb_pcic_set_res_flags(device_t brdev, device_t child, int type, int rid,
    uint32_t flags)
{
	struct cbb_softc *sc = device_get_softc(brdev);
	struct resource *res;

	if (type != SYS_RES_MEMORY)
		return (EINVAL);
	res = cbb_find_res(sc, type, rid);
	if (res == NULL) {
		device_printf(brdev,
		    "set_res_flags: specified rid not found\n");
		return (ENOENT);
	}
	return (exca_mem_set_flags(&sc->exca[0], res, flags));
}

int
cbb_pcic_set_memory_offset(device_t brdev, device_t child, int rid,
    uint32_t cardaddr, uint32_t *deltap)
{
	struct cbb_softc *sc = device_get_softc(brdev);
	struct resource *res;

	res = cbb_find_res(sc, SYS_RES_MEMORY, rid);
	if (res == NULL) {
		device_printf(brdev,
		    "set_memory_offset: specified rid not found\n");
		return (ENOENT);
	}
	return (exca_mem_set_offset(&sc->exca[0], res, cardaddr, deltap));
}

/************************************************************************/
/* BUS Methods								*/
/************************************************************************/


int
cbb_activate_resource(device_t brdev, device_t child, int type, int rid,
    struct resource *r)
{
	struct cbb_softc *sc = device_get_softc(brdev);

	if (sc->flags & CBB_16BIT_CARD)
		return (cbb_pcic_activate_resource(brdev, child, type, rid, r));
	else
		return (cbb_cardbus_activate_resource(brdev, child, type, rid,
		    r));
}

int
cbb_deactivate_resource(device_t brdev, device_t child, int type,
    int rid, struct resource *r)
{
	struct cbb_softc *sc = device_get_softc(brdev);

	if (sc->flags & CBB_16BIT_CARD)
		return (cbb_pcic_deactivate_resource(brdev, child, type,
		    rid, r));
	else
		return (cbb_cardbus_deactivate_resource(brdev, child, type,
		    rid, r));
}

struct resource *
cbb_alloc_resource(device_t brdev, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct cbb_softc *sc = device_get_softc(brdev);

	if (sc->flags & CBB_16BIT_CARD)
		return (cbb_pcic_alloc_resource(brdev, child, type, rid,
		    start, end, count, flags));
	else
		return (cbb_cardbus_alloc_resource(brdev, child, type, rid,
		    start, end, count, flags));
}

int
cbb_release_resource(device_t brdev, device_t child, int type, int rid,
    struct resource *r)
{
	struct cbb_softc *sc = device_get_softc(brdev);

	if (sc->flags & CBB_16BIT_CARD)
		return (cbb_pcic_release_resource(brdev, child, type,
		    rid, r));
	else
		return (cbb_cardbus_release_resource(brdev, child, type,
		    rid, r));
}

int
cbb_read_ivar(device_t brdev, device_t child, int which, uintptr_t *result)
{
	struct cbb_softc *sc = device_get_softc(brdev);

	switch (which) {
	case PCIB_IVAR_BUS:
		*result = sc->secbus;
		return (0);
	}
	return (ENOENT);
}

int
cbb_write_ivar(device_t brdev, device_t child, int which, uintptr_t value)
{
	struct cbb_softc *sc = device_get_softc(brdev);

	switch (which) {
	case PCIB_IVAR_BUS:
		sc->secbus = value;
		break;
	}
	return (ENOENT);
}

/************************************************************************/
/* PCI compat methods							*/
/************************************************************************/

int
cbb_maxslots(device_t brdev)
{
	return (0);
}

uint32_t
cbb_read_config(device_t brdev, int b, int s, int f, int reg, int width)
{
	uint32_t rv;

	/*
	 * Pass through to the next ppb up the chain (i.e. our grandparent).
	 */
	rv = PCIB_READ_CONFIG(device_get_parent(device_get_parent(brdev)),
	    b, s, f, reg, width);
	return (rv);
}

void
cbb_write_config(device_t brdev, int b, int s, int f, int reg, uint32_t val,
    int width)
{
	/*
	 * Pass through to the next ppb up the chain (i.e. our grandparent).
	 */
	PCIB_WRITE_CONFIG(device_get_parent(device_get_parent(brdev)),
	    b, s, f, reg, val, width);
}

int
cbb_suspend(device_t self)
{
	int			error = 0;
	struct cbb_softc	*sc = device_get_softc(self);

	cbb_set(sc, CBB_SOCKET_MASK, 0);	/* Quiet hardware */
	bus_teardown_intr(self, sc->irq_res, sc->intrhand);
	sc->flags &= ~CBB_CARD_OK;		/* Card is bogus now */
	error = bus_generic_suspend(self);
	return (error);
}

int
cbb_resume(device_t self)
{
	int	error = 0;
	struct cbb_softc *sc = (struct cbb_softc *)device_get_softc(self);
	uint32_t tmp;

	/*
	 * Some BIOSes will not save the BARs for the pci chips, so we
	 * must do it ourselves.  If the BAR is reset to 0 for an I/O
	 * device, it will read back as 0x1, so no explicit test for
	 * memory devices are needed.
	 *
	 * Note: The PCI bus code should do this automatically for us on
	 * suspend/resume, but until it does, we have to cope.
	 */
	pci_write_config(self, CBBR_SOCKBASE, rman_get_start(sc->base_res), 4);
	DEVPRINTF((self, "PCI Memory allocated: %08lx\n",
	    rman_get_start(sc->base_res)));

	sc->chipinit(sc);

	/* reset interrupt -- Do we really need to do this? */
	tmp = cbb_get(sc, CBB_SOCKET_EVENT);
	cbb_set(sc, CBB_SOCKET_EVENT, tmp);

	/* re-establish the interrupt. */
	if (bus_setup_intr(self, sc->irq_res, INTR_TYPE_AV | INTR_MPSAFE,
	    cbb_intr, sc, &sc->intrhand)) {
		device_printf(self, "couldn't re-establish interrupt");
		bus_release_resource(self, SYS_RES_IRQ, 0, sc->irq_res);
		bus_release_resource(self, SYS_RES_MEMORY, CBBR_SOCKBASE,
		    sc->base_res);
		sc->irq_res = NULL;
		sc->base_res = NULL;
		return (ENOMEM);
	}

	/* CSC Interrupt: Card detect interrupt on */
	cbb_setb(sc, CBB_SOCKET_MASK, CBB_SOCKET_MASK_CD);

	/* Signal the thread to wakeup. */
	mtx_lock(&sc->mtx);
	cv_signal(&sc->cv);
	mtx_unlock(&sc->mtx);

	error = bus_generic_resume(self);

	return (error);
}

int
cbb_child_present(device_t self)
{
	struct cbb_softc *sc = (struct cbb_softc *)device_get_softc(self);
	uint32_t sockstate;

	sockstate = cbb_get(sc, CBB_SOCKET_STATE);
	return (CBB_CARD_PRESENT(sockstate) &&
	  (sc->flags & CBB_CARD_OK) == CBB_CARD_OK);
}
