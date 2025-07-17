/*
 * Copyright (c) 2014 Roger Pau Monné <roger.pau@citrix.com>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS AS IS'' AND
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/rwlock.h>
#include <sys/selinfo.h>
#include <sys/poll.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/ioccom.h>
#include <sys/rman.h>
#include <sys/tree.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/bitset.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_pager.h>

#include <machine/md_var.h>

#include <xen/xen-os.h>
#include <xen/hypervisor.h>
#include <xen/privcmd.h>
#include <xen/error.h>

MALLOC_DEFINE(M_PRIVCMD, "privcmd_dev", "Xen privcmd user-space device");

#define MAX_DMOP_BUFFERS 16

struct privcmd_map {
	vm_object_t mem;
	vm_size_t size;
	struct resource *pseudo_phys_res;
	int pseudo_phys_res_id;
	vm_paddr_t phys_base_addr;
	boolean_t mapped;
	BITSET_DEFINE_VAR() *err;
};

static d_ioctl_t     privcmd_ioctl;
static d_open_t      privcmd_open;
static d_mmap_single_t	privcmd_mmap_single;

static struct cdevsw privcmd_devsw = {
	.d_version = D_VERSION,
	.d_ioctl = privcmd_ioctl,
	.d_mmap_single = privcmd_mmap_single,
	.d_open = privcmd_open,
	.d_name = "privcmd",
};

static int privcmd_pg_ctor(void *handle, vm_ooffset_t size, vm_prot_t prot,
    vm_ooffset_t foff, struct ucred *cred, u_short *color);
static void privcmd_pg_dtor(void *handle);
static int privcmd_pg_fault(vm_object_t object, vm_ooffset_t offset,
    int prot, vm_page_t *mres);

static struct cdev_pager_ops privcmd_pg_ops = {
	.cdev_pg_fault = privcmd_pg_fault,
	.cdev_pg_ctor =	privcmd_pg_ctor,
	.cdev_pg_dtor =	privcmd_pg_dtor,
};

struct per_user_data {
	domid_t dom;
};

static device_t privcmd_dev = NULL;

/*------------------------- Privcmd Pager functions --------------------------*/
static int
privcmd_pg_ctor(void *handle, vm_ooffset_t size, vm_prot_t prot,
    vm_ooffset_t foff, struct ucred *cred, u_short *color)
{

	return (0);
}

static void
privcmd_pg_dtor(void *handle)
{
	struct xen_remove_from_physmap rm = { .domid = DOMID_SELF };
	struct privcmd_map *map = handle;
	int error __diagused;
	vm_size_t i;

	/*
	 * Remove the mappings from the used pages. This will remove the
	 * underlying p2m bindings in Xen second stage translation.
	 */
	if (map->mapped == true) {
		cdev_mgtdev_pager_free_pages(map->mem);
		for (i = 0; i < map->size; i++) {
			rm.gpfn = atop(map->phys_base_addr) + i;
			HYPERVISOR_memory_op(XENMEM_remove_from_physmap, &rm);
		}
		free(map->err, M_PRIVCMD);
	}

	error = xenmem_free(privcmd_dev, map->pseudo_phys_res_id,
	    map->pseudo_phys_res);
	KASSERT(error == 0, ("Unable to release memory resource: %d", error));

	free(map, M_PRIVCMD);
}

static int
privcmd_pg_fault(vm_object_t object, vm_ooffset_t offset,
    int prot, vm_page_t *mres)
{
	struct privcmd_map *map = object->handle;
	vm_pindex_t pidx;
	vm_page_t page;

	if (map->mapped != true)
		return (VM_PAGER_FAIL);

	pidx = OFF_TO_IDX(offset);
	if (pidx >= map->size || BIT_ISSET(map->size, pidx, map->err))
		return (VM_PAGER_FAIL);

	page = PHYS_TO_VM_PAGE(map->phys_base_addr + offset);
	if (page == NULL)
		return (VM_PAGER_FAIL);

	KASSERT((page->flags & PG_FICTITIOUS) != 0,
	    ("not fictitious %p", page));
	KASSERT(vm_page_wired(page), ("page %p not wired", page));
	KASSERT(!vm_page_busied(page), ("page %p is busy", page));

	vm_page_busy_acquire(page, 0);
	vm_page_valid(page);

	if (*mres != NULL)
		vm_page_replace(page, object, pidx, *mres);
	else
		vm_page_insert(page, object, pidx);
	*mres = page;
	return (VM_PAGER_OK);
}

