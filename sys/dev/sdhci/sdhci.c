/*-
 * Copyright (c) 2008 Alexander Motin <mav@FreeBSD.org>
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/resource.h>
#include <sys/rman.h>
#include <sys/taskqueue.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/stdarg.h>

#include <dev/mmc/bridge.h>
#include <dev/mmc/mmcreg.h>
#include <dev/mmc/mmcbrvar.h>

#include "mmcbr_if.h"
#include "sdhci.h"

#define DMA_BLOCK_SIZE	4096
#define DMA_BOUNDARY	0	/* DMA reload every 4K */

/* Controller doesn't honor resets unless we touch the clock register */
#define SDHCI_QUIRK_CLOCK_BEFORE_RESET			(1<<0)
/* Controller really supports DMA */
#define SDHCI_QUIRK_FORCE_DMA				(1<<1)
/* Controller has unusable DMA engine */
#define SDHCI_QUIRK_BROKEN_DMA				(1<<2)
/* Controller doesn't like to be reset when there is no card inserted. */
#define SDHCI_QUIRK_NO_CARD_NO_RESET			(1<<3)
/* Controller has flaky internal state so reset it on each ios change */
#define SDHCI_QUIRK_RESET_ON_IOS			(1<<4)
/* Controller can only DMA chunk sizes that are a multiple of 32 bits */
#define SDHCI_QUIRK_32BIT_DMA_SIZE			(1<<5)
/* Controller needs to be reset after each request to stay stable */
#define SDHCI_QUIRK_RESET_AFTER_REQUEST			(1<<6)
/* Controller has an off-by-one issue with timeout value */
#define SDHCI_QUIRK_INCR_TIMEOUT_CONTROL		(1<<7)
/* Controller has broken read timings */
#define SDHCI_QUIRK_BROKEN_TIMINGS			(1<<8)

static const struct sdhci_device {
	uint32_t	model;
	uint16_t	subvendor;
	char		*desc;
	u_int		quirks;
} sdhci_devices[] = {
	{ 0x08221180, 	0xffff,	"RICOH R5C822 SD",
	    SDHCI_QUIRK_FORCE_DMA },
	{ 0x8034104c, 	0xffff, "TI XX21/XX11 SD",
	    SDHCI_QUIRK_FORCE_DMA },
	{ 0x05501524, 	0xffff, "ENE CB712 SD",
	    SDHCI_QUIRK_BROKEN_TIMINGS },
	{ 0x05511524, 	0xffff, "ENE CB712 SD 2",
	    SDHCI_QUIRK_BROKEN_TIMINGS },
	{ 0x07501524, 	0xffff, "ENE CB714 SD",
	    SDHCI_QUIRK_RESET_ON_IOS |
	    SDHCI_QUIRK_BROKEN_TIMINGS },
	{ 0x07511524, 	0xffff, "ENE CB714 SD 2",
	    SDHCI_QUIRK_RESET_ON_IOS |
	    SDHCI_QUIRK_BROKEN_TIMINGS },
	{ 0x410111ab, 	0xffff, "Marvell CaFe SD",
	    SDHCI_QUIRK_INCR_TIMEOUT_CONTROL },
	{ 0x2381197B, 	0xffff,	"JMicron JMB38X SD",
	    SDHCI_QUIRK_32BIT_DMA_SIZE |
	    SDHCI_QUIRK_RESET_AFTER_REQUEST },
	{ 0,		0xffff,	NULL,
	    0 }
};

struct sdhci_softc;

struct sdhci_slot {
	struct sdhci_softc	*sc;
	device_t	dev;		/* Slot device */
	u_char		num;		/* Slot number */
	u_char		opt;		/* Slot options */
#define SDHCI_HAVE_DMA		1
	uint32_t	max_clk;	/* Max possible freq */
	uint32_t	timeout_clk;	/* Timeout freq */
	struct resource	*mem_res;	/* Memory resource */
	int		mem_rid;
	bus_dma_tag_t 	dmatag;
	bus_dmamap_t 	dmamap;
	u_char		*dmamem;
	bus_addr_t	paddr;		/* DMA buffer address */
	struct task	card_task;	/* Card presence check task */
	struct callout	card_callout;	/* Card insert delay callout */
	struct mmc_host host;		/* Host parameters */
	struct mmc_request *req;	/* Current request */
	struct mmc_command *curcmd;	/* Current command of current request */
	
	uint32_t	intmask;	/* Current interrupt mask */
	uint32_t	clock;		/* Current clock freq. */
	size_t		offset;		/* Data buffer offset */
	uint8_t		hostctrl;	/* Current host control register */
	u_char		power;		/* Current power */
	u_char		bus_busy;	/* Bus busy status */
	u_char		cmd_done;	/* CMD command part done flag */
	u_char		data_done;	/* DAT command part done flag */
	u_char		flags;		/* Request execution flags */
#define CMD_STARTED		1
#define STOP_STARTED		2
#define SDHCI_USE_DMA		4	/* Use DMA for this req. */
	struct mtx	mtx;		/* Slot mutex */
};

struct sdhci_softc {
	device_t	dev;		/* Controller device */
	u_int		quirks;		/* Chip specific quirks */
	struct resource *irq_res;	/* IRQ resource */
	int 		irq_rid;
	void 		*intrhand;	/* Interrupt handle */

	int		num_slots;	/* Number of slots on this controller */
	struct sdhci_slot slots[6];
};

static inline uint8_t
RD1(struct sdhci_slot *slot, bus_size_t off)
{
	bus_barrier(slot->mem_res, 0, 0xFF,
	    BUS_SPACE_BARRIER_READ | BUS_SPACE_BARRIER_WRITE);
	return bus_read_1(slot->mem_res, off);
}

static inline void
WR1(struct sdhci_slot *slot, bus_size_t off, uint8_t val)
{
	bus_barrier(slot->mem_res, 0, 0xFF,
	    BUS_SPACE_BARRIER_READ | BUS_SPACE_BARRIER_WRITE);
	bus_write_1(slot->mem_res, off, val);
}

static inline uint16_t
RD2(struct sdhci_slot *slot, bus_size_t off)
{
	bus_barrier(slot->mem_res, 0, 0xFF,
	    BUS_SPACE_BARRIER_READ | BUS_SPACE_BARRIER_WRITE);
	return bus_read_2(slot->mem_res, off);
}

static inline void
WR2(struct sdhci_slot *slot, bus_size_t off, uint16_t val)
{
	bus_barrier(slot->mem_res, 0, 0xFF,
	    BUS_SPACE_BARRIER_READ | BUS_SPACE_BARRIER_WRITE);
	bus_write_2(slot->mem_res, off, val);
}

static inline uint32_t
RD4(struct sdhci_slot *slot, bus_size_t off)
{
	bus_barrier(slot->mem_res, 0, 0xFF,
	    BUS_SPACE_BARRIER_READ | BUS_SPACE_BARRIER_WRITE);
	return bus_read_4(slot->mem_res, off);
}

static inline void
WR4(struct sdhci_slot *slot, bus_size_t off, uint32_t val)
{
	bus_barrier(slot->mem_res, 0, 0xFF,
	    BUS_SPACE_BARRIER_READ | BUS_SPACE_BARRIER_WRITE);
	bus_write_4(slot->mem_res, off, val);
}

/* bus entry points */
static int sdhci_probe(device_t dev);
static int sdhci_attach(device_t dev);
static int sdhci_detach(device_t dev);
static void sdhci_intr(void *);

