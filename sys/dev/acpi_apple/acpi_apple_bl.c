/*
 * Copyright (c) 2026 Abdelkader Boudih <seuros@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Apple panel backlight control.
 *
 * Two backends: GMUX (dual-GPU MacBook Pros, base+0x70/0x74 over PIO,
 * indexed I/O, or MMIO depending on platform) and direct GPU PCI BAR
 * MMIO (single-GPU Macs).  GMUX is preferred whenever APP000B is
 * present and its _CRS yields a working protocol.
 */

#include <sys/cdefs.h>
#include "acpi_apple.h"

#include <sys/endian.h>
#include <sys/systm.h>

#include <dev/backlight/backlight.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <machine/cpufunc.h>
#include <vm/vm.h>
#include <vm/pmap.h>

#include "backlight_if.h"

ACPI_SERIAL_DECL(apple, "Apple ACPI");
static struct acpi_apple_softc *apple_backlight_sc;

static const struct apple_bl_hw apple_bl_intel_pch = {
	.name		= "intel_pch",
	.bar		= 0,
	.mapsize	= 0xc8258,
	.ctrl		= { 0xc8254, 0x0000FFFF, 0 },
	.enable		= { 0xc8250, 0xC0000000, 0 },
	.enable_val	= 0xC0000000,
	.max_reg	= { 0xc8254, 0xFFFF0000, 16 },
	.max		= 0xFFFF,
};

static const struct apple_bl_hw apple_bl_intel_cpu = {
	.name		= "intel_cpu",
	.bar		= 0,
	.mapsize	= 0xc8258,
	.ctrl		= { 0x48254, 0x0000FFFF, 0 },
	.enable		= { 0x48250, 0xC0000000, 0 },
	.enable_val	= 0xC0000000,
	.max_reg	= { 0xc8254, 0xFFFF0000, 16 },
	.max		= 0xFFFF,
};

static const struct apple_bl_hw apple_bl_nvidia_nv40 = {
	.name		= "nvidia_nv40",
	.bar		= 0,
	.mapsize	= 0x15f4,
	.ctrl		= { 0x15f0, 0x001F0000, 16 },
	.max		= 31,
};

static const struct apple_bl_hw apple_bl_nvidia_nv50 = {
	.name		= "nvidia_nv50",
	.bar		= 0,
	.mapsize	= 0x61c088,
	.ctrl		= { 0x61c084, 0x000007FF, 0 },
	.enable		= { 0x61c084, 0x80000000, 0 },
	.enable_val	= 0x80000000,
	.max		= 0x7FF,
};

static const struct apple_bl_hw apple_bl_amd_r600 = {
	.name		= "amd_r600",
	.bar		= 2,
	.mapsize	= 0x1730,
	.ctrl		= { 0x172c, 0x0000FF00, 8 },
	.max		= 255,
};

/* DCE10 (Tonga/Fiji, e.g. R9 M395/M395X, R9 Nano). */
static const struct apple_bl_hw apple_bl_amd_dce10 = {
	.name		= "amd_dce10",
	.bar		= 2,
	.mapsize	= 0x1208c,
	.ctrl		= { 0x12080, 0x0000FFFF, 0 },
	.enable		= { 0x12080, 0x80000000, 0 },
	.enable_val	= 0x80000000,
	.max_reg	= { 0x12088, 0x0000FFFF, 0 },
	.max		= 0xFFFF,
};

struct apple_bl_match {
	uint16_t		vendor;
	uint16_t		dev_lo;
	uint16_t		dev_hi;
	const struct apple_bl_hw *hw;
};

static const struct apple_bl_match apple_bl_matches[] = {
	{ 0x8086, 0x0100, 0x013f, &apple_bl_intel_cpu },
	{ 0x8086, 0x0140, 0x017f, &apple_bl_intel_cpu },
	{ 0x8086, 0, 0, &apple_bl_intel_pch },
	{ 0x10de, 0x0040, 0x03ff, &apple_bl_nvidia_nv40 },
	{ 0x10de, 0, 0, &apple_bl_nvidia_nv50 },
	{ 0x1002, 0x6920, 0x6939, &apple_bl_amd_dce10 },
	{ 0x1002, 0, 0, &apple_bl_amd_r600 },
	{ 0, 0, 0, NULL },
};