/*----------------------- Privcmd char device methods ------------------------*/
static int
privcmd_mmap_single(struct cdev *cdev, vm_ooffset_t *offset, vm_size_t size,
    vm_object_t *object, int nprot)
{
	struct privcmd_map *map;

	map = malloc(sizeof(*map), M_PRIVCMD, M_WAITOK | M_ZERO);

	map->size = OFF_TO_IDX(size);
	map->pseudo_phys_res_id = 0;

	map->pseudo_phys_res = xenmem_alloc(privcmd_dev,
	    &map->pseudo_phys_res_id, size);
	if (map->pseudo_phys_res == NULL) {
		free(map, M_PRIVCMD);
		return (ENOMEM);
	}

	map->phys_base_addr = rman_get_start(map->pseudo_phys_res);
	map->mem = cdev_pager_allocate(map, OBJT_MGTDEVICE, &privcmd_pg_ops,
	    size, nprot, *offset, NULL);
	if (map->mem == NULL) {
		xenmem_free(privcmd_dev, map->pseudo_phys_res_id,
		    map->pseudo_phys_res);
		free(map, M_PRIVCMD);
		return (ENOMEM);
	}

	*object = map->mem;

	return (0);
}

static struct privcmd_map *
setup_virtual_area(struct thread *td, unsigned long addr, unsigned long num)
{
	vm_map_t map;
	vm_map_entry_t entry;
	vm_object_t mem;
	vm_pindex_t pindex;
	vm_prot_t prot;
	boolean_t wired;
	struct privcmd_map *umap;
	int error;

	if ((num == 0) || ((addr & PAGE_MASK) != 0))
		return NULL;

	map = &td->td_proc->p_vmspace->vm_map;
	error = vm_map_lookup(&map, addr, VM_PROT_NONE, &entry, &mem, &pindex,
	    &prot, &wired);
	if (error != KERN_SUCCESS || (entry->start != addr) ||
	    (entry->end != addr + (num * PAGE_SIZE)))
		return NULL;

	vm_map_lookup_done(map, entry);
	if ((mem->type != OBJT_MGTDEVICE) ||
	    (mem->un_pager.devp.ops != &privcmd_pg_ops))
		return NULL;

	umap = mem->handle;
	/* Allocate a bitset to store broken page mappings. */
	umap->err = BITSET_ALLOC(num, M_PRIVCMD, M_WAITOK | M_ZERO);

	return umap;
}