static void sdhci_set_clock(struct sdhci_slot *slot, uint32_t clock);
static void sdhci_start(struct sdhci_slot *slot);
static void sdhci_start_data(struct sdhci_slot *slot, struct mmc_data *data);

static void sdhci_card_task(void *, int);

/* helper routines */
#define SDHCI_LOCK(_slot)		mtx_lock(&(_slot)->mtx)
#define	SDHCI_UNLOCK(_slot)		mtx_unlock(&(_slot)->mtx)
#define SDHCI_LOCK_INIT(_slot) \
	mtx_init(&_slot->mtx, "SD slot mtx", "sdhci", MTX_DEF)
#define SDHCI_LOCK_DESTROY(_slot)	mtx_destroy(&_slot->mtx);
#define SDHCI_ASSERT_LOCKED(_slot)	mtx_assert(&_slot->mtx, MA_OWNED);
#define SDHCI_ASSERT_UNLOCKED(_slot)	mtx_assert(&_slot->mtx, MA_NOTOWNED);

static int
slot_printf(struct sdhci_slot *slot, const char * fmt, ...)
{
	va_list ap;
	int retval;

    	retval = printf("%s-slot%d: ",
	    device_get_nameunit(slot->sc->dev), slot->num);

	va_start(ap, fmt);
	retval += vprintf(fmt, ap);
	va_end(ap);
	return (retval);
}

