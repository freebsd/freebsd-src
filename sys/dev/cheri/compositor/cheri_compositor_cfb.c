/*-
 * Copyright (c) 2013 Philip Withnall
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: head/sys/dev/cheri/compositor/cheri_compositor_cfb.c 239691 2012-08-25 22:35:29Z rwatson $");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/rman.h>
#include <sys/rwlock.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <vm/pmap.h>
#include <vm/vm_object.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vm_param.h>
#include <vm/vm_phys.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/vm.h>

#include <dev/cheri/compositor/cheri_compositor_internal.h>

MALLOC_DECLARE(M_CHERI_COMPOSITOR);

static d_mmap_single_t cheri_compositor_cfb_mmap_single;

static struct cdevsw compositor_cfb_cdevsw = {
	.d_version =	D_VERSION,
	.d_mmap_single = cheri_compositor_cfb_mmap_single,
	.d_ioctl =	cheri_compositor_reg_ioctl,
	.d_name =	"cheri_compositor_cfb",
};

/**
 * Calculate the physical address in compositor memory corresponding to the
 * given offset (in bytes) from the base of the given CFB pool.
 *
 * offset must be within the CFB pool's bounds (i.e. less than
 * CHERI_COMPOSITOR_MEM_POOL_LENGTH).
 */
static vm_paddr_t
calculate_physical_address(struct cheri_compositor_softc *sc,
    const struct compositor_cfb_pool *cfb_pool, vm_ooffset_t offset)
{
	vm_paddr_t physical_address;

	physical_address =
	    rman_get_start(sc->compositor_cfb_res) +
	    compositor_cfb_pool_to_byte_offset(sc, cfb_pool) +
	    offset;

	CHERI_COMPOSITOR_DEBUG(sc, "physical_address: %p",
	    (void *) physical_address);

	return physical_address;
}

/**
 * Validate standard prot and offset arguments to various memory functions. This
 * returns 0 if all checks passed, and -1 on error.
 */
static int
validate_prot_and_offset(struct cheri_compositor_softc *sc,
    const struct file *cdev_fd, int prot, vm_ooffset_t offset)
{
	/* Disallow executable mappings. */
	if ((prot & PROT_EXEC) != 0) {
		CHERI_COMPOSITOR_ERROR(sc,
		    "Invalid prot (%u); must not have PROT_EXEC set.", prot);
		return EACCES;
	}

	/* Ensure prot is a subset of cdev_fd->f_flag. */
	if (((cdev_fd->f_flag & FREAD) == 0 && (prot & PROT_READ) != 0) ||
	    ((cdev_fd->f_flag & FWRITE) == 0 && (prot & PROT_WRITE) != 0)) {
		CHERI_COMPOSITOR_ERROR(sc,
		    "Invalid prot (%u); greater permissions than FD has (%u).",
		    prot, cdev_fd->f_flag);
		return EACCES;
	}

	/* Alignment and bounds checks. */
	if (trunc_page(offset) != offset ||
	    offset + PAGE_SIZE > CHERI_COMPOSITOR_MEM_POOL_LENGTH) {
		CHERI_COMPOSITOR_ERROR(sc,
		    "Invalid offset (%lu); must be page-aligned and within its "
		    "CFB pool.", offset);
		return EINVAL;
	}

	return 0;
}

/* See: old_dev_pager_fault() in device_pager.c as an example. */
static int
cheri_compositor_cfb_pg_fault(vm_object_t vm_obj, vm_ooffset_t offset, int prot,
    vm_page_t *mres)
{
	vm_pindex_t pidx;
	vm_paddr_t paddr;
	vm_page_t page;
	struct cfb_vm_object *cfb_vm_obj;
	struct cdev *dev;
	struct cheri_compositor_softc *sc;
	struct cdevsw *csw;
	vm_memattr_t memattr;
	int ref;
	int retval;

	pidx = OFF_TO_IDX(offset);

	VM_OBJECT_WUNLOCK(vm_obj);

	cfb_vm_obj = vm_obj->handle;
	dev = cfb_vm_obj->dev;
	sc = dev->si_drv1;

	retval = VM_PAGER_OK;

	CHERI_COMPOSITOR_DEBUG(sc, "vm_obj: %p, offset: %lu, prot: %i", vm_obj,
	    offset, prot);

	csw = dev_refthread(dev, &ref);

	if (csw == NULL) {
		retval = VM_PAGER_FAIL;
		goto done_unlocked;
	}

	/* Traditional d_mmap() call. */
	CHERI_COMPOSITOR_DEBUG(sc, "offset: %lu, nprot: %i", offset, prot);

	if (validate_prot_and_offset(sc, cfb_vm_obj->pool->mapped_fd,
	    prot, offset) != 0) {
		retval = VM_PAGER_FAIL;
		goto done_unlocked;
	}

	paddr = calculate_physical_address(sc, cfb_vm_obj->pool, offset);
	memattr = VM_MEMATTR_UNCACHEABLE;

	CHERI_COMPOSITOR_DEBUG(sc, "paddr: %p, memattr: %i",
	    (void *) paddr, memattr);

	dev_relthread(dev, ref);

	/* Sanity checks. */
	KASSERT((((*mres)->flags & PG_FICTITIOUS) == 0),
	    ("Expected non-fictitious page."));

	/*
	 * Replace the passed in reqpage page with our own fake page and
	 * free up the all of the original pages.
	 */
	page = vm_page_getfake(paddr, memattr);
	VM_OBJECT_WLOCK(vm_obj);
	vm_page_lock(*mres);
	vm_page_free(*mres);
	vm_page_unlock(*mres);
	*mres = page;
	vm_page_insert(page, vm_obj, pidx);

	page->valid = VM_PAGE_BITS_ALL;

	/* Success! */
	retval = VM_PAGER_OK;
	goto done;

done_unlocked:
	VM_OBJECT_WLOCK(vm_obj);
done:
	CHERI_COMPOSITOR_DEBUG(sc, "Finished with mres: %p (retval: %i)", *mres,
	    retval);

	return (retval);
}