static int
privcmd_ioctl(struct cdev *dev, unsigned long cmd, caddr_t arg,
	      int mode, struct thread *td)
{
	int error;
	unsigned int i;
	void *data;
	const struct per_user_data *u;

	error = devfs_get_cdevpriv(&data);
	if (error != 0)
		return (EINVAL);
	/*
	 * Constify user-data to prevent unintended changes to the restriction
	 * limits.
	 */
	u = data;

	switch (cmd) {
	case IOCTL_PRIVCMD_HYPERCALL: {
		struct ioctl_privcmd_hypercall *hcall;

		hcall = (struct ioctl_privcmd_hypercall *)arg;

		/* Forbid hypercalls if restricted. */
		if (u->dom != DOMID_INVALID) {
			error = EPERM;
			break;
		}

#ifdef __amd64__
		/*
		 * The hypervisor page table walker will refuse to access
		 * user-space pages if SMAP is enabled, so temporary disable it
		 * while performing the hypercall.
		 */
		if (cpu_stdext_feature & CPUID_STDEXT_SMAP)
			stac();
#endif
		error = privcmd_hypercall(hcall->op, hcall->arg[0],
		    hcall->arg[1], hcall->arg[2], hcall->arg[3], hcall->arg[4]);
#ifdef __amd64__
		if (cpu_stdext_feature & CPUID_STDEXT_SMAP)
			clac();
#endif
		if (error >= 0) {
			hcall->retval = error;
			error = 0;
		} else {
			error = xen_translate_error(error);
			hcall->retval = 0;
		}
		break;
	}
	case IOCTL_PRIVCMD_MMAPBATCH: {
		struct ioctl_privcmd_mmapbatch *mmap;
		struct xen_add_to_physmap_batch add;
		xen_ulong_t *idxs;
		xen_pfn_t *gpfns;
		int *errs;
		unsigned int index;
		struct privcmd_map *umap;
		uint16_t num;

		mmap = (struct ioctl_privcmd_mmapbatch *)arg;

		if (u->dom != DOMID_INVALID && u->dom != mmap->dom) {
			error = EPERM;
			break;
		}

		umap = setup_virtual_area(td, mmap->addr, mmap->num);
		if (umap == NULL) {
			error = EINVAL;
			break;
		}

		add.domid = DOMID_SELF;
		add.space = XENMAPSPACE_gmfn_foreign;
		add.u.foreign_domid = mmap->dom;

		/*
		 * The 'size' field in the xen_add_to_physmap_range only
		 * allows for UINT16_MAX mappings in a single hypercall.
		 */
		num = MIN(mmap->num, UINT16_MAX);

		idxs = malloc(sizeof(*idxs) * num, M_PRIVCMD, M_WAITOK);
		gpfns = malloc(sizeof(*gpfns) * num, M_PRIVCMD, M_WAITOK);
		errs = malloc(sizeof(*errs) * num, M_PRIVCMD, M_WAITOK);

		set_xen_guest_handle(add.idxs, idxs);
		set_xen_guest_handle(add.gpfns, gpfns);
		set_xen_guest_handle(add.errs, errs);

		for (index = 0; index < mmap->num; index += num) {
			num = MIN(mmap->num - index, UINT16_MAX);
			add.size = num;

			error = copyin(&mmap->arr[index], idxs,
			    sizeof(idxs[0]) * num);
			if (error != 0)
				goto mmap_out;

			for (i = 0; i < num; i++)
				gpfns[i] = atop(umap->phys_base_addr +
				    (i + index) * PAGE_SIZE);

			bzero(errs, sizeof(*errs) * num);

			error = HYPERVISOR_memory_op(
			    XENMEM_add_to_physmap_batch, &add);
			if (error != 0) {
				error = xen_translate_error(error);
				goto mmap_out;
			}

			for (i = 0; i < num; i++) {
				if (errs[i] != 0) {
					errs[i] = xen_translate_error(errs[i]);

					/* Mark the page as invalid. */
					BIT_SET(mmap->num, index + i,
					    umap->err);
				}
			}

			error = copyout(errs, &mmap->err[index],
			    sizeof(errs[0]) * num);
			if (error != 0)
				goto mmap_out;
		}

		umap->mapped = true;

mmap_out:
		free(idxs, M_PRIVCMD);
		free(gpfns, M_PRIVCMD);
		free(errs, M_PRIVCMD);
		if (!umap->mapped)
			free(umap->err, M_PRIVCMD);

		break;
	}
	case IOCTL_PRIVCMD_MMAP_RESOURCE: {
		struct ioctl_privcmd_mmapresource *mmap;
		struct xen_mem_acquire_resource adq;
		xen_pfn_t *gpfns;
		struct privcmd_map *umap;

		mmap = (struct ioctl_privcmd_mmapresource *)arg;

		if (u->dom != DOMID_INVALID && u->dom != mmap->dom) {
			error = EPERM;
			break;
		}

		bzero(&adq, sizeof(adq));

		adq.domid = mmap->dom;
		adq.type = mmap->type;
		adq.id = mmap->id;

		/* Shortcut for getting the resource size. */
		if (mmap->addr == 0 && mmap->num == 0) {
			error = HYPERVISOR_memory_op(XENMEM_acquire_resource,
			    &adq);
			if (error != 0)
				error = xen_translate_error(error);
			else
				mmap->num = adq.nr_frames;
			break;
		}

		umap = setup_virtual_area(td, mmap->addr, mmap->num);
		if (umap == NULL) {
			error = EINVAL;
			break;
		}

		adq.nr_frames = mmap->num;
		adq.frame = mmap->idx;

		gpfns = malloc(sizeof(*gpfns) * mmap->num, M_PRIVCMD, M_WAITOK);
		for (i = 0; i < mmap->num; i++)
			gpfns[i] = atop(umap->phys_base_addr) + i;
		set_xen_guest_handle(adq.frame_list, gpfns);

		error = HYPERVISOR_memory_op(XENMEM_acquire_resource, &adq);
		if (error != 0)
			error = xen_translate_error(error);
		else
			umap->mapped = true;

		free(gpfns, M_PRIVCMD);
		if (!umap->mapped)
			free(umap->err, M_PRIVCMD);

		break;
	}
	case IOCTL_PRIVCMD_DM_OP: {
		const struct ioctl_privcmd_dmop *dmop;
		struct privcmd_dmop_buf *bufs;
		struct xen_dm_op_buf *hbufs;

		dmop = (struct ioctl_privcmd_dmop *)arg;

		if (u->dom != DOMID_INVALID && u->dom != dmop->dom) {
			error = EPERM;
			break;
		}

		if (dmop->num == 0)
			break;

		if (dmop->num > MAX_DMOP_BUFFERS) {
			error = E2BIG;
			break;
		}

		bufs = malloc(sizeof(*bufs) * dmop->num, M_PRIVCMD, M_WAITOK);

		error = copyin(dmop->ubufs, bufs, sizeof(*bufs) * dmop->num);
		if (error != 0) {
			free(bufs, M_PRIVCMD);
			break;
		}

		hbufs = malloc(sizeof(*hbufs) * dmop->num, M_PRIVCMD, M_WAITOK);
		for (i = 0; i < dmop->num; i++) {
			set_xen_guest_handle(hbufs[i].h, bufs[i].uptr);
			hbufs[i].size = bufs[i].size;
		}

#ifdef __amd64__
		if (cpu_stdext_feature & CPUID_STDEXT_SMAP)
			stac();
#endif
		error = HYPERVISOR_dm_op(dmop->dom, dmop->num, hbufs);
#ifdef __amd64__
		if (cpu_stdext_feature & CPUID_STDEXT_SMAP)
			clac();
#endif
		if (error != 0)
			error = xen_translate_error(error);

		free(bufs, M_PRIVCMD);
		free(hbufs, M_PRIVCMD);


		break;
	}
	case IOCTL_PRIVCMD_RESTRICT: {
		struct per_user_data *u;
		domid_t dom;

		dom = *(domid_t *)arg;

		error = devfs_get_cdevpriv((void **)&u);
		if (error != 0)
			break;

		if (u->dom != DOMID_INVALID && u->dom != dom) {
			error = -EINVAL;
			break;
		}
		u->dom = dom;

		break;
	}
	default:
		error = ENOSYS;
		break;
	}

	return (error);
}

