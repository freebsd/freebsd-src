/* $FreeBSD$ */

/*
 * Copyright (c) 2002 M Warner Losh.  All rights reserved.
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
 *
 * This software may be derived from NetBSD i82365.c and other files with
 * the following copyright:
 *
 * Copyright (c) 1997 Marc Horowitz.  All rights reserved.
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
 *	This product includes software developed by Marc Horowitz.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/module.h>
#include <sys/conf.h>

#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/pccard/pccardreg.h>
#include <dev/pccard/pccardvar.h>

#include <dev/exca/excareg.h>
#include <dev/exca/excavar.h>

#ifdef EXCA_DEBUG
#define DEVPRINTF(dev, fmt, args...)	device_printf((dev), (fmt), ## args)
#define DPRINTF(fmt, args...)		printf(fmt, ## args)
#else
#define DEVPRINTF(dev, fmt, args...)
#define DPRINTF(fmt, args...)
#endif

/* memory */

#define	EXCA_MEMINFO(NUM) {						\
	EXCA_SYSMEM_ADDR ## NUM ## _START_LSB,				\
	EXCA_SYSMEM_ADDR ## NUM ## _START_MSB,				\
	EXCA_SYSMEM_ADDR ## NUM ## _STOP_LSB,				\
	EXCA_SYSMEM_ADDR ## NUM ## _STOP_MSB,				\
	EXCA_SYSMEM_ADDR ## NUM ## _WIN,				\
	EXCA_CARDMEM_ADDR ## NUM ## _LSB,				\
	EXCA_CARDMEM_ADDR ## NUM ## _MSB,				\
	EXCA_ADDRWIN_ENABLE_MEM ## NUM,					\
}

static struct mem_map_index_st {
	int	sysmem_start_lsb;
	int	sysmem_start_msb;
	int	sysmem_stop_lsb;
	int	sysmem_stop_msb;
	int	sysmem_win;
	int	cardmem_lsb;
	int	cardmem_msb;
	int	memenable;
} mem_map_index[] = {
	EXCA_MEMINFO(0),
	EXCA_MEMINFO(1),
	EXCA_MEMINFO(2),
	EXCA_MEMINFO(3),
	EXCA_MEMINFO(4)
};
#undef	EXCA_MEMINFO

/*
 * Helper function.  This will map the requested memory slot.  We setup the
 * map before we call this function.  This is used to initially force the
 * mapping, as well as later restore the mapping after it has been destroyed
 * in some fashion (due to a power event typically).
 */
static void
exca_do_mem_map(struct exca_softc *sc, int win)
{
	struct mem_map_index_st *map;
	struct pccard_mem_handle *mem;
	
	map = &mem_map_index[win];
	mem = &sc->mem[win];
	exca_write(sc, map->sysmem_start_lsb,
	    (mem->addr >> EXCA_SYSMEM_ADDRX_SHIFT) & 0xff);
	exca_write(sc, map->sysmem_start_msb,
	    ((mem->addr >> (EXCA_SYSMEM_ADDRX_SHIFT + 8)) &
	    EXCA_SYSMEM_ADDRX_START_MSB_ADDR_MASK) | 0x80);

	exca_write(sc, map->sysmem_stop_lsb,
	    ((mem->addr + mem->realsize - 1) >>
	    EXCA_SYSMEM_ADDRX_SHIFT) & 0xff);
	exca_write(sc, map->sysmem_stop_msb,
	    (((mem->addr + mem->realsize - 1) >>
	    (EXCA_SYSMEM_ADDRX_SHIFT + 8)) &
	    EXCA_SYSMEM_ADDRX_STOP_MSB_ADDR_MASK) |
	    EXCA_SYSMEM_ADDRX_STOP_MSB_WAIT2);

	exca_write(sc, map->sysmem_win,
	    (mem->addr >> EXCA_MEMREG_WIN_SHIFT) & 0xff);

	exca_write(sc, map->cardmem_lsb,
	    (mem->offset >> EXCA_CARDMEM_ADDRX_SHIFT) & 0xff);
	exca_write(sc, map->cardmem_msb,
	    ((mem->offset >> (EXCA_CARDMEM_ADDRX_SHIFT + 8)) &
	    EXCA_CARDMEM_ADDRX_MSB_ADDR_MASK) |
	    ((mem->kind == PCCARD_MEM_ATTR) ?
	    EXCA_CARDMEM_ADDRX_MSB_REGACTIVE_ATTR : 0));

	exca_setb(sc, EXCA_ADDRWIN_ENABLE, EXCA_ADDRWIN_ENABLE_MEMCS16 |
	    map->memenable);

	DELAY(100);
#ifdef EXCA_DEBUG
	{
		int r1, r2, r3, r4, r5, r6, r7;
		r1 = exca_read(sc, map->sysmem_start_msb);
		r2 = exca_read(sc, map->sysmem_start_lsb);
		r3 = exca_read(sc, map->sysmem_stop_msb);
		r4 = exca_read(sc, map->sysmem_stop_lsb);
		r5 = exca_read(sc, map->cardmem_msb);
		r6 = exca_read(sc, map->cardmem_lsb);
		r7 = exca_read(sc, map->sysmem_win);
		printf("exca_do_mem_map window %d: %02x%02x %02x%02x "
		    "%02x%02x %02x (%08x+%08x.%08x*%08lx)\n",
		    win, r1, r2, r3, r4, r5, r6, r7,
		    mem->addr, mem->size, mem->realsize,
		    mem->offset);
	}
#endif
}