static void
sdhci_getaddr(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{
	if (error != 0) {
		printf("getaddr: error %d\n", error);
		return;
	}
	*(bus_addr_t *)arg = segs[0].ds_addr;
}

static void
sdhci_dumpregs(struct sdhci_slot *slot)
{
	slot_printf(slot,
	    "============== REGISTER DUMP ==============\n");

	slot_printf(slot, "Sys addr: 0x%08x | Version:  0x%08x\n",
	    RD4(slot, SDHCI_DMA_ADDRESS), RD2(slot, SDHCI_HOST_VERSION));
	slot_printf(slot, "Blk size: 0x%08x | Blk cnt:  0x%08x\n",
	    RD2(slot, SDHCI_BLOCK_SIZE), RD2(slot, SDHCI_BLOCK_COUNT));
	slot_printf(slot, "Argument: 0x%08x | Trn mode: 0x%08x\n",
	    RD4(slot, SDHCI_ARGUMENT), RD2(slot, SDHCI_TRANSFER_MODE));
	slot_printf(slot, "Present:  0x%08x | Host ctl: 0x%08x\n",
	    RD4(slot, SDHCI_PRESENT_STATE), RD1(slot, SDHCI_HOST_CONTROL));
	slot_printf(slot, "Power:    0x%08x | Blk gap:  0x%08x\n",
	    RD1(slot, SDHCI_POWER_CONTROL), RD1(slot, SDHCI_BLOCK_GAP_CONTROL));
	slot_printf(slot, "Wake-up:  0x%08x | Clock:    0x%08x\n",
	    RD1(slot, SDHCI_WAKE_UP_CONTROL), RD2(slot, SDHCI_CLOCK_CONTROL));
	slot_printf(slot, "Timeout:  0x%08x | Int stat: 0x%08x\n",
	    RD1(slot, SDHCI_TIMEOUT_CONTROL), RD4(slot, SDHCI_INT_STATUS));
	slot_printf(slot, "Int enab: 0x%08x | Sig enab: 0x%08x\n",
	    RD4(slot, SDHCI_INT_ENABLE), RD4(slot, SDHCI_SIGNAL_ENABLE));
	slot_printf(slot, "AC12 err: 0x%08x | Slot int: 0x%08x\n",
	    RD2(slot, SDHCI_ACMD12_ERR), RD2(slot, SDHCI_SLOT_INT_STATUS));
	slot_printf(slot, "Caps:     0x%08x | Max curr: 0x%08x\n",
	    RD4(slot, SDHCI_CAPABILITIES), RD4(slot, SDHCI_MAX_CURRENT));

	slot_printf(slot,
	    "===========================================\n");
}

static void
sdhci_reset(struct sdhci_slot *slot, uint8_t mask)
{
	int timeout;
	uint8_t res;

	if (slot->sc->quirks & SDHCI_QUIRK_NO_CARD_NO_RESET) {
		if (!(RD4(slot, SDHCI_PRESENT_STATE) &
			SDHCI_CARD_PRESENT))
			return;
	}

	/* Some controllers need this kick or reset won't work. */
	if ((mask & SDHCI_RESET_ALL) == 0 &&
	    (slot->sc->quirks & SDHCI_QUIRK_CLOCK_BEFORE_RESET)) {
		uint32_t clock;

		/* This is to force an update */
		clock = slot->clock;
		slot->clock = 0;
		sdhci_set_clock(slot, clock);
	}

	WR1(slot, SDHCI_SOFTWARE_RESET, mask);

	if (mask & SDHCI_RESET_ALL) {
		slot->clock = 0;
		slot->power = 0;
	}

	/* Wait max 100 ms */
	timeout = 100;
	/* Controller clears the bits when it's done */
	while ((res = RD1(slot, SDHCI_SOFTWARE_RESET)) & mask) {
		if (timeout == 0) {
			slot_printf(slot,
			    "Reset 0x%x never completed - 0x%x.\n",
			    (int)mask, (int)res);
			sdhci_dumpregs(slot);
			return;
		}
		timeout--;
		DELAY(1000);
	}
}

static void
sdhci_init(struct sdhci_slot *slot)
{

	sdhci_reset(slot, SDHCI_RESET_ALL);

	/* Enable interrupts. */
	slot->intmask = SDHCI_INT_BUS_POWER | SDHCI_INT_DATA_END_BIT |
	    SDHCI_INT_DATA_CRC | SDHCI_INT_DATA_TIMEOUT | SDHCI_INT_INDEX |
	    SDHCI_INT_END_BIT | SDHCI_INT_CRC | SDHCI_INT_TIMEOUT |
	    SDHCI_INT_CARD_REMOVE | SDHCI_INT_CARD_INSERT |
	    SDHCI_INT_DATA_AVAIL | SDHCI_INT_SPACE_AVAIL |
	    SDHCI_INT_DMA_END | SDHCI_INT_DATA_END | SDHCI_INT_RESPONSE |
	    SDHCI_INT_ACMD12ERR;
	WR4(slot, SDHCI_INT_ENABLE, slot->intmask);
	WR4(slot, SDHCI_SIGNAL_ENABLE, slot->intmask);
}

static void
sdhci_set_clock(struct sdhci_slot *slot, uint32_t clock)
{
	uint32_t res;
	uint16_t clk;
	int timeout;

	if (clock == slot->clock)
		return;
	slot->clock = clock;

	/* Turn off the clock. */
	WR2(slot, SDHCI_CLOCK_CONTROL, 0);
	/* If no clock requested - left it so. */
	if (clock == 0)
		return;
	/* Looking for highest freq <= clock. */
	res = slot->max_clk;
	for (clk = 1; clk < 256; clk <<= 1) {
		if (res <= clock)
			break;
		res >>= 1;
	}
	/* Divider 1:1 is 0x00, 2:1 is 0x01, 256:1 is 0x80 ... */
	clk >>= 1;
	/* Now we have got divider, set it. */
	clk <<= SDHCI_DIVIDER_SHIFT;
	WR2(slot, SDHCI_CLOCK_CONTROL, clk);
	/* Enable clock. */
	clk |= SDHCI_CLOCK_INT_EN;
	WR2(slot, SDHCI_CLOCK_CONTROL, clk);
	/* Wait up to 10 ms until it stabilize. */
	timeout = 10;
	while (!((clk = RD2(slot, SDHCI_CLOCK_CONTROL))
		& SDHCI_CLOCK_INT_STABLE)) {
		if (timeout == 0) {
			slot_printf(slot, 
			    "Internal clock never stabilised.\n");
			sdhci_dumpregs(slot);
			return;
		}
		timeout--;
		DELAY(1000);
	}
	/* Pass clock signal to the bus. */
	clk |= SDHCI_CLOCK_CARD_EN;
	WR2(slot, SDHCI_CLOCK_CONTROL, clk);
}

static void
sdhci_set_power(struct sdhci_slot *slot, u_char power)
{
	uint8_t pwr;

	if (slot->power == power)
		return;
	slot->power = power;

	/* Turn off the power. */
	pwr = 0;
	WR1(slot, SDHCI_POWER_CONTROL, pwr);
	/* If power down requested - left it so. */
	if (power == 0)
		return;
	/* Set voltage. */
	switch (1 << power) {
	case MMC_OCR_LOW_VOLTAGE:
		pwr |= SDHCI_POWER_180;
		break;
	case MMC_OCR_290_300:
	case MMC_OCR_300_310:
		pwr |= SDHCI_POWER_300;
		break;
	case MMC_OCR_320_330:
	case MMC_OCR_330_340:
		pwr |= SDHCI_POWER_330;
		break;
	}
	WR1(slot, SDHCI_POWER_CONTROL, pwr);
	/* Turn on the power. */
	pwr |= SDHCI_POWER_ON;
	WR1(slot, SDHCI_POWER_CONTROL, pwr);
}

static void
sdhci_read_block_pio(struct sdhci_slot *slot)
{
	uint32_t data;
	char *buffer;
	size_t left;

	buffer = slot->curcmd->data->data;
	buffer += slot->offset;
	/* Transfer one block at a time. */
	left = min(512, slot->curcmd->data->len - slot->offset);
	slot->offset += left;

	/* If we are too fast, broken controllers return zeroes. */
	if (slot->sc->quirks & SDHCI_QUIRK_BROKEN_TIMINGS)
		DELAY(10);
	/* Handle unalligned and alligned buffer cases. */
	if ((intptr_t)buffer & 3) {
		while (left > 3) {
			data = RD4(slot, SDHCI_BUFFER);
			buffer[0] = data;
			buffer[1] = (data >> 8);
			buffer[2] = (data >> 16);
			buffer[3] = (data >> 24);
			buffer += 4;
			left -= 4;
		}
	} else {
		bus_read_multi_stream_4(slot->mem_res, SDHCI_BUFFER,
		    (uint32_t *)buffer, left >> 2);
		left &= 3;
	}
	/* Handle uneven size case. */
	if (left > 0) {
		data = RD4(slot, SDHCI_BUFFER);
		while (left > 0) {
			*(buffer++) = data;
			data >>= 8;
			left--;
		}
	}
}

static void
sdhci_write_block_pio(struct sdhci_slot *slot)
{
	uint32_t data = 0;
	char *buffer;
	size_t left;

	buffer = slot->curcmd->data->data;
	buffer += slot->offset;
	/* Transfer one block at a time. */
	left = min(512, slot->curcmd->data->len - slot->offset);
	slot->offset += left;

	/* Handle unalligned and alligned buffer cases. */
	if ((intptr_t)buffer & 3) {
		while (left > 3) {
			data = buffer[0] +
			    (buffer[1] << 8) +
			    (buffer[2] << 16) +
			    (buffer[3] << 24);
			left -= 4;
			buffer += 4;
			WR4(slot, SDHCI_BUFFER, data);
		}
	} else {
		bus_write_multi_stream_4(slot->mem_res, SDHCI_BUFFER,
		    (uint32_t *)buffer, left >> 2);
		left &= 3;
	}
	/* Handle uneven size case. */
	if (left > 0) {
		while (left > 0) {
			data <<= 8;
			data += *(buffer++);
			left--;
		}
		WR4(slot, SDHCI_BUFFER, data);
	}
}

static void
sdhci_transfer_pio(struct sdhci_slot *slot)
{

	/* Read as many blocks as possible. */
	if (slot->curcmd->data->flags & MMC_DATA_READ) {
		while (RD4(slot, SDHCI_PRESENT_STATE) &
		    SDHCI_DATA_AVAILABLE) {
			sdhci_read_block_pio(slot);
			if (slot->offset >= slot->curcmd->data->len)
				break;
		}
	} else {
		while (RD4(slot, SDHCI_PRESENT_STATE) &
		    SDHCI_SPACE_AVAILABLE) {
			sdhci_write_block_pio(slot);
			if (slot->offset >= slot->curcmd->data->len)
				break;
		}
	}
}

static void 
sdhci_card_delay(void *arg)
{
	struct sdhci_slot *slot = arg;

	taskqueue_enqueue(taskqueue_swi_giant, &slot->card_task);
}
 
static void
sdhci_card_task(void *arg, int pending)
{
	struct sdhci_slot *slot = arg;

	SDHCI_LOCK(slot);
	if (RD4(slot, SDHCI_PRESENT_STATE) & SDHCI_CARD_PRESENT) {
		if (slot->dev == NULL) {
			/* If card is present - attach mmc bus. */
			slot->dev = device_add_child(slot->sc->dev, "mmc", -1);
			device_set_ivars(slot->dev, slot);
			SDHCI_UNLOCK(slot);
			device_probe_and_attach(slot->dev);
		} else
			SDHCI_UNLOCK(slot);
	} else {
		if (slot->dev != NULL) {
			/* If no card present - detach mmc bus. */
			device_t d = slot->dev;
			slot->dev = NULL;
			SDHCI_UNLOCK(slot);
			device_delete_child(slot->sc->dev, d);
		} else
			SDHCI_UNLOCK(slot);
	}
}

static int
sdhci_probe(device_t dev)
{
	uint32_t model;
	uint16_t subvendor;
	uint8_t class, subclass;
	int i, result;
	
	model = (uint32_t)pci_get_device(dev) << 16;
	model |= (uint32_t)pci_get_vendor(dev) & 0x0000ffff;
	subvendor = pci_get_subvendor(dev);
	class = pci_get_class(dev);
	subclass = pci_get_subclass(dev);
	
	result = ENXIO;
	for (i = 0; sdhci_devices[i].model != 0; i++) {
		if (sdhci_devices[i].model == model &&
		    (sdhci_devices[i].subvendor == 0xffff ||
		    sdhci_devices[i].subvendor == subvendor)) {
			device_set_desc(dev, sdhci_devices[i].desc);
			result = BUS_PROBE_DEFAULT;
			break;
		}
	}
	if (result == ENXIO && class == PCIC_BASEPERIPH &&
	    subclass == PCIS_BASEPERIPH_SDHC) {
		device_set_desc(dev, "Generic SD HCI");
		result = BUS_PROBE_GENERIC;
	}
	
	return (result);
}

static int
sdhci_attach(device_t dev)
{
	struct sdhci_softc *sc = device_get_softc(dev);
	uint32_t model;
	uint16_t subvendor;
	uint8_t class, subclass, progif;
	int err, slots, bar, i;

	sc->dev = dev;
	model = (uint32_t)pci_get_device(dev) << 16;
	model |= (uint32_t)pci_get_vendor(dev) & 0x0000ffff;
	subvendor = pci_get_subvendor(dev);
	class = pci_get_class(dev);
	subclass = pci_get_subclass(dev);
	progif = pci_get_progif(dev);
	/* Apply chip specific quirks. */
	for (i = 0; sdhci_devices[i].model != 0; i++) {
		if (sdhci_devices[i].model == model &&
		    (sdhci_devices[i].subvendor == 0xffff ||
		    sdhci_devices[i].subvendor == subvendor)) {
			sc->quirks = sdhci_devices[i].quirks;
			break;
		}
	}
	/* Read slots info from PCI registers. */
	slots = pci_read_config(dev, PCI_SLOT_INFO, 1);
	bar = PCI_SLOT_INFO_FIRST_BAR(slots);
	slots = PCI_SLOT_INFO_SLOTS(slots);
	if (slots > 6 || bar > 5) {
		device_printf(dev, "Incorrect slots information (%d, %d).\n",
		    slots, bar);
		return (EINVAL);
	}
	/* Allocate IRQ. */
	sc->irq_rid = 0;
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &sc->irq_rid,
	    RF_SHAREABLE | RF_ACTIVE);
	if (sc->irq_res == NULL) {
		device_printf(dev, "Can't allocate IRQ\n");
		return (ENOMEM);
	}
	/* Scan all slots. */
	for (i = 0; i < slots; i++) {
		struct sdhci_slot *slot = &sc->slots[sc->num_slots];
		uint32_t caps;

		SDHCI_LOCK_INIT(slot);
		slot->sc = sc;
		slot->num = sc->num_slots;
		/* Allocate memory. */
		slot->mem_rid = PCIR_BAR(bar + i);
		slot->mem_res = bus_alloc_resource(dev,
		    SYS_RES_MEMORY, &slot->mem_rid, 0ul, ~0ul, 0x100, RF_ACTIVE);
		if (slot->mem_res == NULL) {
			device_printf(dev, "Can't allocate memory\n");
			SDHCI_LOCK_DESTROY(slot);
			continue;
		}
		/* Allocate DMA tag. */
		err = bus_dma_tag_create(bus_get_dma_tag(dev),
		    DMA_BLOCK_SIZE, 0, BUS_SPACE_MAXADDR_32BIT,
		    BUS_SPACE_MAXADDR, NULL, NULL,
		    DMA_BLOCK_SIZE, 1, DMA_BLOCK_SIZE,
		    BUS_DMA_ALLOCNOW, NULL, NULL,
		    &slot->dmatag);
		if (err != 0) {
			device_printf(dev, "Can't create DMA tag\n");
			SDHCI_LOCK_DESTROY(slot);
			continue;
		}
		/* Allocate DMA memory. */
		err = bus_dmamem_alloc(slot->dmatag, (void **)&slot->dmamem,
		    BUS_DMA_NOWAIT, &slot->dmamap);
		if (err != 0) {
			device_printf(dev, "Can't alloc DMA memory\n");
			SDHCI_LOCK_DESTROY(slot);
			continue;
		}
		/* Map the memory. */
		err = bus_dmamap_load(slot->dmatag, slot->dmamap,
		    (void *)slot->dmamem, DMA_BLOCK_SIZE,
		    sdhci_getaddr, &slot->paddr, 0);
		if (err != 0 || slot->paddr == 0) {
			device_printf(dev, "Can't load DMA memory\n");
			SDHCI_LOCK_DESTROY(slot);
			continue;
		}
		/* Initialize slot. */
		sdhci_init(slot);
		caps = RD4(slot, SDHCI_CAPABILITIES);
		/* Calculate base clock frequency. */
		slot->max_clk =
			(caps & SDHCI_CLOCK_BASE_MASK) >> SDHCI_CLOCK_BASE_SHIFT;
		if (slot->max_clk == 0) {
			device_printf(dev, "Hardware doesn't specify base clock "
			    "frequency.\n");
		}
		slot->max_clk *= 1000000;
		/* Calculate timeout clock frequency. */
		slot->timeout_clk =
			(caps & SDHCI_TIMEOUT_CLK_MASK) >> SDHCI_TIMEOUT_CLK_SHIFT;
		if (slot->timeout_clk == 0) {
			device_printf(dev, "Hardware doesn't specify timeout clock "
			    "frequency.\n");
		}
		if (caps & SDHCI_TIMEOUT_CLK_UNIT)
			slot->timeout_clk *= 1000;

		slot->host.f_min = slot->max_clk / 256;
		slot->host.f_max = slot->max_clk;
		slot->host.host_ocr = 0;
		if (caps & SDHCI_CAN_VDD_330)
		    slot->host.host_ocr |= MMC_OCR_320_330 | MMC_OCR_330_340;
		if (caps & SDHCI_CAN_VDD_300)
		    slot->host.host_ocr |= MMC_OCR_290_300 | MMC_OCR_300_310;
		if (caps & SDHCI_CAN_VDD_180)
		    slot->host.host_ocr |= MMC_OCR_LOW_VOLTAGE;
		if (slot->host.host_ocr == 0) {
			device_printf(dev, "Hardware doesn't report any "
			    "support voltages.\n");
		}
		slot->host.caps = MMC_CAP_4_BIT_DATA;
		if (caps & SDHCI_CAN_DO_HISPD)
			slot->host.caps |= MMC_CAP_HSPEED;
		/* Decide if we have usable DMA. */
		if (caps & SDHCI_CAN_DO_DMA)
			slot->opt |= SDHCI_HAVE_DMA;
		if (class == PCIC_BASEPERIPH &&
		    subclass == PCIS_BASEPERIPH_SDHC &&
		    progif != PCI_SDHCI_IFDMA)
			slot->opt &= ~SDHCI_HAVE_DMA;
		if (sc->quirks & SDHCI_QUIRK_BROKEN_DMA)
			slot->opt &= ~SDHCI_HAVE_DMA;
		if (sc->quirks & SDHCI_QUIRK_FORCE_DMA)
			slot->opt |= SDHCI_HAVE_DMA;

		if (bootverbose) {
			slot_printf(slot, "%uMHz%s 4bits%s%s%s %s\n",
			    slot->max_clk / 1000000,
			    (caps & SDHCI_CAN_DO_HISPD) ? " HS" : "",
			    (caps & SDHCI_CAN_VDD_330) ? " 3.3V" : "",
			    (caps & SDHCI_CAN_VDD_300) ? " 3.0V" : "",
			    (caps & SDHCI_CAN_VDD_180) ? " 1.8V" : "",
			    (slot->opt & SDHCI_HAVE_DMA) ? "DMA" : "PIO");
			sdhci_dumpregs(slot);
		}
		
		TASK_INIT(&slot->card_task, 0, sdhci_card_task, slot);
		callout_init(&slot->card_callout, 1);
		sc->num_slots++;
	}
	device_printf(dev, "%d slot(s) allocated\n", sc->num_slots);
	/* Activate the interrupt */
	err = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_MISC | INTR_MPSAFE,
	    NULL, sdhci_intr, sc, &sc->intrhand);
	if (err)
		device_printf(dev, "Can't setup IRQ\n");
	pci_enable_busmaster(dev);
	/* Process cards detection. */
	for (i = 0; i < sc->num_slots; i++) {
		struct sdhci_slot *slot = &sc->slots[i];

		sdhci_card_task(slot, 0);
	}
		
	return (0);
}