/* Classic (PIO) GMUX - pre-Retina Macs. */
static uint32_t
gmux_pio_read32(uint16_t base, uint8_t port)
{
	return (inl(base + port));
}

/* A 32-bit GMUX write goes out as four byte writes, low byte first. */
static void
gmux_pio_write32_at(uint16_t addr, uint32_t val)
{

	for (u_int i = 0; i < sizeof(val); i++)
		outb(addr + i, (uint8_t)(val >> (i * NBBY)));
}

static void
gmux_pio_write32(uint16_t base, uint8_t port, uint32_t val)
{
	gmux_pio_write32_at(base + port, val);
}

/* Indexed GMUX - Retina Macs (2012-2015). */
static int
gmux_index_wait_ready(uint16_t base)
{
	uint8_t gwr;

	for (int i = 0; i < GMUX_INDEXED_RETRIES; i++) {
		gwr = inb(base + GMUX_PORT_WRITE);
		if ((gwr & GMUX_INDEXED_BUSY) == 0)
			return (0);
		inb(base + GMUX_PORT_READ);
		DELAY(GMUX_POLL_DELAY);
	}
	return (ETIMEDOUT);
}

static int
gmux_index_wait_complete(uint16_t base)
{
	uint8_t gwr;

	for (int i = 0; i < GMUX_INDEXED_RETRIES; i++) {
		gwr = inb(base + GMUX_PORT_WRITE);
		if ((gwr & GMUX_INDEXED_BUSY) != 0) {
			/* Acknowledge completion. */
			inb(base + GMUX_PORT_READ);
			return (0);
		}
		DELAY(GMUX_POLL_DELAY);
	}
	/* Clear stale status. */
	inb(base + GMUX_PORT_READ);
	return (ETIMEDOUT);
}

static uint32_t
gmux_index_read32(uint16_t base, uint8_t port)
{
	uint32_t val;

	gmux_index_wait_ready(base);
	outb(base + GMUX_PORT_READ, port);
	gmux_index_wait_complete(base);
	val = inl(base + GMUX_PORT_VALUE);
	return (val);
}

static void
gmux_index_write32(uint16_t base, uint8_t port, uint32_t val)
{
	gmux_pio_write32_at(base + GMUX_PORT_VALUE, val);
	gmux_index_wait_ready(base);
	outb(base + GMUX_PORT_WRITE, port);
	gmux_index_wait_complete(base);
}

/* MMIO GMUX - T2 Macs. */
static inline uint8_t
gmux_mmio_read8(void *mmio, uint8_t port)
{
	return (*(volatile uint8_t *)((char *)mmio + port));
}

static inline void
gmux_mmio_write8(void *mmio, uint8_t port, uint8_t val)
{
	*(volatile uint8_t *)((char *)mmio + port) = val;
}

static int
gmux_mmio_wait(void *mmio)
{

	for (int i = 0; i < GMUX_MMIO_RETRIES; i++) {
		if (gmux_mmio_read8(mmio, GMUX_MMIO_COMMAND_SEND) == 0)
			return (0);
		DELAY(GMUX_POLL_DELAY);
	}
	return (ETIMEDOUT);
}

static uint32_t
gmux_mmio_read32(void *mmio, uint8_t port)
{
	uint32_t val;

	gmux_mmio_wait(mmio);
	gmux_mmio_write8(mmio, GMUX_MMIO_PORT_SELECT, port);
	gmux_mmio_write8(mmio, GMUX_MMIO_COMMAND_SEND,
	    GMUX_MMIO_READ | sizeof(val));
	gmux_mmio_wait(mmio);
	val = be32toh(*(volatile uint32_t *)mmio);
	return (val);
}