/*
 * public interface to map a resource.  kind is the type of memory to
 * map (either common or attribute).  Memory created via this interface
 * starts out at card address 0.  Since the only way to set this is
 * to set it on a struct resource after it has been mapped, we're safe
 * in maping this assumption.  Note that resources can be remapped using
 * exca_do_mem_map so that's how the card address can be set later.
 */
int
exca_mem_map(struct exca_softc *sc, int kind, struct resource *res)
{
	int win;

	for (win = 0; win < EXCA_MEM_WINS; win++) {
		if ((sc->memalloc & (1 << win)) == 0) {
			sc->memalloc |= (1 << win);
			break;
		}
	}
	if (win >= EXCA_MEM_WINS)
		return (1);
	if (((rman_get_start(res) >> EXCA_CARDMEM_ADDRX_SHIFT) & 0xff) != 0 &&
	    (sc->flags & EXCA_HAS_MEMREG_WIN) == 0) {
		device_printf(sc->dev, "Does not support mapping above 24M.");
		return (1);
	}

	sc->mem[win].cardaddr = 0;
	sc->mem[win].memt = rman_get_bustag(res);
	sc->mem[win].memh = rman_get_bushandle(res);
	sc->mem[win].addr = rman_get_start(res);
	sc->mem[win].size = rman_get_end(res) - sc->mem[win].addr + 1;
	sc->mem[win].realsize = sc->mem[win].size + EXCA_MEM_PAGESIZE - 1;
	sc->mem[win].realsize = sc->mem[win].realsize -
	    (sc->mem[win].realsize % EXCA_MEM_PAGESIZE);
	sc->mem[win].offset = (long)(sc->mem[win].addr);
	sc->mem[win].kind = kind;
	DPRINTF("exca_mem_map window %d bus %x+%x+%lx card addr %x\n",
	    win, sc->mem[win].addr, sc->mem[win].size,
	    sc->mem[win].offset, sc->mem[win].cardaddr);
	exca_do_mem_map(sc, win);

	return (0);
}

/*
 * Private helper function.  This turns off a given memory map that is in
 * use.  We do this by just clearing the enable bit in the pcic.  If we needed
 * to make memory unmapping/mapping pairs faster, we would have to store
 * more state information about the pcic and then use that to intelligently
 * to the map/unmap.  However, since we don't do that sort of thing often
 * (generally just at configure time), it isn't a case worth optimizing.
 */
static void
exca_mem_unmap(struct exca_softc *sc, int window)
{
	if (window < 0 || window >= EXCA_MEM_WINS)
		panic("exca_mem_unmap: window out of range");

	exca_clrb(sc, EXCA_ADDRWIN_ENABLE, mem_map_index[window].memenable);
	sc->memalloc &= ~(1 << window);
}

/*
 * Find the map that we're using to hold the resoruce.  This works well
 * so long as the client drivers don't do silly things like map the same
 * area mutliple times, or map both common and attribute memory at the
 * same time.  This latter restriction is a bug.  We likely should just
 * store a pointer to the res in the mem[x] data structure.
 */