static int
sdhci_detach(device_t dev)
{
	struct sdhci_softc *sc = device_get_softc(dev);
	int i;

	bus_teardown_intr(dev, sc->irq_res, sc->intrhand);
	bus_release_resource(dev, SYS_RES_IRQ,
	    sc->irq_rid, sc->irq_res);

	for (i = 0; i < sc->num_slots; i++) {
		struct sdhci_slot *slot = &sc->slots[i];
		device_t d;

		callout_drain(&slot->card_callout);
		taskqueue_drain(taskqueue_swi_giant, &slot->card_task);

		SDHCI_LOCK(slot);
		d = slot->dev;
		slot->dev = NULL;
		SDHCI_UNLOCK(slot);
		if (d != NULL)
			device_delete_child(dev, d);

		SDHCI_LOCK(slot);
		sdhci_reset(slot, SDHCI_RESET_ALL);
		SDHCI_UNLOCK(slot);
		bus_dmamap_unload(slot->dmatag, slot->dmamap);
		bus_dmamem_free(slot->dmatag, slot->dmamem, slot->dmamap);
		bus_dma_tag_destroy(slot->dmatag);
		bus_release_resource(dev, SYS_RES_MEMORY,
		    slot->mem_rid, slot->mem_res);
		SDHCI_LOCK_DESTROY(slot);
	}
	return (0);
}

