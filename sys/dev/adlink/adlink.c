/*-
 * Copyright (c) 2003 Poul-Henning Kamp
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
 * 3. The names of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifdef _KERNEL
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/conf.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <pci_if.h>
#include <vm/vm.h>
#include <vm/pmap.h>

#endif /* _KERNEL */

#include <sys/ioccom.h>

struct wave {
	int			index;
	int			period;
	int			offset;
	int			length;
	int			avg;
	off_t			mapvir;
	int			flags;

	int			npages;
	void			**virtual;
};

#define ADLINK_SETWAVE		_IOWR('A', 232, struct wave)
#define ADLINK_GETWAVE		_IOWR('A', 233, struct wave)

#ifdef _KERNEL

#define INTPERPAGE (PAGE_SIZE / sizeof(int))
#define I16PERPAGE (PAGE_SIZE / sizeof(int16_t))

/*
 * Sample rate
 */
#define SPS	1250000

/*
 * We sample one channel (= 16 bits) at 1.25 msps giving 2.5Mbyte/sec,
 * 100 pages will give us about 1/6 second buffering.
 */
#define NRING	100

/*
 * How many waves are we willing to entertain
 */
#define NWAVE	25

struct info {
	int			nring;
	off_t			o_ring;
	
	int			ngri;
	int			ppgri;
	off_t			o_gri;
};

struct softc {
	device_t		device;
	void			*intrhand;
	struct resource		*r0, *r1, *ri;
	bus_space_tag_t		t0, t1;
	bus_space_handle_t	h0, h1;
	dev_t			dev;
	off_t			mapvir;

	struct proc		*procp;

	struct info		*info;

	struct wave		*wave[NWAVE];

	int			idx;
	void			*ring[NRING];
	vm_paddr_t		pring[NRING];
	int			stat[NRING];

	uint64_t		cnt;

	u_char			flags[I16PERPAGE];
};

static void
adlink_wave(struct softc *sc, struct wave *wp, int16_t *sp)
{
	int f, i, k, m, *ip;

	f = 0;
	for (i = 0; i < I16PERPAGE; ) {
		k = (sc->cnt - wp->offset + i) % wp->period;
		if (k >= wp->length) {
			i += wp->period - k;
			sp += wp->period - k;
			continue;
		}
		m = k % INTPERPAGE;
		ip = (int *)(wp->virtual[k / INTPERPAGE]) + m;
		while (m < INTPERPAGE && i < I16PERPAGE && k < wp->length) {
			if (sc->flags[i] >= wp->index)
				*ip += (*sp * 8 - *ip) >> wp->avg;
			if (wp->flags & 1)
				sc->flags[i] = wp->index;
			sp++;
			ip++;
			m++;
			i++;
			k++;
		}
	}
}

static void
adlink_tickle(struct softc *sc)
{

	wakeup(sc);
	tsleep(&sc->ring, PUSER | PCATCH, "tickle", 1);
}

static int
adlink_new_wave(struct softc *sc, int index, int period, int offset, int length, int avg, int flags)
{
	struct wave *wp;
	int l, i;
	void **oldvir, **newvir;

	if (index < 0 || index >= NWAVE)
		return (EINVAL);
	wp = sc->wave[index];
	if (wp == NULL) {
		adlink_tickle(sc);
		wp = malloc(sizeof *wp, M_DEVBUF, M_WAITOK | M_ZERO);
	}
	l = howmany(length, INTPERPAGE);
	/* Setting a high average here to neuter the realtime bits */
	wp->avg = 31;
	if (wp->npages < l) {
		oldvir = wp->virtual;
		adlink_tickle(sc);
		newvir = malloc(sizeof(void *) * l, M_DEVBUF, M_WAITOK | M_ZERO);
		if (wp->npages > 0) {
			adlink_tickle(sc);
			bcopy(oldvir, newvir, wp->npages * sizeof(void *));
		}
		for (i = wp->npages; i < l; i++) {
			adlink_tickle(sc);
			newvir[i] = malloc(PAGE_SIZE, M_DEVBUF, M_WAITOK);
		}
		wp->virtual = newvir;
		wp->npages = l;
		wp->mapvir = sc->mapvir;
		sc->mapvir += l * PAGE_SIZE;
	} else {
		oldvir = NULL;
	}
	wp->index = index;
	wp->period = period;
	wp->offset = offset;
	wp->length = length;
	wp->flags = flags;
	
	for (i = 0; i < l; i++) {
		adlink_tickle(sc);
		bzero(wp->virtual[i], PAGE_SIZE);
	}
	wp->avg = avg;
	sc->wave[index] = wp;
	printf("Wave[%d] {period %d, offset %d, length %d, avg %d, flags %x}\n",
	    wp->index, wp->period, wp->offset, wp->length, wp->avg, wp->flags);
	free(oldvir, M_DEVBUF);
	return (0);
}

