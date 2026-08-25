/*
 * Copyright (c) 2026 Abdelkader Boudih <freebsd@seuros.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * /dev/cbmem — ioctl interface for CBMEM entry enumeration and read
 *
 * Provides structured access to coreboot's CBMEM entries without
 * requiring /dev/mem. Entries can be listed (CBMEM_IOC_LIST) or
 * read by ID (CBMEM_IOC_READ).
 */

#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/ioccom.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/uio.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <dev/coreboot/coreboot.h>

static d_ioctl_t	coreboot_cbmem_ioctl;

static struct cdevsw coreboot_cbmem_cdevsw = {
	.d_version =	D_VERSION,
	.d_ioctl =	coreboot_cbmem_ioctl,
	.d_name =	"cbmem",
};

/*
 * Find a CBMEM entry by its ID.
 */
static struct cbmem_entry_info *
cbmem_find_entry(struct coreboot_softc *sc, uint32_t id)
{
	uint32_t i;

	for (i = 0; i < sc->cbmem_count; i++) {
		if (sc->cbmem_entries[i].id == id)
			return (&sc->cbmem_entries[i]);
	}
	return (NULL);
}

static int
coreboot_cbmem_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int fflag,
    struct thread *td)
{
	struct coreboot_softc *sc;
	int error;

	sc = dev->si_drv1;

	switch (cmd) {
	case CBMEM_IOC_LIST: {
		struct cbmem_list *list = (struct cbmem_list *)data;

		list->count = sc->cbmem_count;
		memcpy(list->entries, sc->cbmem_entries,
		    sc->cbmem_count * sizeof(struct cbmem_entry_info));
		error = 0;
		break;
	}

	case CBMEM_IOC_READ: {
		struct cbmem_read_req *req = (struct cbmem_read_req *)data;
		struct cbmem_entry_info *info;
		void *mapped;
		vm_size_t map_size;
		uint64_t paddr;

		info = cbmem_find_entry(sc, req->id);
		if (info == NULL) {
			error = ENOENT;
			break;
		}

		if (req->offset >= info->size) {
			error = EINVAL;
			break;
		}

		if (req->size > info->size - req->offset)
			req->size = info->size - req->offset;

		if (req->size == 0) {
			error = 0;
			break;
		}

		if (info->address > UINT64_MAX - req->offset) {
			error = EINVAL;
			break;
		}
		paddr = info->address + req->offset;
		map_size = req->size;
		mapped = pmap_mapbios((vm_paddr_t)paddr, map_size);
		if (mapped == NULL) {
			error = ENOMEM;
			break;
		}

		error = copyout(mapped, req->buffer, req->size);

		pmap_unmapbios(mapped, map_size);
		break;
	}

	default:
		error = ENOTTY;
		break;
	}

	return (error);
}

int
coreboot_cbmem_create(struct coreboot_softc *sc)
{
	struct make_dev_args args;
	int error;

	if (sc->cbmem_count == 0)
		return (ENXIO);

	make_dev_args_init(&args);
	args.mda_devsw = &coreboot_cbmem_cdevsw;
	args.mda_uid = UID_ROOT;
	args.mda_gid = GID_WHEEL;
	args.mda_mode = 0440;
	args.mda_si_drv1 = sc;
	error = make_dev_s(&args, &sc->cbmem_cdev, "cbmem");
	if (error != 0)
		return (error);

	return (0);
}

void
coreboot_cbmem_destroy(struct coreboot_softc *sc)
{

	coreboot_cdev_destroy(&sc->cbmem_cdev);
}