static int
sdhci_suspend(device_t dev)
{
	struct sdhci_softc *sc = device_get_softc(dev);
	int i, err;

	err = bus_generic_suspend(dev);
	if (err)
		return (err);
	for (i = 0; i < sc->num_slots; i++)
		sdhci_reset(&sc->slots[i], SDHCI_RESET_ALL);
	return (0);
}

static int
sdhci_resume(device_t dev)
{
	struct sdhci_softc *sc = device_get_softc(dev);
	int i;

	for (i = 0; i < sc->num_slots; i++)
		sdhci_init(&sc->slots[i]);
	return (bus_generic_resume(dev));
}

static int
sdhci_update_ios(device_t brdev, device_t reqdev)
{
	struct sdhci_slot *slot = device_get_ivars(reqdev);
	struct mmc_ios *ios = &slot->host.ios;

	SDHCI_LOCK(slot);
	/* Do full reset on bus power down to clear from any state. */
	if (ios->power_mode == power_off) {
		WR4(slot, SDHCI_SIGNAL_ENABLE, 0);
		sdhci_init(slot);
	}
	/* Configure the bus. */
	sdhci_set_clock(slot, ios->clock);
	sdhci_set_power(slot, (ios->power_mode == power_off)?0:ios->vdd);
	if (ios->bus_width == bus_width_4)
		slot->hostctrl |= SDHCI_CTRL_4BITBUS;
	else
		slot->hostctrl &= ~SDHCI_CTRL_4BITBUS;
	if (ios->timing == bus_timing_hs)
		slot->hostctrl |= SDHCI_CTRL_HISPD;
	else
		slot->hostctrl &= ~SDHCI_CTRL_HISPD;
	WR1(slot, SDHCI_HOST_CONTROL, slot->hostctrl);
	/* Some controllers like reset after bus changes. */
	if(slot->sc->quirks & SDHCI_QUIRK_RESET_ON_IOS)
		sdhci_reset(slot, SDHCI_RESET_CMD | SDHCI_RESET_DATA);

	SDHCI_UNLOCK(slot);
	return (0);
}

static void
sdhci_set_transfer_mode(struct sdhci_slot *slot,
	struct mmc_data *data)
{
	uint16_t mode;

	if (data == NULL)
		return;

	mode = SDHCI_TRNS_BLK_CNT_EN;
	if (data->len > 512)
		mode |= SDHCI_TRNS_MULTI;
	if (data->flags & MMC_DATA_READ)
		mode |= SDHCI_TRNS_READ;
	if (slot->req->stop)
		mode |= SDHCI_TRNS_ACMD12;
	if (slot->flags & SDHCI_USE_DMA)
		mode |= SDHCI_TRNS_DMA;

	WR2(slot, SDHCI_TRANSFER_MODE, mode);
}

static void
sdhci_start_command(struct sdhci_slot *slot, struct mmc_command *cmd)
{
	struct mmc_request *req = slot->req;
	int flags, timeout;
	uint32_t mask, state;

	slot->curcmd = cmd;
	slot->cmd_done = 0;

	cmd->error = MMC_ERR_NONE;

	/* This flags combination is not supported by controller. */
	if ((cmd->flags & MMC_RSP_136) && (cmd->flags & MMC_RSP_BUSY)) {
		slot_printf(slot, "Unsupported response type!\n");
		cmd->error = MMC_ERR_FAILED;
		slot->req = NULL;
		slot->curcmd = NULL;
		req->done(req);
		return;
	}

	/* Read controller present state. */
	state = RD4(slot, SDHCI_PRESENT_STATE);
	/* Do not issue command if there is no card, clock or power.
	 * Controller will not detect timeout without clock active. */
	if ((state & SDHCI_CARD_PRESENT) == 0 ||
	    slot->power == 0 ||
	    slot->clock == 0) {
		cmd->error = MMC_ERR_FAILED;
		slot->req = NULL;
		slot->curcmd = NULL;
		req->done(req);
		return;
	}
	/* Always wait for free CMD bus. */
	mask = SDHCI_CMD_INHIBIT;
	/* Wait for free DAT if we have data or busy signal. */
	if (cmd->data || (cmd->flags & MMC_RSP_BUSY))
		mask |= SDHCI_DAT_INHIBIT;
	/* We shouldn't wait for DAT for stop commands. */
	if (cmd == slot->req->stop)
		mask &= ~SDHCI_DAT_INHIBIT;
	/* Wait for bus no more then 10 ms. */
	timeout = 10;
	while (state & mask) {
		if (timeout == 0) {
			slot_printf(slot, "Controller never released "
			    "inhibit bit(s).\n");
			sdhci_dumpregs(slot);
			cmd->error = MMC_ERR_FAILED;
			slot->req = NULL;
			slot->curcmd = NULL;
			req->done(req);
			return;
		}
		timeout--;
		DELAY(1000);
		state = RD4(slot, SDHCI_PRESENT_STATE);
	}

	/* Prepare command flags. */
	if (!(cmd->flags & MMC_RSP_PRESENT))
		flags = SDHCI_CMD_RESP_NONE;
	else if (cmd->flags & MMC_RSP_136)
		flags = SDHCI_CMD_RESP_LONG;
	else if (cmd->flags & MMC_RSP_BUSY)
		flags = SDHCI_CMD_RESP_SHORT_BUSY;
	else
		flags = SDHCI_CMD_RESP_SHORT;
	if (cmd->flags & MMC_RSP_CRC)
		flags |= SDHCI_CMD_CRC;
	if (cmd->flags & MMC_RSP_OPCODE)
		flags |= SDHCI_CMD_INDEX;
	if (cmd->data)
		flags |= SDHCI_CMD_DATA;
	if (cmd->opcode == MMC_STOP_TRANSMISSION)
		flags |= SDHCI_CMD_TYPE_ABORT;
	/* Prepare data. */
	sdhci_start_data(slot, cmd->data);
	/* 
	 * Interrupt aggregation: To reduce total number of interrupts
	 * group response interrupt with data interrupt when possible.
	 * If there going to be data interrupt, mask response one.
	 */
	if (slot->data_done == 0) {
		WR4(slot, SDHCI_SIGNAL_ENABLE,
		    slot->intmask &= ~SDHCI_INT_RESPONSE);
	}
	/* Set command argument. */
	WR4(slot, SDHCI_ARGUMENT, cmd->arg);
	/* Set data transfer mode. */
	sdhci_set_transfer_mode(slot, cmd->data);
	/* Set command flags. */
	WR1(slot, SDHCI_COMMAND_FLAGS, flags);
	/* Start command. */
	WR1(slot, SDHCI_COMMAND, cmd->opcode);
}