static void
gmux_mmio_write32(void *mmio, uint8_t port, uint32_t val)
{
	gmux_mmio_wait(mmio);
	*(volatile uint32_t *)mmio = htobe32(val);
	gmux_mmio_write8(mmio, GMUX_MMIO_PORT_SELECT, port);
	gmux_mmio_write8(mmio, GMUX_MMIO_COMMAND_SEND,
	    GMUX_MMIO_WRITE | sizeof(val));
	gmux_mmio_wait(mmio);
}

static uint32_t
gmux_read32(struct acpi_apple_softc *sc, uint8_t port)
{
	if (sc->bl_gmux_type == GMUX_TYPE_MMIO)
		return (gmux_mmio_read32(sc->bl_gmux_mmio, port));
	if (sc->bl_gmux_type == GMUX_TYPE_INDEXED)
		return (gmux_index_read32(sc->bl_gmux_base, port));
	return (gmux_pio_read32(sc->bl_gmux_base, port));
}

static void
gmux_write32(struct acpi_apple_softc *sc, uint8_t port, uint32_t val)
{
	if (sc->bl_gmux_type == GMUX_TYPE_MMIO)
		gmux_mmio_write32(sc->bl_gmux_mmio, port, val);
	else if (sc->bl_gmux_type == GMUX_TYPE_INDEXED)
		gmux_index_write32(sc->bl_gmux_base, port, val);
	else
		gmux_pio_write32(sc->bl_gmux_base, port, val);
}

static inline bool
apple_bl_gmux_active(struct acpi_apple_softc *sc)
{
	return (sc->bl_gmux_base != 0 || sc->bl_gmux_mmio != NULL);
}

static void
apple_bl_gmux_teardown(struct acpi_apple_softc *sc)
{
	if (sc->bl_gmux_mmio != NULL) {
		pmap_unmapdev(sc->bl_gmux_mmio, GMUX_MIN_IO_LEN);
		sc->bl_gmux_mmio = NULL;
	}
	sc->bl_gmux_base = 0;
}

static inline uint32_t
apple_bl_read(struct acpi_apple_softc *sc, uint32_t offset)
{
	return (*(volatile uint32_t *)((char *)sc->bl_mmio + offset));
}

static inline void
apple_bl_write(struct acpi_apple_softc *sc, uint32_t offset, uint32_t val)
{
	*(volatile uint32_t *)((char *)sc->bl_mmio + offset) = val;
}

static int
apple_bl_hw_get(struct acpi_apple_softc *sc)
{
	if (apple_bl_gmux_active(sc))
		return (gmux_read32(sc,
		    GMUX_PORT_BRIGHTNESS) & GMUX_BRIGHTNESS_MASK);

	return ((apple_bl_read(sc, sc->bl_hw->ctrl.offset) &
	    sc->bl_hw->ctrl.mask) >> sc->bl_hw->ctrl.shift);
}

/* Resolve max brightness, enable the PWM if needed, cache brightness. */
static void
apple_bl_mmio_init(struct acpi_apple_softc *sc, const struct apple_bl_hw *hw)
{
	uint32_t val;

	if (hw->max_reg.offset != 0) {
		sc->bl_max = (apple_bl_read(sc, hw->max_reg.offset) &
		    hw->max_reg.mask) >> hw->max_reg.shift;
		if (sc->bl_max == 0)
			sc->bl_max = hw->max;
	} else {
		sc->bl_max = hw->max;
	}

	if (hw->enable.offset != 0) {
		val = apple_bl_read(sc, hw->enable.offset);
		if ((val & hw->enable_val) != hw->enable_val) {
			val |= hw->enable_val;
			apple_bl_write(sc, hw->enable.offset, val);
		}
	}

	if (sc->bl_max > 0)
		sc->bl_brightness = apple_bl_hw_get(sc) * 100 / sc->bl_max;
}