/* See: old_dev_pager_ctor() in device_pager.c as an example. */
static int
cheri_compositor_cfb_pg_ctor(void *handle, vm_ooffset_t size, vm_prot_t prot,
    vm_ooffset_t foff, struct ucred *cred, u_short *color)
{
	struct cfb_vm_object *cfb_vm_obj;
	struct cdev *dev;
	struct cheri_compositor_softc *sc;
	struct cdevsw *csw;
	vm_ooffset_t top_offset; /* offset of the first byte above the alloc. */
	unsigned int npages;
	int ref;
	int retval = 0;

	cfb_vm_obj = handle;
	dev = cfb_vm_obj->dev;
	sc = dev->si_drv1;

	CHERI_COMPOSITOR_DEBUG(sc,
	    "handle: %p, size: %lu, prot: %i, foff: %lu, cred: %p",
	    handle, size, prot, foff, cred);

	/* Make sure this device can be mapped. */
	csw = dev_refthread(dev, &ref);
	if (csw == NULL) {
		retval = ENXIO;
		goto done_unreffed;
	}

	/* Protection, alignment and bounds checks. */
	npages = OFF_TO_IDX(size);
	top_offset = foff + (npages - 1) * PAGE_SIZE;

	retval = validate_prot_and_offset(sc, cfb_vm_obj->pool->mapped_fd, prot,
	    top_offset);
	if (retval != 0) {
		goto done;
	}

	/* Hold a reference to the device until this mapping is destroyed in
	 * cheri_compositor_cfb_pg_dtor(). */
	dev_ref(dev);

	/* All compositor pages are uncached, so colouring them (to reduce cache
	 * collisions; see
	 * http://docs.freebsd.org/doc/4.4-RELEASE/usr/share/doc/en/articles/vm-design/x103.html)
	 * is pointless. */
	*color = 0;

	/* Success. */
	retval = 0;

done:
	dev_relthread(dev, ref);
done_unreffed:
	CHERI_COMPOSITOR_DEBUG(sc,
	    "Finished with color: %u (retval: %u).", *color, retval);

	return (retval);
}

/* See: old_dev_pager_dtor() in device_pager.c as an example.
 * Called when the VM object is dead (i.e. when the client calls munmap()). We
 * need to free the CFB VM object. If this was the last mapping (i.e. FD) using
 * that pool, we can free the pool and any CFBs in it. */
static void
cheri_compositor_cfb_pg_dtor(void *handle)
{
	struct cfb_vm_object *cfb_vm_obj;
	struct cdev *dev;
	struct cheri_compositor_softc *sc;

	cfb_vm_obj = handle;
	dev = cfb_vm_obj->dev;
	sc = dev->si_drv1;

	CHERI_COMPOSITOR_DEBUG(sc, "handle: %p", handle);

	/* Unref and potentially free the CFB pool and its CFBs. */
	CHERI_COMPOSITOR_LOCK(sc);
	unref_cfb_pool(sc, cfb_vm_obj->pool);
	CHERI_COMPOSITOR_UNLOCK(sc);

	free(cfb_vm_obj, M_CHERI_COMPOSITOR);

	/* Release the mapping's device reference. */
	dev_rel(dev);
}

struct cdev_pager_ops cheri_compositor_cfb_pager_ops = {
	.cdev_pg_fault = cheri_compositor_cfb_pg_fault,
	.cdev_pg_ctor = cheri_compositor_cfb_pg_ctor,
	.cdev_pg_dtor = cheri_compositor_cfb_pg_dtor,
};

/**
 * Get the VM object representing a given memory mapping of the compositor. This
 * gets or allocates a CFB pool corresponding to the FD being used to perform
 * the user's mmap() call. If a new FD is mmap()ped, a new CFB pool is allocated
 * and returned. If the same FD is mmap()ped again, the same CFB pool is
 * returned. Each vm_object corresponds directly with a CFB pool.
 *
 * offset is a guaranteed-page-aligned offset into the FD requested by the user
 * in their call to mmap(). We may modify it.
 * size is a guaranteed-page-rounded size for the mapping as requested by the
 * user in their call to mmap().
 */