static void
adlink_loran(void *arg)
{
	struct softc *sc;
	int idx, i;

	sc = arg;
	idx = 0;
	for (;;) {
		while (sc->stat[idx] == 0)
			msleep(sc, NULL, PRIBIO, "loran", 1);
		memset(sc->flags, NWAVE, sizeof sc->flags);
		for (i = 0; i < NWAVE; i++) {
			if (sc->wave[i] != NULL)
				adlink_wave(sc, sc->wave[i], sc->ring[idx]);
		}
		sc->cnt += I16PERPAGE;
		sc->stat[idx] = 0;
		idx++;
		idx %= NRING;
	}
	kthread_exit(0);
}

static int
adlink_open(dev_t dev, int oflags, int devtype, struct thread *td)
{
	static int once;
	struct softc *sc;
	int i, error;
	uint32_t u;

	if (once)
		return (0);
	once = 1;

	sc = dev->si_drv1;
	sc->info = malloc(PAGE_SIZE, M_DEVBUF, M_ZERO | M_WAITOK);
	sc->info->nring = NRING;

	sc->info->o_ring = PAGE_SIZE;
	for (i = 0; i < NRING; i++) {
		sc->ring[i] = malloc(PAGE_SIZE, M_DEVBUF, M_ZERO | M_WAITOK);
		sc->pring[i] = vtophys(sc->ring[i]);
	}

	error = adlink_new_wave(sc, NWAVE - 1, SPS, 0, SPS, 7, 0);
	if (error)
		return (error);

	error = kthread_create(adlink_loran, sc, &sc->procp,
	    0, 0, "adlink%d", device_get_unit(sc->device));
	if (error)
		return (error);

	/* Enable interrupts on write complete */
	bus_space_write_4(sc->t0, sc->h0, 0x38, 0x00004000);

	/* Sample CH0 only */
	bus_space_write_4(sc->t1, sc->h1, 0x00, 1);

	/* Divide clock by ten */
	bus_space_write_4(sc->t1, sc->h1, 0x04, 4);

	/* Software trigger mode: software */
	bus_space_write_4(sc->t1, sc->h1, 0x08, 0);

	/* Trigger level zero */
	bus_space_write_4(sc->t1, sc->h1, 0x0c, 0);

	/* Trigger source CH0 (not used) */
	bus_space_write_4(sc->t1, sc->h1, 0x10, 0);

	/* Fifo control/status: flush */
	bus_space_write_4(sc->t1, sc->h1, 0x18, 3);

	/* Clock source: external sine */
	bus_space_write_4(sc->t1, sc->h1, 0x20, 2);

	/* Set up Write DMA */
	bus_space_write_4(sc->t0, sc->h0, 0x24, sc->pring[i]);
	bus_space_write_4(sc->t0, sc->h0, 0x28, PAGE_SIZE);
	u = bus_space_read_4(sc->t0, sc->h0, 0x3c);
	bus_space_write_4(sc->t0, sc->h0, 0x3c, u | 0x00000600);

	/* Acquisition Enable Register: go! */
	bus_space_write_4(sc->t1, sc->h1, 0x1c, 1);
	return (0);
}

static int
adlink_ioctl(dev_t dev, u_long cmd, caddr_t data, int fflag, struct thread *td)
{
	struct softc *sc;
	struct wave *wp;
	int i, error;
	
	sc = dev->si_drv1;
	wp = (struct wave *)data;
	i = wp->index;
	if (i < 0 || i >= NWAVE)
		return (EINVAL);
	if (cmd == ADLINK_GETWAVE) {
		if (sc->wave[i] == NULL)
			return (ENOENT);
		bcopy(sc->wave[i], wp, sizeof(*wp));
		return (0);
	}
	if (cmd == ADLINK_SETWAVE) {
		error = adlink_new_wave(sc,
			i,
			wp->period,
			wp->offset,
			wp->length,
			wp->avg,
			wp->flags);
		if (error)
			return (error);
		bcopy(sc->wave[i], wp, sizeof(*wp));
		return (0);
	}
	return (ENOIOCTL);
}