static void
apple_bl_hw_set(struct acpi_apple_softc *sc, int level)
{
	uint32_t val;

	if (apple_bl_gmux_active(sc)) {
		gmux_write32(sc, GMUX_PORT_BRIGHTNESS,
		    level & GMUX_BRIGHTNESS_MASK);
		return;
	}

	val = apple_bl_read(sc, sc->bl_hw->ctrl.offset);
	val = (val & ~sc->bl_hw->ctrl.mask) |
	    ((level << sc->bl_hw->ctrl.shift) & sc->bl_hw->ctrl.mask);
	apple_bl_write(sc, sc->bl_hw->ctrl.offset, val);
}

static const struct apple_bl_hw *
apple_bl_find_gpu(device_t *gpu_devp)
{
	const struct apple_bl_match *m;
	device_t gpu;
	uint16_t vendor, device;

	gpu = pci_find_class(PCIC_DISPLAY, PCIS_DISPLAY_VGA);
	if (gpu == NULL)
		return (NULL);

	vendor = pci_get_vendor(gpu);
	device = pci_get_device(gpu);

	for (m = apple_bl_matches; m->vendor != 0; m++) {
		if (vendor != m->vendor)
			continue;
		if (m->dev_lo != 0 &&
		    (device < m->dev_lo || device > m->dev_hi))
			continue;
		*gpu_devp = gpu;
		return (m->hw);
	}

	return (NULL);
}

static void *
apple_bl_map_bar(device_t dev, device_t gpu, const struct apple_bl_hw *hw)
{
	uint64_t bar_addr;
	uint32_t bar_lo, bar_hi;

	bar_lo = pci_read_config(gpu, PCIR_BAR(hw->bar), 4);

	if (PCI_BAR_IO(bar_lo)) {
		device_printf(dev, "BAR%d is I/O, expected memory\n", hw->bar);
		return (NULL);
	}

	if ((bar_lo & PCIM_BAR_MEM_TYPE) == PCIM_BAR_MEM_64) {
		bar_hi = pci_read_config(gpu, PCIR_BAR(hw->bar) + 4, 4);
		bar_addr = ((uint64_t)bar_hi << 32) |
		    (bar_lo & PCIM_BAR_MEM_BASE);
	} else {
		bar_addr = bar_lo & PCIM_BAR_MEM_BASE;
	}

	if (bar_addr == 0) {
		device_printf(dev, "BAR%d address is zero\n", hw->bar);
		return (NULL);
	}

	return (pmap_mapdev(bar_addr, hw->mapsize));
}

/*
 * Probe for GMUX.  Locate APP000B in the ACPI namespace by HID, consume
 * its _CRS resource, and detect which I/O protocol the hardware speaks.
 */
#define	GMUX_ACPI_HID	"APP000B"

struct apple_bl_gmux_crs {
	uint16_t	io_base;
	uint32_t	io_len;
	uint64_t	mem_base;
};

static ACPI_STATUS
apple_bl_gmux_found(ACPI_HANDLE handle, UINT32 level __unused, void *ctx,
    void **retval __unused)
{
	*(ACPI_HANDLE *)ctx = handle;
	return (AE_CTRL_TERMINATE);
}

static ACPI_STATUS
apple_bl_gmux_crs_cb(ACPI_RESOURCE *res, void *ctx)
{
	struct apple_bl_gmux_crs *crs = ctx;

	switch (res->Type) {
	case ACPI_RESOURCE_TYPE_IO:
		crs->io_base = res->Data.Io.Minimum;
		crs->io_len = res->Data.Io.AddressLength;
		break;
	case ACPI_RESOURCE_TYPE_FIXED_IO:
		crs->io_base = res->Data.FixedIo.Address;
		crs->io_len = res->Data.FixedIo.AddressLength;
		break;
	case ACPI_RESOURCE_TYPE_MEMORY32:
		crs->mem_base = res->Data.Memory32.Minimum;
		break;
	case ACPI_RESOURCE_TYPE_FIXED_MEMORY32:
		crs->mem_base = res->Data.FixedMemory32.Address;
		break;
	default:
		break;
	}
	return (AE_OK);
}

