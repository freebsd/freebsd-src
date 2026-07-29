/*
 * Copyright (c) 2026 Abdelkader Boudih <freebsd@seuros.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * /dev/coreboot_console — read-only access to the CBMEM firmware console
 *
 * The CBMEM console is a ring buffer written by coreboot during boot.
 * Bit 31 of the cursor field indicates overflow (the buffer has wrapped).
 * When overflow is set, data starts at cursor and wraps around.
 * When not set, data starts at offset 0 and ends at cursor.
 */

#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/uio.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <dev/coreboot/coreboot.h>

static d_read_t		coreboot_console_read;

static struct cdevsw coreboot_console_cdevsw = {
	.d_version =	D_VERSION,
	.d_read =	coreboot_console_read,
	.d_name =	"coreboot_console",
};

/*
 * Read the console ring buffer.
 *
 * The ring buffer has a cursor that may have wrapped. If the OVERFLOW
 * bit (bit 31) is set, the ring has wrapped and data goes from
 * body[cursor..size-1] then body[0..cursor-1]. Otherwise it's just
 * body[0..cursor-1].
 *
 * We linearize the buffer into a contiguous view for the uiomove.
 */
static int
coreboot_console_read(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct coreboot_softc *sc;
	struct cbmem_console *cons;
	uint32_t cursor, flags, buf_size;
	uint32_t data_start, data_len;
	off_t offset;
	size_t todo;
	int error;

	sc = dev->si_drv1;
	if (sc->console_vaddr == NULL)
		return (ENXIO);

	cons = sc->console_vaddr;
	cursor = cons->cursor & CBMEM_CONSOLE_CURSOR_MASK;
	flags = cons->cursor & ~CBMEM_CONSOLE_CURSOR_MASK;
	buf_size = sc->console_data_size;

	if (cursor > buf_size)
		cursor = 0;

	if (flags & CBMEM_CONSOLE_OVERFLOW) {
		/*
		 * Ring has wrapped. Logical order:
		 *   segment 1: body[cursor .. buf_size-1]  (oldest data)
		 *   segment 2: body[0 .. cursor-1]         (newest data)
		 * Total length = buf_size
		 */
		data_len = buf_size;
		offset = uio->uio_offset;

		if (offset >= data_len)
			return (0);

		while (uio->uio_resid > 0 && offset < data_len) {
			uint32_t seg_start, seg_len;
			uint32_t seg1_len = buf_size - cursor;

			if (offset < seg1_len) {
				seg_start = cursor + offset;
				seg_len = seg1_len - offset;
			} else {
				seg_start = offset - seg1_len;
				seg_len = cursor - seg_start;
			}

			todo = MIN(uio->uio_resid, seg_len);
			error = uiomove(cons->body + seg_start, todo, uio);
			if (error != 0)
				return (error);
			offset += todo;
		}
	} else {
		/*
		 * No overflow — data is linear: body[0 .. cursor-1]
		 */
		data_start = 0;
		data_len = cursor;
		offset = uio->uio_offset;

		if (offset >= data_len)
			return (0);

		todo = MIN(uio->uio_resid, data_len - offset);
		error = uiomove(cons->body + data_start + offset, todo, uio);
		if (error != 0)
			return (error);
	}

	return (0);
}

int
coreboot_console_create(struct coreboot_softc *sc)
{
	struct cbmem_console *tmp;
	vm_size_t map_size;
	uint32_t cons_size;

	if (!sc->has_console || sc->console_paddr == 0)
		return (ENXIO);

	tmp = (struct cbmem_console *)pmap_mapbios(sc->console_paddr,
	    sizeof(struct cbmem_console));
	if (tmp == NULL) {
		device_printf(sc->dev,
		    "unable to map CBMEM console header\n");
		return (ENOMEM);
	}

	cons_size = tmp->size;
	pmap_unmapbios(tmp, sizeof(struct cbmem_console));
	if (cons_size == 0 || cons_size > CB_MAX_CONSOLE_BYTES) {
		device_printf(sc->dev, "invalid CBMEM console size: %u\n",
		    cons_size);
		return (EINVAL);
	}
	map_size = sizeof(struct cbmem_console) + cons_size;

	sc->console_vaddr = (struct cbmem_console *)pmap_mapbios(
	    sc->console_paddr, map_size);
	if (sc->console_vaddr == NULL) {
		device_printf(sc->dev,
		    "unable to map CBMEM console (%zu bytes)\n",
		    (size_t)map_size);
		return (ENOMEM);
	}
	sc->console_size = map_size;
	sc->console_data_size = cons_size;

	struct make_dev_args args;
	int error;

	make_dev_args_init(&args);
	args.mda_devsw = &coreboot_console_cdevsw;
	args.mda_uid = UID_ROOT;
	args.mda_gid = GID_WHEEL;
	args.mda_mode = 0440;
	args.mda_si_drv1 = sc;
	error = make_dev_s(&args, &sc->console_cdev, "coreboot_console");
	if (error != 0) {
		pmap_unmapbios(sc->console_vaddr, sc->console_size);
		sc->console_vaddr = NULL;
		sc->console_size = 0;
		sc->console_data_size = 0;
		return (error);
	}

	device_printf(sc->dev,
	    "CBMEM console: %u bytes, cursor at %u%s\n",
	    sc->console_vaddr->size,
	    sc->console_vaddr->cursor & CBMEM_CONSOLE_CURSOR_MASK,
	    (sc->console_vaddr->cursor & CBMEM_CONSOLE_OVERFLOW) ?
	    " (wrapped)" : "");

	return (0);
}

void
coreboot_console_destroy(struct coreboot_softc *sc)
{

	coreboot_cdev_destroy(&sc->console_cdev);
}