static int
exca_mem_findmap(struct exca_softc *sc, struct resource *res)
{
	int win;

	for (win = 0; win < EXCA_MEM_WINS; win++) {
		if (sc->mem[win].memt == rman_get_bustag(res) &&
		    sc->mem[win].addr == rman_get_start(res) &&
		    sc->mem[win].size == rman_get_size(res))
			return (win);
	}
	return (-1);
}

/*
 * Set the memory flag.  This means that we are setting if the memory
 * is coming from attribute memory or from common memory on the card.
 * CIS entries are generally in attribute memory (although they can
 * reside in common memory).  Generally, this is the only use for attribute
 * memory.  However, some cards require their drivers to dance in both
 * common and/or attribute memory and this interface (and setting the
 * offset interface) exist for such cards.
 */
int
exca_mem_set_flags(struct exca_softc *sc, struct resource *res, uint32_t flags)
{
	int win;

	win = exca_mem_findmap(sc, res);
	if (win < 0) {
		device_printf(sc->dev,
		    "set_res_flags: specified resource not active\n");
		return (ENOENT);
	}

	sc->mem[win].kind = flags;
	exca_do_mem_map(sc, win);
	return (0);
}

/*
 * Given a resource, go ahead and unmap it if we can find it in the
 * resrouce list that's used.
 */
int
exca_mem_unmap_res(struct exca_softc *sc, struct resource *res)
{
	int win;

	win = exca_mem_findmap(sc, res);
	if (win < 0)
		return (ENOENT);
	exca_mem_unmap(sc, win);
	return (0);
}
	
/*
 * Set the offset of the memory.  We use this for reading the CIS and
 * frobbing the pccard's pccard registers (POR, etc).  Some drivers
 * need to access this functionality as well, since they have receive
 * buffers defined in the attribute memory.  Thankfully, these cards
 * are few and fare between.  Some cards also have common memory that
 * is large and only map a small portion of it at a time (but these cards
 * are rare, the more common case being to have just a small amount
 * of common memory that the driver needs to bcopy data from in order to
 * get at it.
 */
int
exca_mem_set_offset(struct exca_softc *sc, struct resource *res,
    uint32_t cardaddr, uint32_t *deltap)
{
	int win;
	uint32_t delta;

	win = exca_mem_findmap(sc, res);
	if (win < 0) {
		device_printf(sc->dev,
		    "set_memory_offset: specified resource not active\n");
		return (ENOENT);
	}
	sc->mem[win].cardaddr = cardaddr;
	delta = cardaddr % EXCA_MEM_PAGESIZE;
	if (deltap)
		*deltap = delta;
	cardaddr -= delta;
	sc->mem[win].realsize = sc->mem[win].size + delta +
	    EXCA_MEM_PAGESIZE - 1;
	sc->mem[win].realsize = sc->mem[win].realsize -
	    (sc->mem[win].realsize % EXCA_MEM_PAGESIZE);
	sc->mem[win].offset = cardaddr - sc->mem[win].addr;
	exca_do_mem_map(sc, win);
	return (0);
}
			

/* I/O */

#define	EXCA_IOINFO(NUM) {						\
	EXCA_IOADDR ## NUM ## _START_LSB,				\
	EXCA_IOADDR ## NUM ## _START_MSB,				\
	EXCA_IOADDR ## NUM ## _STOP_LSB,				\
	EXCA_IOADDR ## NUM ## _STOP_MSB,				\
	EXCA_ADDRWIN_ENABLE_IO ## NUM,					\
	EXCA_IOCTL_IO ## NUM ## _WAITSTATE				\
	| EXCA_IOCTL_IO ## NUM ## _ZEROWAIT				\
	| EXCA_IOCTL_IO ## NUM ## _IOCS16SRC_MASK			\
	| EXCA_IOCTL_IO ## NUM ## _DATASIZE_MASK,			\
	{								\
		EXCA_IOCTL_IO ## NUM ## _IOCS16SRC_CARD,		\
		EXCA_IOCTL_IO ## NUM ## _IOCS16SRC_DATASIZE		\
		| EXCA_IOCTL_IO ## NUM ## _DATASIZE_8BIT,		\
		EXCA_IOCTL_IO ## NUM ## _IOCS16SRC_DATASIZE		\
		| EXCA_IOCTL_IO ## NUM ## _DATASIZE_16BIT,		\
	}								\
}