static bool
apple_bl_is_indexed(uint16_t base)
{
	uint16_t val;

	outb(base + GMUX_PORT_INDEXED_MAGIC1, GMUX_INDEXED_UNLOCK1);
	outb(base + GMUX_PORT_INDEXED_MAGIC2, GMUX_INDEXED_UNLOCK2);
	outb(base + GMUX_PORT_INDEXED_MAGIC3, GMUX_INDEXED_UNLOCK3);

	val = inb(base + GMUX_PORT_INDEXED_MAGIC1) |
	    ((uint16_t)inb(base + GMUX_PORT_INDEXED_MAGIC2) << 8);
	return (val == GMUX_INDEXED_SIGNATURE);
}

static bool
apple_bl_is_mmio(void *mmio)
{
	return (gmux_mmio_read8(mmio, GMUX_MMIO_COMMAND_SEND) !=
	    GMUX_FLOATING);
}

static bool
apple_bl_find_gmux(struct acpi_apple_softc *sc)
{
	ACPI_HANDLE gmux_handle;
	struct apple_bl_gmux_crs crs;
	uint16_t base;
	uint8_t ver_major, ver_minor, ver_release;
	uint32_t version;

	gmux_handle = NULL;
	AcpiGetDevices(GMUX_ACPI_HID, apple_bl_gmux_found, &gmux_handle, NULL);
	if (gmux_handle == NULL)
		return (false);

	bzero(&crs, sizeof(crs));
	if (ACPI_FAILURE(AcpiWalkResources(gmux_handle, "_CRS",
	    apple_bl_gmux_crs_cb, &crs)))
		return (false);

	if (crs.io_base != 0 && crs.io_len >= GMUX_MIN_IO_LEN) {
		base = crs.io_base;

		/* Try direct PIO first (classic/pre-Retina GMUX). */
		ver_major = inb(base + GMUX_PORT_VERSION_MAJOR);
		ver_minor = inb(base + GMUX_PORT_VERSION_MINOR);
		ver_release = inb(base + GMUX_PORT_VERSION_RELEASE);

		if (ver_major != GMUX_FLOATING ||
		    ver_minor != GMUX_FLOATING) {
			sc->bl_gmux_base = base;
			sc->bl_gmux_type = GMUX_TYPE_PIO;
			device_printf(sc->dev,
			    "GMUX v%d.%d.%d [classic] at 0x%x\n",
			    ver_major, ver_minor, ver_release, base);
			return (true);
		}

		/* The bus floated high - check for indexed GMUX. */
		if (apple_bl_is_indexed(base)) {
			sc->bl_gmux_base = base;
			sc->bl_gmux_type = GMUX_TYPE_INDEXED;

			version = gmux_index_read32(base,
			    GMUX_PORT_VERSION_MAJOR);
			ver_major = GMUX_VERSION_MAJOR(version);
			ver_minor = GMUX_VERSION_MINOR(version);
			ver_release = GMUX_VERSION_RELEASE(version);

			device_printf(sc->dev,
			    "GMUX v%d.%d.%d [indexed] at 0x%x\n",
			    ver_major, ver_minor, ver_release, base);
			return (true);
		}
	}

	/* No usable I/O resource - try the MMIO protocol (T2 GMUX). */
	if (crs.mem_base == 0)
		return (false);

	sc->bl_gmux_mmio = pmap_mapdev(crs.mem_base, GMUX_MIN_IO_LEN);
	if (sc->bl_gmux_mmio == NULL)
		return (false);

	if (!apple_bl_is_mmio(sc->bl_gmux_mmio)) {
		pmap_unmapdev(sc->bl_gmux_mmio, GMUX_MIN_IO_LEN);
		sc->bl_gmux_mmio = NULL;
		return (false);
	}

	sc->bl_gmux_type = GMUX_TYPE_MMIO;

	version = gmux_mmio_read32(sc->bl_gmux_mmio, GMUX_PORT_VERSION_MAJOR);
	ver_major = GMUX_VERSION_MAJOR(version);
	ver_minor = GMUX_VERSION_MINOR(version);
	ver_release = GMUX_VERSION_RELEASE(version);

	device_printf(sc->dev, "GMUX v%d.%d.%d [mmio] at %#jx\n",
	    ver_major, ver_minor, ver_release, (uintmax_t)crs.mem_base);
	return (true);
}