static int
adlink_mmap(dev_t dev, vm_offset_t offset, vm_paddr_t *paddr, int nprot)
{
	struct softc *sc;
	struct wave *wp;
	int i, j;

	sc = dev->si_drv1;
	if (nprot != VM_PROT_READ)
		return (-1);
	for (i = 0; i < NWAVE; i++) {
		if (sc->wave[i] == NULL)
			continue;
		wp = sc->wave[i];
		if (offset < wp->mapvir)
			continue;
		j = (offset - wp->mapvir) / PAGE_SIZE;
		if (j >= wp->npages)
			continue;
		*paddr = vtophys(wp->virtual[j]);
		return (0);
	}
	return (-1);
}

static void
adlink_intr(void *arg)
{
	struct softc *sc;
	uint32_t u;
	int i, j;

	sc = arg;
	u = bus_space_read_4(sc->t0, sc->h0, 0x38);
	if (!(u & 0x00800000))
		return;
	bus_space_write_4(sc->t0, sc->h0, 0x38, u | 0x003f4000);

	j = sc->idx;
	sc->stat[j] = 1;
	i = (j + 1) % NRING;
	sc->idx = i;
	u = bus_space_read_4(sc->t1, sc->h1, 0x18);
	if (u & 1) {
		printf("adlink FIFO overrun\n");
		return;
	}
	bus_space_write_4(sc->t0, sc->h0, 0x24, sc->pring[i]);
	bus_space_write_4(sc->t0, sc->h0, 0x28, PAGE_SIZE);
	wakeup(sc);
	if (sc->stat[i]) {
		printf("adlink page busy\n");
	}
}

static struct cdevsw adlink_cdevsw = {
	.d_open =	adlink_open,
	.d_close =	nullclose,
	.d_ioctl =	adlink_ioctl,
	.d_mmap =	adlink_mmap,
	.d_name =	"adlink",
};

static devclass_t adlink_devclass;

static int
adlink_probe(device_t self)
{

	if (pci_get_devid(self) != 0x80da10e8)
		return (ENXIO);
	device_set_desc(self, "Adlink PCI-9812 4 ch 12 bit 20 msps");
	return (0);
}

static int
adlink_attach(device_t self)
{
	struct softc *sc;
	int rid, i;

	sc = device_get_softc(self);
	bzero(sc, sizeof *sc);
	sc->device = self;

	/*
	 * This is the PCI mapped registers of the AMCC 9535 "matchmaker"
	 * chip.
	 */
	rid = 0x10;
	sc->r0 = bus_alloc_resource(self, SYS_RES_IOPORT, &rid,
	    0, ~0, 1, RF_ACTIVE);
	if (sc->r0 == NULL)
		return(ENODEV);
	sc->t0 = rman_get_bustag(sc->r0);
	sc->h0 = rman_get_bushandle(sc->r0);
	printf("Res0 %x %x\n", sc->t0, sc->h0);

	/*
	 * This is the PCI mapped registers of the ADC hardware, they
	 * are described in the manual which comes with the card.
	 */
	rid = 0x14;
	sc->r1 =  bus_alloc_resource(self, SYS_RES_IOPORT, &rid, 
            0, ~0, 1, RF_ACTIVE);
	if (sc->r1 == NULL)
		return(ENODEV);
	sc->t1 = rman_get_bustag(sc->r1);
	sc->h1 = rman_get_bushandle(sc->r1);
	printf("Res1 %x %x\n", sc->t1, sc->h1);

	rid = 0x0;
	sc->ri =  bus_alloc_resource(self, SYS_RES_IRQ, &rid, 
            0, ~0, 1, RF_ACTIVE | RF_SHAREABLE);
	if (sc->ri == NULL)
		return (ENODEV);

	i = bus_setup_intr(self, sc->ri, INTR_MPSAFE | INTR_TYPE_MISC | INTR_FAST,
	    adlink_intr, sc, &sc->intrhand);
	if (i) {
		printf("adlink: Couldn't get FAST intr\n");
		i = bus_setup_intr(self, sc->ri, INTR_TYPE_MISC,
		    adlink_intr, sc, &sc->intrhand);
	}

	if (i)
		return (ENODEV);

	sc->dev = make_dev(&adlink_cdevsw, device_get_unit(self),
	    UID_ROOT, GID_WHEEL, 0444, "adlink%d", device_get_unit(self));
	sc->dev->si_drv1 = sc;

	return (0);
}

static device_method_t adlink_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		adlink_probe),
	DEVMETHOD(device_attach,	adlink_attach),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	{0, 0}
};
 
static driver_t adlink_driver = {
	"adlink",
	adlink_methods,
	sizeof(struct softc)
};

DRIVER_MODULE(adlink, pci, adlink_driver, adlink_devclass, 0, 0);
#endif /* _KERNEL */