static int
cheri_compositor_cfb_mmap_single(struct cdev *dev, vm_ooffset_t *offset,
    vm_size_t size, struct vm_object **obj_res, int nprot)
{
	struct cheri_compositor_softc *sc;
	struct cfb_vm_object *cfb_vm_obj;
	struct vm_object *vm_obj = NULL;
	struct file *cdev_fd;
	struct compositor_cfb_pool *cfb_pool;
	int error;

	sc = dev->si_drv1;

	error = 0;

	CHERI_COMPOSITOR_DEBUG(sc,
	    "dev: %p, offset: %lu, size: %lu, nprot: %i", dev, *offset, size,
	    nprot);

	cdev_fd = curthread->td_fpop;
	KASSERT(cdev_fd != NULL, ("mmap_single td_fpop == NULL"));

	CHERI_COMPOSITOR_DEBUG(sc, "cdev_fd: %p", cdev_fd);

	/* Allocate a CFB VM object to associate the cdev with the CFB pool
	 * mapping. Note: The ordering here is fairly sensitive to changes, as
	 * the cdev_pager_allocate() call results in sub-calls to
	 * cheri_compositor_cfb_pg_fault(), which assumes various fields in the
	 * CFB VM object have been initialised.
	 *
	 * The CFB VM object gets destroyed in
	 * cheri_compositor_cfb_pg_dtor(). */
	cfb_vm_obj =
	    malloc(sizeof(*cfb_vm_obj), M_CHERI_COMPOSITOR, M_WAITOK | M_ZERO);

	CHERI_COMPOSITOR_LOCK(sc);

	/* Find/Allocate a pool mapping for this FD. */
	if (dup_or_allocate_cfb_pool_for_cdev_fd(sc, cdev_fd,
	    NULL /* set later */, &cfb_pool) != 0) {
		free(cfb_vm_obj, M_CHERI_COMPOSITOR);
		error = ENOMEM;
		goto done;
	}

	/* Update the CFB VM object with the pool mapping and cdev. These have
	 * both been referenced, and the references are transferred to the CFB
	 * VM object. */
	cfb_vm_obj->dev = dev;
	cfb_vm_obj->pool = cfb_pool;

	/* If a pool had already been allocated for this FD, re-use it. */
	if (cfb_pool->vm_obj != NULL) {
		vm_object_reference(cfb_pool->vm_obj);
		vm_obj = cfb_pool->vm_obj;
		goto done;
	}

	/* Allocate a device pager VM object. */
	vm_obj = cdev_pager_allocate(cfb_vm_obj, OBJT_DEVICE,
	    &cheri_compositor_cfb_pager_ops, size, nprot,
	    *offset, curthread->td_ucred);

	if (vm_obj == NULL) {
		CHERI_COMPOSITOR_UNLOCK(sc);
		cheri_compositor_cfb_pg_dtor(cfb_vm_obj);
		error = EINVAL;
		goto done_unlocked;
	}

	/* Update the CFB pool to store the VM object. Transfer the reference
	 * from allocation. */
	cfb_pool->vm_obj = vm_obj;

done:
	CHERI_COMPOSITOR_UNLOCK(sc);
done_unlocked:
	CHERI_COMPOSITOR_DEBUG(sc,
	    "Finished with vm_obj: %p, cfb_pool: %p (retval: %u).",
	    vm_obj, cfb_pool, error);

	*obj_res = vm_obj;

	/* Don't need to modify the offset. It was originally passed by the user
	 * as an offset from the start of the cdev FD. Since the cdev FD maps
	 * directly to a CFB pool/VM object, the offset becomes an offset from
	 * the start of the CFB pool/VM object. */

	return (error);
}

int
cheri_compositor_cfb_attach(struct cheri_compositor_softc *sc)
{
	/* Set up the CFB metadata. */
	LIST_INIT(&sc->cfbs);

	/* Set the current resolution (0 until we send a SetConfiguration
	 * command to the hardware. */
	sc->configuration.x_resolution = 0;
	sc->configuration.y_resolution = 0;

	/* Set up command processing. */
	sc->seq_num = 0;

	/* Create the device node. */
	sc->compositor_cfb_cdev = make_dev(&compositor_cfb_cdevsw,
	    sc->compositor_unit,
	    UID_ROOT, GID_WHEEL, 0444 /* world readable/writeable */,
	    "compositor_cfb%d", sc->compositor_unit);
	if (sc->compositor_cfb_cdev == NULL) {
		CHERI_COMPOSITOR_ERROR(sc, "make_dev failed");
		return (ENXIO);
	}

	/* XXXRW: Slight race between make_dev(9) and here. */
	sc->compositor_cfb_cdev->si_drv1 = sc;

	return (0);
}

void
cheri_compositor_cfb_detach(struct cheri_compositor_softc *sc)
{
	if (sc->compositor_cfb_cdev != NULL)
		destroy_dev(sc->compositor_cfb_cdev);
}