static int
acpi_apple_backlight_attach_gmux(struct acpi_apple_softc *sc)
{
	uint32_t raw_max;

	mtx_init(&sc->bl_mtx, "apple_bl", NULL, MTX_DEF);

	raw_max = gmux_read32(sc, GMUX_PORT_MAX_BRIGHTNESS) &
	    GMUX_BRIGHTNESS_MASK;
	sc->bl_max = (raw_max > 0) ? raw_max : GMUX_BRIGHTNESS_MASK;

	if (sc->bl_max > 0)
		sc->bl_brightness = apple_bl_hw_get(sc) * 100 / sc->bl_max;

	sc->bl_cdev = backlight_register("apple_bl", sc->dev);
	if (sc->bl_cdev == NULL) {
		device_printf(sc->dev, "cannot register backlight\n");
		apple_bl_gmux_teardown(sc);
		mtx_destroy(&sc->bl_mtx);
		return (ENXIO);
	}

	device_printf(sc->dev, "gmux brightness %d/%d\n",
	    apple_bl_hw_get(sc), sc->bl_max);

	ACPI_SERIAL_BEGIN(apple);
	apple_backlight_sc = sc;
	ACPI_SERIAL_END(apple);
	return (0);
}

static int
apple_bl_attach_gpu(struct acpi_apple_softc *sc, device_t gpu,
    const struct apple_bl_hw *hw)
{
	sc->bl_hw = hw;

	sc->bl_mmio = apple_bl_map_bar(sc->dev, gpu, hw);
	if (sc->bl_mmio == NULL)
		return (ENXIO);

	mtx_init(&sc->bl_mtx, "apple_bl", NULL, MTX_DEF);
	apple_bl_mmio_init(sc, hw);

	sc->bl_cdev = backlight_register("apple_bl", sc->dev);
	if (sc->bl_cdev == NULL) {
		device_printf(sc->dev, "cannot register backlight\n");
		mtx_destroy(&sc->bl_mtx);
		pmap_unmapdev(sc->bl_mmio, hw->mapsize);
		sc->bl_mmio = NULL;
		return (ENXIO);
	}

	device_printf(sc->dev, "%s, brightness %d/%d\n",
	    hw->name, apple_bl_hw_get(sc), sc->bl_max);

	ACPI_SERIAL_BEGIN(apple);
	apple_backlight_sc = sc;
	ACPI_SERIAL_END(apple);
	return (0);
}

static void
acpi_apple_backlight_deferred(void *arg)
{
	struct acpi_apple_softc *sc = arg;
	device_t gpu;
	const struct apple_bl_hw *hw;

	hw = apple_bl_find_gpu(&gpu);
	if (hw == NULL) {
		device_printf(sc->dev, "no supported GPU found\n");
		return;
	}

	(void)apple_bl_attach_gpu(sc, gpu, hw);
}

static int
acpi_apple_backlight_attach_mmio(struct acpi_apple_softc *sc)
{
	device_t gpu;
	const struct apple_bl_hw *hw;

	hw = apple_bl_find_gpu(&gpu);
	if (hw == NULL) {
		/*
		 * GPU not yet attached (ACPI bus enumerates before PCI).
		 * Defer the lookup until after all buses finish probing.
		 */
		config_intrhook_oneshot(acpi_apple_backlight_deferred, sc);
		return (0);
	}

	return (apple_bl_attach_gpu(sc, gpu, hw));
}