static void
sdhci_finish_command(struct sdhci_slot *slot)
{
	int i;

	slot->cmd_done = 1;
	/* Interrupt aggregation: Restore command interrupt.
	 * Main restore point for the case when command interrupt
	 * happened first. */
	WR4(slot, SDHCI_SIGNAL_ENABLE, slot->intmask |= SDHCI_INT_RESPONSE);
	/* In case of error - reset host and return. */
	if (slot->curcmd->error) {
		sdhci_reset(slot, SDHCI_RESET_CMD);
		sdhci_reset(slot, SDHCI_RESET_DATA);
		sdhci_start(slot);
		return;
	}
	/* If command has response - fetch it. */
	if (slot->curcmd->flags & MMC_RSP_PRESENT) {
		if (slot->curcmd->flags & MMC_RSP_136) {
			/* CRC is stripped so we need one byte shift. */
			uint8_t extra = 0;
			for (i = 0; i < 4; i++) {
				uint32_t val = RD4(slot, SDHCI_RESPONSE + i * 4);
				slot->curcmd->resp[3 - i] = (val << 8) + extra;
				extra = val >> 24;
			}
		} else
			slot->curcmd->resp[0] = RD4(slot, SDHCI_RESPONSE);
	}
	/* If data ready - finish. */
	if (slot->data_done)
		sdhci_start(slot);
}

static void
sdhci_start_data(struct sdhci_slot *slot, struct mmc_data *data)
{
	uint32_t target_timeout, current_timeout;
	uint8_t div;

	if (data == NULL && (slot->curcmd->flags & MMC_RSP_BUSY) == 0) {
		slot->data_done = 1;
		return;
	}

	slot->data_done = 0;

	/* Calculate and set data timeout.*/
	/* XXX: We should have this from mmc layer, now assume 1 sec. */
	target_timeout = 1000000;
	div = 0;
	current_timeout = (1 << 13) * 1000 / slot->timeout_clk;
	while (current_timeout < target_timeout) {
		div++;
		current_timeout <<= 1;
		if (div >= 0xF)
			break;
	}
	/* Compensate for an off-by-one error in the CaFe chip.*/
	if (slot->sc->quirks & SDHCI_QUIRK_INCR_TIMEOUT_CONTROL)
		div++;
	if (div >= 0xF) {
		slot_printf(slot, "Timeout too large!\n");
		div = 0xE;
	}
	WR1(slot, SDHCI_TIMEOUT_CONTROL, div);

	if (data == NULL)
		return;

	/* Use DMA if possible. */
	if ((slot->opt & SDHCI_HAVE_DMA))
		slot->flags |= SDHCI_USE_DMA;
	/* If data is small, broken DMA may return zeroes instead of data, */
	if ((slot->sc->quirks & SDHCI_QUIRK_BROKEN_TIMINGS) &&
	    (data->len <= 512))
		slot->flags &= ~SDHCI_USE_DMA;
	/* Some controllers require even block sizes. */
	if ((slot->sc->quirks & SDHCI_QUIRK_32BIT_DMA_SIZE) &&
	    ((data->len) & 0x3))
		slot->flags &= ~SDHCI_USE_DMA;
	/* Load DMA buffer. */
	if (slot->flags & SDHCI_USE_DMA) {
		if (data->flags & MMC_DATA_READ)
			bus_dmamap_sync(slot->dmatag, slot->dmamap, BUS_DMASYNC_PREREAD);
		else {
			memcpy(slot->dmamem, data->data,
			    (data->len < DMA_BLOCK_SIZE)?data->len:DMA_BLOCK_SIZE);
			bus_dmamap_sync(slot->dmatag, slot->dmamap, BUS_DMASYNC_PREWRITE);
		}
		WR4(slot, SDHCI_DMA_ADDRESS, slot->paddr);
		/* Interrupt aggregation: Mask border interrupt
		 * for the last page and unmask else. */
		if (data->len == DMA_BLOCK_SIZE)
			slot->intmask &= ~SDHCI_INT_DMA_END;
		else
			slot->intmask |= SDHCI_INT_DMA_END;
		WR4(slot, SDHCI_SIGNAL_ENABLE, slot->intmask);
	}
	/* Current data offset for both PIO and DMA. */
	slot->offset = 0;
	/* Set block size and request IRQ on 4K border. */
	WR2(slot, SDHCI_BLOCK_SIZE,
	    SDHCI_MAKE_BLKSZ(DMA_BOUNDARY, (data->len < 512)?data->len:512));
	/* Set block count. */
	WR2(slot, SDHCI_BLOCK_COUNT, (data->len + 511) / 512);
}

static void
sdhci_finish_data(struct sdhci_slot *slot)
{
	struct mmc_data *data = slot->curcmd->data;

	slot->data_done = 1;
	/* Interrupt aggregation: Restore command interrupt.
	 * Auxillary restore point for the case when data interrupt
	 * happened first. */
	if (!slot->cmd_done) {
		WR4(slot, SDHCI_SIGNAL_ENABLE,
		    slot->intmask |= SDHCI_INT_RESPONSE);
	}
	/* Unload rest of data from DMA buffer. */
	if (slot->flags & SDHCI_USE_DMA) {
		if (data->flags & MMC_DATA_READ) {
			size_t left = data->len - slot->offset;
			bus_dmamap_sync(slot->dmatag, slot->dmamap, BUS_DMASYNC_POSTREAD);
			memcpy((u_char*)data->data + slot->offset, slot->dmamem,
			    (left < DMA_BLOCK_SIZE)?left:DMA_BLOCK_SIZE);
		} else
			bus_dmamap_sync(slot->dmatag, slot->dmamap, BUS_DMASYNC_POSTWRITE);
	}
	/* If there was error - reset the host. */
	if (slot->curcmd->error) {
		sdhci_reset(slot, SDHCI_RESET_CMD);
		sdhci_reset(slot, SDHCI_RESET_DATA);
		sdhci_start(slot);
		return;
	}
	/* If we already have command response - finish. */
	if (slot->cmd_done)
		sdhci_start(slot);
}

static void
sdhci_start(struct sdhci_slot *slot)
{
	struct mmc_request *req;

	req = slot->req;
	if (req == NULL)
		return;

	if (!(slot->flags & CMD_STARTED)) {
		slot->flags |= CMD_STARTED;
		sdhci_start_command(slot, req->cmd);
		return;
	}
/* 	We don't need this until using Auto-CMD12 feature
	if (!(slot->flags & STOP_STARTED) && req->stop) {
		slot->flags |= STOP_STARTED;
		sdhci_start_command(slot, req->stop);
		return;
	}
*/
	if (req->cmd->error) {
		if (bootverbose) {
			slot_printf(slot,
			    "Command error %d (opcode %u arg %u flags %u "
			    "dlen %u dflags %u)\n",
			    req->cmd->error, req->cmd->opcode, req->cmd->arg,
			    req->cmd->flags,
			    (req->cmd->data)?(u_int)req->cmd->data->len:0,
			    (req->cmd->data)?(u_int)req->cmd->data->flags:0);
		}
	} else if (slot->sc->quirks & SDHCI_QUIRK_RESET_AFTER_REQUEST) {
		sdhci_reset(slot, SDHCI_RESET_CMD);
		sdhci_reset(slot, SDHCI_RESET_DATA);
	}

	/* We must be done -- bad idea to do this while locked? */
	slot->req = NULL;
	slot->curcmd = NULL;
	req->done(req);
}