static void
user_release(void *arg)
{

	free(arg, M_PRIVCMD);
}

static int
privcmd_open(struct cdev *dev, int flag, int otyp, struct thread *td)
{
	struct per_user_data *u;
	int error;

	u = malloc(sizeof(*u), M_PRIVCMD, M_WAITOK);
	u->dom = DOMID_INVALID;

	/* Assign the allocated per_user_data to this open instance. */
	error = devfs_set_cdevpriv(u, user_release);
	if (error != 0) {
		free(u, M_PRIVCMD);
	}

	return (error);
}

/*------------------ Private Device Attachment Functions  --------------------*/
static void
privcmd_identify(driver_t *driver, device_t parent)
{

	KASSERT(xen_domain(),
	    ("Trying to attach privcmd device on non Xen domain"));

	if (BUS_ADD_CHILD(parent, 0, "privcmd", 0) == NULL)
		panic("unable to attach privcmd user-space device");
}

static int
privcmd_probe(device_t dev)
{

	privcmd_dev = dev;
	device_set_desc(dev, "Xen privileged interface user-space device");
	return (BUS_PROBE_NOWILDCARD);
}

static int
privcmd_attach(device_t dev)
{

	make_dev_credf(MAKEDEV_ETERNAL, &privcmd_devsw, 0, NULL, UID_ROOT,
	    GID_WHEEL, 0600, "xen/privcmd");
	return (0);
}

/*-------------------- Private Device Attachment Data  -----------------------*/
static device_method_t privcmd_methods[] = {
	DEVMETHOD(device_identify,	privcmd_identify),
	DEVMETHOD(device_probe,		privcmd_probe),
	DEVMETHOD(device_attach,	privcmd_attach),

	DEVMETHOD_END
};

static driver_t privcmd_driver = {
	"privcmd",
	privcmd_methods,
	0,
};

DRIVER_MODULE(privcmd, xenpv, privcmd_driver, 0, 0);
MODULE_DEPEND(privcmd, xenpv, 1, 1, 1);