static struct io_map_index_st {
	int	start_lsb;
	int	start_msb;
	int	stop_lsb;
	int	stop_msb;
	int	ioenable;
	int	ioctlmask;
	int	ioctlbits[3]; /* indexed by PCCARD_WIDTH_* */
} io_map_index[] = {
	EXCA_IOINFO(0),
	EXCA_IOINFO(1),
};
#undef	EXCA_IOINFO

static void
exca_do_io_map(struct exca_softc *sc, int win)
{
	struct io_map_index_st *map;

	struct pccard_io_handle *io;

	map = &io_map_index[win];
	io = &sc->io[win];
	exca_write(sc, map->start_lsb, io->addr & 0xff);
	exca_write(sc, map->start_msb, (io->addr >> 8) & 0xff);

	exca_write(sc, map->stop_lsb, (io->addr + io->size - 1) & 0xff);
	exca_write(sc, map->stop_msb, ((io->addr + io->size - 1) >> 8) & 0xff);

	exca_clrb(sc, EXCA_IOCTL, map->ioctlmask);
	exca_setb(sc, EXCA_IOCTL, map->ioctlbits[io->width]);

	exca_setb(sc, EXCA_ADDRWIN_ENABLE, map->ioenable);
#ifdef EXCA_DEBUG
	{
		int r1, r2, r3, r4;
		r1 = exca_read(sc, map->start_msb);
		r2 = exca_read(sc, map->start_lsb);
		r3 = exca_read(sc, map->stop_msb);
		r4 = exca_read(sc, map->stop_lsb);
		DPRINTF("exca_do_io_map window %d: %02x%02x %02x%02x "
		    "(%08x+%08x)\n", win, r1, r2, r3, r4,
		    io->addr, io->size);
	}
#endif
}

int
exca_io_map(struct exca_softc *sc, int width, struct resource *r)
{
	int win;
#ifdef EXCA_DEBUG
	static char *width_names[] = { "auto", "io8", "io16"};
#endif
	for (win=0; win < EXCA_IO_WINS; win++) {
		if ((sc->ioalloc & (1 << win)) == 0) {
			sc->ioalloc |= (1 << win);
			break;
		}
	}
	if (win >= EXCA_IO_WINS)
		return (1);

	sc->io[win].iot = rman_get_bustag(r);
	sc->io[win].ioh = rman_get_bushandle(r);
	sc->io[win].addr = rman_get_start(r);
	sc->io[win].size = rman_get_end(r) - sc->io[win].addr + 1;
	sc->io[win].flags = 0;
	sc->io[win].width = width;
	DPRINTF("exca_io_map window %d %s port %x+%x\n",
	    win, width_names[width], sc->io[win].addr,
	    sc->io[win].size);
	exca_do_io_map(sc, win);

	return (0);
}

static void
exca_io_unmap(struct exca_softc *sc, int window)
{
	if (window >= EXCA_IO_WINS)
		panic("exca_io_unmap: window out of range");

	exca_clrb(sc, EXCA_ADDRWIN_ENABLE, io_map_index[window].ioenable);

	sc->ioalloc &= ~(1 << window);

	sc->io[window].iot = 0;
	sc->io[window].ioh = 0;
	sc->io[window].addr = 0;
	sc->io[window].size = 0;
	sc->io[window].flags = 0;
	sc->io[window].width = 0;
}

static int
exca_io_findmap(struct exca_softc *sc, struct resource *res)
{
	int win;

	for (win = 0; win < EXCA_IO_WINS; win++) {
		if (sc->io[win].iot == rman_get_bustag(res) &&
		    sc->io[win].addr == rman_get_start(res) &&
		    sc->io[win].size == rman_get_size(res))
			return (win);
	}
	return (-1);
}


int
exca_io_unmap_res(struct exca_softc *sc, struct resource *res)
{
	int win;

	win = exca_io_findmap(sc, res);
	if (win < 0)
		return (ENOENT);
	exca_io_unmap(sc, win);
	return (0);
}

/* Misc */

/*
 * If interrupts are enabled, then we should be able to just wait for
 * an interrupt routine to wake us up.  Busy waiting shouldn't be
 * necessary.  Sadly, not all legacy ISA cards support an interrupt
 * for the busy state transitions, at least according to their datasheets, 
 * so we busy wait a while here..
 */