static int
sdhci_request(device_t brdev, device_t reqdev, struct mmc_request *req)
{
	struct sdhci_slot *slot = device_get_ivars(reqdev);

	SDHCI_LOCK(slot);
	if (slot->req != NULL) {
		SDHCI_UNLOCK(slot);
		return (EBUSY);
	}
/*	printf("%s cmd op %u arg %u flags %u data %ju\n", __func__,
    	    req->cmd->opcode, req->cmd->arg, req->cmd->flags,
    	    (req->cmd->data)?req->cmd->data->len:0); */
	slot->req = req;
	slot->flags = 0;
	sdhci_start(slot);
	SDHCI_UNLOCK(slot);
	return (0);
}

static int
sdhci_get_ro(device_t brdev, device_t reqdev)
{
	struct sdhci_slot *slot = device_get_ivars(reqdev);
	uint32_t val;

	SDHCI_LOCK(slot);
	val = RD4(slot, SDHCI_PRESENT_STATE);
	SDHCI_UNLOCK(slot);
	return (!(val & SDHCI_WRITE_PROTECT));
}

static int
sdhci_acquire_host(device_t brdev, device_t reqdev)
{
	struct sdhci_slot *slot = device_get_ivars(reqdev);
	int err = 0;

	SDHCI_LOCK(slot);
	while (slot->bus_busy)
		msleep(slot, &slot->mtx, PZERO, "sdhciah", hz / 5);
	slot->bus_busy++;
	/* Activate led. */
	WR1(slot, SDHCI_HOST_CONTROL, slot->hostctrl |= SDHCI_CTRL_LED);
	SDHCI_UNLOCK(slot);
	return (err);
}

static int
sdhci_release_host(device_t brdev, device_t reqdev)
{
	struct sdhci_slot *slot = device_get_ivars(reqdev);

	SDHCI_LOCK(slot);
	/* Deactivate led. */
	WR1(slot, SDHCI_HOST_CONTROL, slot->hostctrl &= ~SDHCI_CTRL_LED);
	slot->bus_busy--;
	wakeup(slot);
	SDHCI_UNLOCK(slot);
	return (0);
}

static void
sdhci_cmd_irq(struct sdhci_slot *slot, uint32_t intmask)
{

	if (!slot->curcmd) {
		slot_printf(slot, "Got command interrupt 0x%08x, but "
		    "there is no active command.\n", intmask);
		sdhci_dumpregs(slot);
		return;
	}
	if (intmask & SDHCI_INT_TIMEOUT)
		slot->curcmd->error = MMC_ERR_TIMEOUT;
	else if (intmask & SDHCI_INT_CRC)
		slot->curcmd->error = MMC_ERR_BADCRC;
	else if (intmask & (SDHCI_INT_END_BIT | SDHCI_INT_INDEX))
		slot->curcmd->error = MMC_ERR_FIFO;

	sdhci_finish_command(slot);
}

static void
sdhci_data_irq(struct sdhci_slot *slot, uint32_t intmask)
{

	if (!slot->curcmd) {
		slot_printf(slot, "Got data interrupt 0x%08x, but "
		    "there is no active command.\n", intmask);
		sdhci_dumpregs(slot);
		return;
	}
	if (slot->curcmd->data == NULL &&
	    (slot->curcmd->flags & MMC_RSP_BUSY) == 0) {
		slot_printf(slot, "Got data interrupt 0x%08x, but "
		    "there is no active data operation.\n",
		    intmask);
		sdhci_dumpregs(slot);
		return;
	}
	if (intmask & SDHCI_INT_DATA_TIMEOUT)
		slot->curcmd->error = MMC_ERR_TIMEOUT;
	else if (intmask & (SDHCI_INT_DATA_CRC | SDHCI_INT_DATA_END_BIT))
		slot->curcmd->error = MMC_ERR_BADCRC;
	if (slot->curcmd->data == NULL &&
	    (intmask & (SDHCI_INT_DATA_AVAIL | SDHCI_INT_SPACE_AVAIL |
	    SDHCI_INT_DMA_END))) {
		slot_printf(slot, "Got data interrupt 0x%08x, but "
		    "there is busy-only command.\n", intmask);
		sdhci_dumpregs(slot);
		slot->curcmd->error = MMC_ERR_INVALID;
	}
	if (slot->curcmd->error) {
		/* No need to continue after any error. */
		sdhci_finish_data(slot);
		return;
	}

	/* Handle PIO interrupt. */
	if (intmask & (SDHCI_INT_DATA_AVAIL | SDHCI_INT_SPACE_AVAIL))
		sdhci_transfer_pio(slot);
	/* Handle DMA border. */
	if (intmask & SDHCI_INT_DMA_END) {
		struct mmc_data *data = slot->curcmd->data;
		size_t left;

		/* Unload DMA buffer... */
		left = data->len - slot->offset;
		if (data->flags & MMC_DATA_READ) {
			bus_dmamap_sync(slot->dmatag, slot->dmamap,
			    BUS_DMASYNC_POSTREAD);
			memcpy((u_char*)data->data + slot->offset, slot->dmamem,
			    (left < DMA_BLOCK_SIZE)?left:DMA_BLOCK_SIZE);
		} else {
			bus_dmamap_sync(slot->dmatag, slot->dmamap,
			    BUS_DMASYNC_POSTWRITE);
		}
		/* ... and reload it again. */
		slot->offset += DMA_BLOCK_SIZE;
		left = data->len - slot->offset;
		if (data->flags & MMC_DATA_READ) {
			bus_dmamap_sync(slot->dmatag, slot->dmamap,
			    BUS_DMASYNC_PREREAD);
		} else {
			memcpy(slot->dmamem, (u_char*)data->data + slot->offset,
			    (left < DMA_BLOCK_SIZE)?left:DMA_BLOCK_SIZE);
			bus_dmamap_sync(slot->dmatag, slot->dmamap,
			    BUS_DMASYNC_PREWRITE);
		}
		/* Interrupt aggregation: Mask border interrupt
		 * for the last page. */
		if (left == DMA_BLOCK_SIZE) {
			slot->intmask &= ~SDHCI_INT_DMA_END;
			WR4(slot, SDHCI_SIGNAL_ENABLE, slot->intmask);
		}
		/* Restart DMA. */
		WR4(slot, SDHCI_DMA_ADDRESS, slot->paddr);
	}
	/* We have got all data. */
	if (intmask & SDHCI_INT_DATA_END)
		sdhci_finish_data(slot);
}

static void
sdhci_acmd_irq(struct sdhci_slot *slot)
{
	uint16_t err;
	
	err = RD4(slot, SDHCI_ACMD12_ERR);
	if (!slot->curcmd) {
		slot_printf(slot, "Got AutoCMD12 error 0x%04x, but "
		    "there is no active command.\n", err);
		sdhci_dumpregs(slot);
		return;
	}
	slot_printf(slot, "Got AutoCMD12 error 0x%04x\n", err);
	sdhci_reset(slot, SDHCI_RESET_CMD);
}