int
acpi_apple_backlight_attach(struct acpi_apple_softc *sc)
{
	if (apple_bl_find_gmux(sc))
		return (acpi_apple_backlight_attach_gmux(sc));

	return (acpi_apple_backlight_attach_mmio(sc));
}

void
acpi_apple_backlight_detach(struct acpi_apple_softc *sc)
{
	bool had_gmux;

	ACPI_SERIAL_BEGIN(apple);
	if (apple_backlight_sc == sc)
		apple_backlight_sc = NULL;
	ACPI_SERIAL_END(apple);

	had_gmux = apple_bl_gmux_active(sc);

	if (sc->bl_cdev != NULL)
		backlight_destroy(sc->bl_cdev);
	if (sc->bl_mmio != NULL)
		pmap_unmapdev(sc->bl_mmio, sc->bl_hw->mapsize);
	apple_bl_gmux_teardown(sc);
	if (sc->bl_cdev != NULL || sc->bl_mmio != NULL || had_gmux)
		mtx_destroy(&sc->bl_mtx);
}

static inline bool
apple_bl_ready(struct acpi_apple_softc *sc)
{
	return (sc->bl_hw != NULL || apple_bl_gmux_active(sc));
}

static inline int
apple_bl_pct_to_hw(struct acpi_apple_softc *sc, int pct)
{
	int level;

	level = pct * sc->bl_max / 100;
	if (level > (int)sc->bl_max)
		level = sc->bl_max;
	return (level);
}

int
acpi_apple_bl_update_status(device_t dev, struct backlight_props *props)
{
	struct acpi_apple_softc *sc;

	sc = device_get_softc(dev);
	if (!apple_bl_ready(sc))
		return (ENXIO);
	mtx_lock(&sc->bl_mtx);
	apple_bl_hw_set(sc, apple_bl_pct_to_hw(sc, props->brightness));
	sc->bl_brightness = props->brightness;
	mtx_unlock(&sc->bl_mtx);

	return (0);
}

int
acpi_apple_bl_get_status(device_t dev, struct backlight_props *props)
{
	struct acpi_apple_softc *sc;

	sc = device_get_softc(dev);
	if (!apple_bl_ready(sc))
		return (ENXIO);
	props->brightness = sc->bl_brightness;
	props->nlevels = 0;

	return (0);
}

int
acpi_apple_bl_get_info(device_t dev, struct backlight_info *info)
{
	info->type = BACKLIGHT_TYPE_PANEL;
	strlcpy(info->name, "Apple Panel Backlight", BACKLIGHTMAXNAMELENGTH);
	return (0);
}

/*
 * Restore backlight to cached brightness.  Called from the platform
 * instance lid-open notify handler.  Caller holds no locks.
 */
void
acpi_apple_backlight_restore(void)
{
	struct acpi_apple_softc *sc;

	ACPI_SERIAL_BEGIN(apple);
	sc = apple_backlight_sc;
	if (sc == NULL || !apple_bl_ready(sc)) {
		ACPI_SERIAL_END(apple);
		return;
	}
	mtx_lock(&sc->bl_mtx);
	apple_bl_hw_set(sc, apple_bl_pct_to_hw(sc, sc->bl_brightness));
	mtx_unlock(&sc->bl_mtx);
	ACPI_SERIAL_END(apple);
}

/*
 * Turn off backlight.  Called from the platform instance lid-close
 * notify handler.  Does not change the cached brightness so that
 * restore() brings it back on lid open.
 */
void
acpi_apple_backlight_off(void)
{
	struct acpi_apple_softc *sc;

	ACPI_SERIAL_BEGIN(apple);
	sc = apple_backlight_sc;
	if (sc == NULL || !apple_bl_ready(sc)) {
		ACPI_SERIAL_END(apple);
		return;
	}
	mtx_lock(&sc->bl_mtx);
	apple_bl_hw_set(sc, 0);
	mtx_unlock(&sc->bl_mtx);
	ACPI_SERIAL_END(apple);
}