static void
exca_wait_ready(struct exca_softc *sc)
{
	int i;
	DEVPRINTF(sc->dev, "exca_wait_ready: status 0x%02x\n",
	    exca_read(sc, EXCA_IF_STATUS));
	for (i = 0; i < 10000; i++) {
		if (exca_read(sc, EXCA_IF_STATUS) & EXCA_IF_STATUS_READY)
			return;
		DELAY(500);
	}
	device_printf(sc->dev, "ready never happened, status = %02x\n",
	    exca_read(sc, EXCA_IF_STATUS));
}

/*
 * Reset the card.  Ideally, we'd do a lot of this via interrupts.
 * However, many PC Cards will deassert the ready signal.  This means
 * that they are asserting an interrupt.  This makes it hard to 
 * do anything but a busy wait here.  One could argue that these
 * such cards are broken, or that the bridge that allows this sort
 * of interrupt through isn't quite what you'd want (and may be a standards
 * violation).  However, such arguing would leave a huge class of pc cards
 * and bridges out of reach for use in the system.
 *
 * Maybe I should reevaluate the above based on the power bug I fixed
 * in OLDCARD.
 */
void
exca_reset(struct exca_softc *sc, device_t child)
{
	int cardtype;
	int win;

	/* enable socket i/o */
	exca_setb(sc, EXCA_PWRCTL, EXCA_PWRCTL_OE);

	exca_write(sc, EXCA_INTR, EXCA_INTR_ENABLE);
	/* hold reset for 30ms */
	DELAY(30*1000);
	/* clear the reset flag */
	exca_setb(sc, EXCA_INTR, EXCA_INTR_RESET);
	/* wait 20ms as per pc card standard (r2.01) section 4.3.6 */
	DELAY(20*1000);

	exca_wait_ready(sc);

	/* disable all address windows */
	exca_write(sc, EXCA_ADDRWIN_ENABLE, 0);

	CARD_GET_TYPE(child, &cardtype);
	exca_setb(sc, EXCA_INTR, (cardtype == PCCARD_IFTYPE_IO) ?
	    EXCA_INTR_CARDTYPE_IO : EXCA_INTR_CARDTYPE_MEM);
	DEVPRINTF(sc->dev, "card type is %s\n",
	    (cardtype == PCCARD_IFTYPE_IO) ? "io" : "mem");

	/* reinstall all the memory and io mappings */
	for (win = 0; win < EXCA_MEM_WINS; ++win)
		if (sc->memalloc & (1 << win))
			exca_do_mem_map(sc, win);
	for (win = 0; win < EXCA_IO_WINS; ++win)
		if (sc->ioalloc & (1 << win))
			exca_do_io_map(sc, win);
}

/*
 * Initialize the exca_softc data structure for the first time.
 */
void
exca_init(struct exca_softc *sc, device_t dev, 
    bus_space_tag_t bst, bus_space_handle_t bsh, uint32_t offset)
{
	sc->dev = dev;
	sc->memalloc = 0;
	sc->ioalloc = 0;
	sc->bst = bst;
	sc->bsh = bsh;
	sc->offset = offset;
	sc->flags = 0;
}

/*
 * Probe the expected slots.  We maybe should set the ID for each of these
 * slots too while we're at it.  But maybe that belongs to a separate
 * function.
 *
 * Callers must charantee that there are at least EXCA_NSLOTS (4) in
 * the array that they pass the address of the first element in the
 * "exca" parameter.
 */
int
exca_probe_slots(device_t dev, struct exca_softc *exca)
{
	int rid;
	struct resource *res;
	int err;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int i;

	err = ENXIO;
	rid = 0;
	res = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0, EXCA_IOSIZE,
	    RF_ACTIVE);
	if (res == NULL)
		return (ENXIO);
	iot = rman_get_bustag(res);
	ioh = rman_get_bushandle(res);
	for (i = 0; i < EXCA_NSLOTS; i++) {
		exca_init(&exca[i], dev, iot, ioh, i * EXCA_SOCKET_SIZE);
		if (exca_is_pcic(&exca[i])) {
			err = 0;
			exca[i].flags |= EXCA_SOCKET_PRESENT;
		}
	}
	bus_release_resource(dev, SYS_RES_IOPORT, rid, res);
	return (err);
}

int
exca_is_pcic(struct exca_softc *sc)
{
	/* XXX */
	return (0);
}

static int exca_modevent(module_t mod, int cmd, void *arg)
{
	return 0;
}
DEV_MODULE(exca, exca_modevent, NULL);
MODULE_VERSION(exca, 1);