static void
sdhci_intr(void *arg)
{
	struct sdhci_softc *sc = (struct sdhci_softc *)arg;
	int i;

	for (i = 0; i < sc->num_slots; i++) {
		struct sdhci_slot *slot = &sc->slots[i];
		uint32_t intmask;
		
		SDHCI_LOCK(slot);
		/* Read slot interrupt status. */
		intmask = RD4(slot, SDHCI_INT_STATUS);
		if (intmask == 0 || intmask == 0xffffffff) {
			SDHCI_UNLOCK(slot);
			continue;
		}
/*
		slot_printf(slot, "got interrupt %x\n", intmask);
*/
		/* Handle card presence interrupts. */
		if (intmask & (SDHCI_INT_CARD_INSERT | SDHCI_INT_CARD_REMOVE)) {
			WR4(slot, SDHCI_INT_STATUS, intmask & 
			    (SDHCI_INT_CARD_INSERT | SDHCI_INT_CARD_REMOVE));

			if (intmask & SDHCI_INT_CARD_REMOVE) {
				if (bootverbose)
					slot_printf(slot, "Card removed\n");
				callout_stop(&slot->card_callout);
				taskqueue_enqueue(taskqueue_swi_giant,
				    &slot->card_task);
			}
			if (intmask & SDHCI_INT_CARD_INSERT) {
				if (bootverbose)
					slot_printf(slot, "Card inserted\n");
				callout_reset(&slot->card_callout, hz / 2,
				    sdhci_card_delay, slot);
			}
			intmask &= ~(SDHCI_INT_CARD_INSERT | SDHCI_INT_CARD_REMOVE);
		}
		/* Handle command interrupts. */
		if (intmask & SDHCI_INT_CMD_MASK) {
			WR4(slot, SDHCI_INT_STATUS, intmask & SDHCI_INT_CMD_MASK);
			sdhci_cmd_irq(slot, intmask & SDHCI_INT_CMD_MASK);
		}
		/* Handle data interrupts. */
		if (intmask & SDHCI_INT_DATA_MASK) {
			WR4(slot, SDHCI_INT_STATUS, intmask & SDHCI_INT_DATA_MASK);
			sdhci_data_irq(slot, intmask & SDHCI_INT_DATA_MASK);
		}
		/* Handle AutoCMD12 error interrupt. */
		if (intmask & SDHCI_INT_ACMD12ERR) {
			WR4(slot, SDHCI_INT_STATUS, SDHCI_INT_ACMD12ERR);
			sdhci_acmd_irq(slot);
		}
		intmask &= ~(SDHCI_INT_CMD_MASK | SDHCI_INT_DATA_MASK);
		intmask &= ~SDHCI_INT_ACMD12ERR;
		intmask &= ~SDHCI_INT_ERROR;
		/* Handle bus power interrupt. */
		if (intmask & SDHCI_INT_BUS_POWER) {
			WR4(slot, SDHCI_INT_STATUS, SDHCI_INT_BUS_POWER);
			slot_printf(slot,
			    "Card is consuming too much power!\n");
			intmask &= ~SDHCI_INT_BUS_POWER;
		}
		/* The rest is unknown. */
		if (intmask) {
			WR4(slot, SDHCI_INT_STATUS, intmask);
			slot_printf(slot, "Unexpected interrupt 0x%08x.\n",
			    intmask);
			sdhci_dumpregs(slot);
		}
		
		SDHCI_UNLOCK(slot);
	}
}

static int
sdhci_read_ivar(device_t bus, device_t child, int which, u_char *result)
{
	struct sdhci_slot *slot = device_get_ivars(child);

	switch (which) {
	default:
		return (EINVAL);
	case MMCBR_IVAR_BUS_MODE:
		*(int *)result = slot->host.ios.bus_mode;
		break;
	case MMCBR_IVAR_BUS_WIDTH:
		*(int *)result = slot->host.ios.bus_width;
		break;
	case MMCBR_IVAR_CHIP_SELECT:
		*(int *)result = slot->host.ios.chip_select;
		break;
	case MMCBR_IVAR_CLOCK:
		*(int *)result = slot->host.ios.clock;
		break;
	case MMCBR_IVAR_F_MIN:
		*(int *)result = slot->host.f_min;
		break;
	case MMCBR_IVAR_F_MAX:
		*(int *)result = slot->host.f_max;
		break;
	case MMCBR_IVAR_HOST_OCR:
		*(int *)result = slot->host.host_ocr;
		break;
	case MMCBR_IVAR_MODE:
		*(int *)result = slot->host.mode;
		break;
	case MMCBR_IVAR_OCR:
		*(int *)result = slot->host.ocr;
		break;
	case MMCBR_IVAR_POWER_MODE:
		*(int *)result = slot->host.ios.power_mode;
		break;
	case MMCBR_IVAR_VDD:
		*(int *)result = slot->host.ios.vdd;
		break;
	case MMCBR_IVAR_CAPS:
		*(int *)result = slot->host.caps;
		break;
	case MMCBR_IVAR_TIMING:
		*(int *)result = slot->host.ios.timing;
		break;
	case MMCBR_IVAR_MAX_DATA:
		*(int *)result = 65535;
		break;
	}
	return (0);
}

static int
sdhci_write_ivar(device_t bus, device_t child, int which, uintptr_t value)
{
	struct sdhci_slot *slot = device_get_ivars(child);

	switch (which) {
	default:
		return (EINVAL);
	case MMCBR_IVAR_BUS_MODE:
		slot->host.ios.bus_mode = value;
		break;
	case MMCBR_IVAR_BUS_WIDTH:
		slot->host.ios.bus_width = value;
		break;
	case MMCBR_IVAR_CHIP_SELECT:
		slot->host.ios.chip_select = value;
		break;
	case MMCBR_IVAR_CLOCK:
		if (value > 0) {
			uint32_t clock = slot->max_clk;
			int i;

			for (i = 0; i < 8; i++) {
				if (clock <= value)
					break;
				clock >>= 1;
			}
			slot->host.ios.clock = clock;
		} else
			slot->host.ios.clock = 0;
		break;
	case MMCBR_IVAR_MODE:
		slot->host.mode = value;
		break;
	case MMCBR_IVAR_OCR:
		slot->host.ocr = value;
		break;
	case MMCBR_IVAR_POWER_MODE:
		slot->host.ios.power_mode = value;
		break;
	case MMCBR_IVAR_VDD:
		slot->host.ios.vdd = value;
		break;
	case MMCBR_IVAR_TIMING:
		slot->host.ios.timing = value;
		break;
	case MMCBR_IVAR_CAPS:
	case MMCBR_IVAR_HOST_OCR:
	case MMCBR_IVAR_F_MIN:
	case MMCBR_IVAR_F_MAX:
	case MMCBR_IVAR_MAX_DATA:
		return (EINVAL);
	}
	return (0);
}

static device_method_t sdhci_methods[] = {
	/* device_if */
	DEVMETHOD(device_probe, sdhci_probe),
	DEVMETHOD(device_attach, sdhci_attach),
	DEVMETHOD(device_detach, sdhci_detach),
	DEVMETHOD(device_suspend, sdhci_suspend),
	DEVMETHOD(device_resume, sdhci_resume),

	/* Bus interface */
	DEVMETHOD(bus_read_ivar,	sdhci_read_ivar),
	DEVMETHOD(bus_write_ivar,	sdhci_write_ivar),

	/* mmcbr_if */
	DEVMETHOD(mmcbr_update_ios, sdhci_update_ios),
	DEVMETHOD(mmcbr_request, sdhci_request),
	DEVMETHOD(mmcbr_get_ro, sdhci_get_ro),
	DEVMETHOD(mmcbr_acquire_host, sdhci_acquire_host),
	DEVMETHOD(mmcbr_release_host, sdhci_release_host),

	{0, 0},
};

static driver_t sdhci_driver = {
	"sdhci",
	sdhci_methods,
	sizeof(struct sdhci_softc),
};
static devclass_t sdhci_devclass;


DRIVER_MODULE(sdhci, pci, sdhci_driver, sdhci_devclass, 0, 0);
