/*-
 * Copyright (c) 2017, 2018 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Konstantin Belousov <kib@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#include "opt_acpi.h"
#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/devicestat.h>
#include <sys/disk.h>
#include <sys/efi.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/rwlock.h>
#include <sys/sglist.h>
#include <sys/uio.h>
#include <sys/uuid.h>
#include <geom/geom.h>
#include <geom/geom_int.h>
#include <machine/vmparam.h>
#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/acuuid.h>
#include <dev/acpica/acpivar.h>
#include <dev/nvdimm/nvdimm_var.h>

struct SPA_mapping *spa_mappings;
int spa_mappings_cnt;

static int
nvdimm_spa_count(void *nfitsubtbl __unused, void *arg)
{
	int *cnt;

	cnt = arg;
	(*cnt)++;
	return (0);
}

static struct nvdimm_SPA_uuid_list_elm {
	const char		*u_name;
	const char		*u_id_str;
	struct uuid		u_id;
	const bool		u_usr_acc;
} nvdimm_SPA_uuid_list[] = {
	[SPA_TYPE_VOLATILE_MEMORY] = {
		.u_name =	"VOLA MEM ",
		.u_id_str =	UUID_VOLATILE_MEMORY,
		.u_usr_acc =	true,
	},
	[SPA_TYPE_PERSISTENT_MEMORY] = {
		.u_name =	"PERS MEM",
		.u_id_str =	UUID_PERSISTENT_MEMORY,
		.u_usr_acc =	true,
	},
	[SPA_TYPE_CONTROL_REGION] = {
		.u_name =	"CTRL RG ",
		.u_id_str =	UUID_CONTROL_REGION,
		.u_usr_acc =	false,
	},
	[SPA_TYPE_DATA_REGION] = {
		.u_name =	"DATA RG ",
		.u_id_str =	UUID_DATA_REGION,
		.u_usr_acc =	true,
	},
	[SPA_TYPE_VOLATILE_VIRTUAL_DISK] = {
		.u_name =	"VIRT DSK",
		.u_id_str =	UUID_VOLATILE_VIRTUAL_DISK,
		.u_usr_acc =	true,
	},
	[SPA_TYPE_VOLATILE_VIRTUAL_CD] = {
		.u_name =	"VIRT CD ",
		.u_id_str =	UUID_VOLATILE_VIRTUAL_CD,
		.u_usr_acc =	true,
	},
	[SPA_TYPE_PERSISTENT_VIRTUAL_DISK] = {
		.u_name =	"PV DSK  ",
		.u_id_str =	UUID_PERSISTENT_VIRTUAL_DISK,
		.u_usr_acc =	true,
	},
	[SPA_TYPE_PERSISTENT_VIRTUAL_CD] = {
		.u_name =	"PV CD   ",
		.u_id_str =	UUID_PERSISTENT_VIRTUAL_CD,
		.u_usr_acc =	true,
	},
};

static vm_memattr_t
nvdimm_spa_memattr(struct SPA_mapping *spa)
{
	vm_memattr_t mode;

	if ((spa->spa_efi_mem_flags & EFI_MD_ATTR_WB) != 0)
		mode = VM_MEMATTR_WRITE_BACK;
	else if ((spa->spa_efi_mem_flags & EFI_MD_ATTR_WT) != 0)
		mode = VM_MEMATTR_WRITE_THROUGH;
	else if ((spa->spa_efi_mem_flags & EFI_MD_ATTR_WC) != 0)
		mode = VM_MEMATTR_WRITE_COMBINING;
	else if ((spa->spa_efi_mem_flags & EFI_MD_ATTR_WP) != 0)
		mode = VM_MEMATTR_WRITE_PROTECTED;
	else if ((spa->spa_efi_mem_flags & EFI_MD_ATTR_UC) != 0)
		mode = VM_MEMATTR_UNCACHEABLE;
	else {
		if (bootverbose)
			printf("SPA%d mapping attr unsupported\n",
			    spa->spa_nfit_idx);
		mode = VM_MEMATTR_UNCACHEABLE;
	}
	return (mode);
}

static int
nvdimm_spa_uio(struct SPA_mapping *spa, struct uio *uio)
{
	struct vm_page m, *ma;
	off_t off;
	vm_memattr_t mattr;
	int error, n;

	if (spa->spa_kva == NULL) {
		mattr = nvdimm_spa_memattr(spa);
		vm_page_initfake(&m, 0, mattr);
		ma = &m;
		while (uio->uio_resid > 0) {
			if (uio->uio_offset >= spa->spa_len)
				break;
			off = spa->spa_phys_base + uio->uio_offset;
			vm_page_updatefake(&m, trunc_page(off), mattr);
			n = PAGE_SIZE;
			if (n > uio->uio_resid)
				n = uio->uio_resid;
			error = uiomove_fromphys(&ma, off & PAGE_MASK, n, uio);
			if (error != 0)
				break;
		}
	} else {
		while (uio->uio_resid > 0) {
			if (uio->uio_offset >= spa->spa_len)
				break;
			n = INT_MAX;
			if (n > uio->uio_resid)
				n = uio->uio_resid;
			if (uio->uio_offset + n > spa->spa_len)
				n = spa->spa_len - uio->uio_offset;
			error = uiomove((char *)spa->spa_kva + uio->uio_offset,
			    n, uio);
			if (error != 0)
				break;
		}
	}
	return (error);
}

static int
nvdimm_spa_rw(struct cdev *dev, struct uio *uio, int ioflag)
{

	return (nvdimm_spa_uio(dev->si_drv1, uio));
}

static int
nvdimm_spa_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int fflag,
    struct thread *td)
{
	struct SPA_mapping *spa;
	int error;

	spa = dev->si_drv1;
	error = 0;
	switch (cmd) {
	case DIOCGSECTORSIZE:
		*(u_int *)data = DEV_BSIZE;
		break;
	case DIOCGMEDIASIZE:
		*(off_t *)data = spa->spa_len;
		break;
	default:
		error = ENOTTY;
		break;
	}
	return (error);
}

static int
nvdimm_spa_mmap_single(struct cdev *dev, vm_ooffset_t *offset, vm_size_t size,
    vm_object_t *objp, int nprot)
{
	struct SPA_mapping *spa;

	spa = dev->si_drv1;
	if (spa->spa_obj == NULL)
		return (ENXIO);
	if (*offset >= spa->spa_len || *offset + size < *offset ||
	    *offset + size > spa->spa_len)
		return (EINVAL);
	vm_object_reference(spa->spa_obj);
	*objp = spa->spa_obj;
	return (0);
}

static struct cdevsw spa_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	D_DISK,
	.d_name =	"nvdimm_spa",
	.d_read =	nvdimm_spa_rw,
	.d_write =	nvdimm_spa_rw,
	.d_ioctl =	nvdimm_spa_ioctl,
	.d_mmap_single = nvdimm_spa_mmap_single,
};

static void
nvdimm_spa_g_all_unmapped(struct SPA_mapping *spa, struct bio *bp,
    int rw)
{
	struct vm_page maa[bp->bio_ma_n];
	vm_page_t ma[bp->bio_ma_n];
	vm_memattr_t mattr;
	int i;

	mattr = nvdimm_spa_memattr(spa);
	for (i = 0; i < nitems(ma); i++) {
		maa[i].flags = 0;
		vm_page_initfake(&maa[i], spa->spa_phys_base +
		    trunc_page(bp->bio_offset) + PAGE_SIZE * i, mattr);
		ma[i] = &maa[i];
	}
	if (rw == BIO_READ)
		pmap_copy_pages(ma, bp->bio_offset & PAGE_MASK, bp->bio_ma,
		    bp->bio_ma_offset, bp->bio_length);
	else
		pmap_copy_pages(bp->bio_ma, bp->bio_ma_offset, ma,
		    bp->bio_offset & PAGE_MASK, bp->bio_length);
}

static void
nvdimm_spa_g_thread(void *arg)
{
	struct SPA_mapping *spa;
	struct bio *bp;
	struct uio auio;
	struct iovec aiovec;
	int error;

	spa = arg;
	for (;;) {
		mtx_lock(&spa->spa_g_mtx);
		for (;;) {
			bp = bioq_takefirst(&spa->spa_g_queue);
			if (bp != NULL)
				break;
			msleep(&spa->spa_g_queue, &spa->spa_g_mtx, PRIBIO,
			    "spa_g", 0);
			if (!spa->spa_g_proc_run) {
				spa->spa_g_proc_exiting = true;
				wakeup(&spa->spa_g_queue);
				mtx_unlock(&spa->spa_g_mtx);
				kproc_exit(0);
			}
			continue;
		}
		mtx_unlock(&spa->spa_g_mtx);
		if (bp->bio_cmd != BIO_READ && bp->bio_cmd != BIO_WRITE &&
		    bp->bio_cmd != BIO_FLUSH) {
			error = EOPNOTSUPP;
			goto completed;
		}

		error = 0;
		if (bp->bio_cmd == BIO_FLUSH) {
			if (spa->spa_kva != NULL) {
				pmap_large_map_wb(spa->spa_kva, spa->spa_len);
			} else {
				pmap_flush_cache_phys_range(
				    (vm_paddr_t)spa->spa_phys_base,
				    (vm_paddr_t)spa->spa_phys_base +
				    spa->spa_len, nvdimm_spa_memattr(spa));
			}
			/*
			 * XXX flush IMC
			 */
			goto completed;
		}
		
		if ((bp->bio_flags & BIO_UNMAPPED) != 0) {
			if (spa->spa_kva != NULL) {
				aiovec.iov_base = (char *)spa->spa_kva +
				    bp->bio_offset;
				aiovec.iov_len = bp->bio_length;
				auio.uio_iov = &aiovec;
				auio.uio_iovcnt = 1;
				auio.uio_resid = bp->bio_length;
				auio.uio_offset = bp->bio_offset;
				auio.uio_segflg = UIO_SYSSPACE;
				auio.uio_rw = bp->bio_cmd == BIO_READ ?
				    UIO_WRITE : UIO_READ;
				auio.uio_td = curthread;
				error = uiomove_fromphys(bp->bio_ma,
				    bp->bio_ma_offset, bp->bio_length, &auio);
			} else {
				nvdimm_spa_g_all_unmapped(spa, bp, bp->bio_cmd);
				error = 0;
			}
		} else {
			aiovec.iov_base = bp->bio_data;
			aiovec.iov_len = bp->bio_length;
			auio.uio_iov = &aiovec;
			auio.uio_iovcnt = 1;
			auio.uio_resid = bp->bio_length;
			auio.uio_offset = bp->bio_offset;
			auio.uio_segflg = UIO_SYSSPACE;
			auio.uio_rw = bp->bio_cmd == BIO_READ ? UIO_READ :
			    UIO_WRITE;
			auio.uio_td = curthread;
			error = nvdimm_spa_uio(spa, &auio);
		}
		devstat_end_transaction_bio(spa->spa_g_devstat, bp);
completed:
		bp->bio_completed = bp->bio_length;
		g_io_deliver(bp, error);
	}
}

static void
nvdimm_spa_g_start(struct bio *bp)
{
	struct SPA_mapping *spa;

	spa = bp->bio_to->geom->softc;
	if (bp->bio_cmd == BIO_READ || bp->bio_cmd == BIO_WRITE) {
		mtx_lock(&spa->spa_g_stat_mtx);
		devstat_start_transaction_bio(spa->spa_g_devstat, bp);
		mtx_unlock(&spa->spa_g_stat_mtx);
	}
	mtx_lock(&spa->spa_g_mtx);
	bioq_disksort(&spa->spa_g_queue, bp);
	wakeup(&spa->spa_g_queue);
	mtx_unlock(&spa->spa_g_mtx);
}

static int
nvdimm_spa_g_access(struct g_provider *pp, int r, int w, int e)
{

	return (0);
}

static g_init_t nvdimm_spa_g_init;
static g_fini_t nvdimm_spa_g_fini;

struct g_class nvdimm_spa_g_class = {
	.name =		"SPA",
	.version =	G_VERSION,
	.start =	nvdimm_spa_g_start,
	.access =	nvdimm_spa_g_access,
	.init =		nvdimm_spa_g_init,
	.fini =		nvdimm_spa_g_fini,
};
DECLARE_GEOM_CLASS(nvdimm_spa_g_class, g_spa);

static int
nvdimm_spa_init_one(struct SPA_mapping *spa, ACPI_NFIT_SYSTEM_ADDRESS *nfitaddr,
    int spa_type)
{
	struct make_dev_args mda;
	struct sglist *spa_sg;
	int error, error1;

	spa->spa_type = spa_type;
	spa->spa_domain = ((nfitaddr->Flags & ACPI_NFIT_PROXIMITY_VALID) != 0) ?
	    nfitaddr->ProximityDomain : -1;
	spa->spa_nfit_idx = nfitaddr->RangeIndex;
	spa->spa_phys_base = nfitaddr->Address;
	spa->spa_len = nfitaddr->Length;
	spa->spa_efi_mem_flags = nfitaddr->MemoryMapping;
	if (bootverbose) {
		printf("NVDIMM SPA%d base %#016jx len %#016jx %s fl %#jx\n",
		    spa->spa_nfit_idx,
		    (uintmax_t)spa->spa_phys_base, (uintmax_t)spa->spa_len,
		    nvdimm_SPA_uuid_list[spa_type].u_name,
		    spa->spa_efi_mem_flags);
	}
	if (!nvdimm_SPA_uuid_list[spa_type].u_usr_acc)
		return (0);

	error1 = pmap_large_map(spa->spa_phys_base, spa->spa_len,
	    &spa->spa_kva, nvdimm_spa_memattr(spa));
	if (error1 != 0) {
		printf("NVDIMM SPA%d cannot map into KVA, error %d\n",
		    spa->spa_nfit_idx, error1);
		spa->spa_kva = NULL;
	}

	spa_sg = sglist_alloc(1, M_WAITOK);
	error = sglist_append_phys(spa_sg, spa->spa_phys_base,
	    spa->spa_len);
	if (error == 0) {
		spa->spa_obj = vm_pager_allocate(OBJT_SG, spa_sg, spa->spa_len,
		    VM_PROT_ALL, 0, NULL);
		if (spa->spa_obj == NULL) {
			printf("NVDIMM SPA%d failed to alloc vm object",
			    spa->spa_nfit_idx);
			sglist_free(spa_sg);
		}
	} else {
		printf("NVDIMM SPA%d failed to init sglist, error %d",
		    spa->spa_nfit_idx, error);
		sglist_free(spa_sg);
	}

	make_dev_args_init(&mda);
	mda.mda_flags = MAKEDEV_WAITOK | MAKEDEV_CHECKNAME;
	mda.mda_devsw = &spa_cdevsw;
	mda.mda_cr = NULL;
	mda.mda_uid = UID_ROOT;
	mda.mda_gid = GID_OPERATOR;
	mda.mda_mode = 0660;
	mda.mda_si_drv1 = spa;
	error = make_dev_s(&mda, &spa->spa_dev, "nvdimm_spa%d",
	    spa->spa_nfit_idx);
	if (error != 0) {
		printf("NVDIMM SPA%d cannot create devfs node, error %d\n",
		    spa->spa_nfit_idx, error);
		if (error1 == 0)
			error1 = error;
	}

	bioq_init(&spa->spa_g_queue);
	mtx_init(&spa->spa_g_mtx, "spag", NULL, MTX_DEF);
	mtx_init(&spa->spa_g_stat_mtx, "spagst", NULL, MTX_DEF);
	spa->spa_g_proc_run = true;
	spa->spa_g_proc_exiting = false;
	error = kproc_create(nvdimm_spa_g_thread, spa, &spa->spa_g_proc, 0, 0,
	    "g_spa%d", spa->spa_nfit_idx);
	if (error != 0) {
		printf("NVDIMM SPA%d cannot create geom worker, error %d\n",
		    spa->spa_nfit_idx, error);
		if (error1 == 0)
			error1 = error;
	} else {
		g_topology_assert();
		spa->spa_g = g_new_geomf(&nvdimm_spa_g_class, "spa%d",
		    spa->spa_nfit_idx);
		spa->spa_g->softc = spa;
		spa->spa_p = g_new_providerf(spa->spa_g, "spa%d",
		    spa->spa_nfit_idx);
		spa->spa_p->mediasize = spa->spa_len;
		spa->spa_p->sectorsize = DEV_BSIZE;
		spa->spa_p->flags |= G_PF_DIRECT_SEND | G_PF_DIRECT_RECEIVE |
		    G_PF_ACCEPT_UNMAPPED;
		g_error_provider(spa->spa_p, 0);
		spa->spa_g_devstat = devstat_new_entry("spa", spa->spa_nfit_idx,
		    DEV_BSIZE, DEVSTAT_ALL_SUPPORTED, DEVSTAT_TYPE_DIRECT,
		    DEVSTAT_PRIORITY_MAX);
	}
	return (error1);
}

static void
nvdimm_spa_fini_one(struct SPA_mapping *spa)
{

	mtx_lock(&spa->spa_g_mtx);
	spa->spa_g_proc_run = false;
	wakeup(&spa->spa_g_queue);
	while (!spa->spa_g_proc_exiting)
		msleep(&spa->spa_g_queue, &spa->spa_g_mtx, PRIBIO, "spa_e", 0);
	mtx_unlock(&spa->spa_g_mtx);
	if (spa->spa_g != NULL) {
		g_topology_lock();
		g_wither_geom(spa->spa_g, ENXIO);
		g_topology_unlock();
		spa->spa_g = NULL;
		spa->spa_p = NULL;
	}
	if (spa->spa_g_devstat != NULL) {
		devstat_remove_entry(spa->spa_g_devstat);
		spa->spa_g_devstat = NULL;
	}
	if (spa->spa_dev != NULL) {
		destroy_dev(spa->spa_dev);
		spa->spa_dev = NULL;
	}
	vm_object_deallocate(spa->spa_obj);
	if (spa->spa_kva != NULL) {
		pmap_large_unmap(spa->spa_kva, spa->spa_len);
		spa->spa_kva = NULL;
	}
	mtx_destroy(&spa->spa_g_mtx);
	mtx_destroy(&spa->spa_g_stat_mtx);
}

static int
nvdimm_spa_parse(void *nfitsubtbl, void *arg)
{
	ACPI_NFIT_SYSTEM_ADDRESS *nfitaddr;
	struct SPA_mapping *spa;
	int error, *i, j;

	i = arg;
	spa = &spa_mappings[*i];
	nfitaddr = nfitsubtbl;

	for (j = 0; j < nitems(nvdimm_SPA_uuid_list); j++) {
		/* XXXKIB: is ACPI UUID representation compatible ? */
		if (uuidcmp((struct uuid *)&nfitaddr->RangeGuid,
		    &nvdimm_SPA_uuid_list[j].u_id) != 0)
			continue;
		error = nvdimm_spa_init_one(spa, nfitaddr, j);
		if (error != 0)
			nvdimm_spa_fini_one(spa);
		break;
	}
	if (j == nitems(nvdimm_SPA_uuid_list) && bootverbose) {
		printf("Unknown SPA UUID %d ", nfitaddr->RangeIndex);
		printf_uuid((struct uuid *)&nfitaddr->RangeGuid);
		printf("\n");
	}
	(*i)++;
	return (0);
}

static int
nvdimm_spa_init1(ACPI_TABLE_NFIT *nfitbl)
{
	struct nvdimm_SPA_uuid_list_elm *sle;
	int error, i;

	for (i = 0; i < nitems(nvdimm_SPA_uuid_list); i++) {
		sle = &nvdimm_SPA_uuid_list[i];
		error = parse_uuid(sle->u_id_str, &sle->u_id);
		if (error != 0) {
			if (bootverbose)
				printf("nvdimm_identify: error %d parsing "
				    "known SPA UUID %d %s\n", error, i,
				    sle->u_id_str);
			return (error);
		}
	}

	error = nvdimm_iterate_nfit(nfitbl, ACPI_NFIT_TYPE_SYSTEM_ADDRESS,
	    nvdimm_spa_count, &spa_mappings_cnt);
	if (error != 0)
		return (error);
	spa_mappings = malloc(sizeof(struct SPA_mapping) * spa_mappings_cnt,
	    M_NVDIMM, M_WAITOK | M_ZERO);
	i = 0;
	error = nvdimm_iterate_nfit(nfitbl, ACPI_NFIT_TYPE_SYSTEM_ADDRESS,
	    nvdimm_spa_parse, &i);
	if (error != 0) {
		free(spa_mappings, M_NVDIMM);
		spa_mappings = NULL;
		return (error);
	}
	return (0);
}

static void
nvdimm_spa_g_init(struct g_class *mp __unused)
{
	ACPI_TABLE_NFIT *nfitbl;
	ACPI_STATUS status;
	int error;

	spa_mappings_cnt = 0;
	spa_mappings = NULL;
	if (acpi_disabled("nvdimm"))
		return;
	status = AcpiGetTable(ACPI_SIG_NFIT, 1, (ACPI_TABLE_HEADER **)&nfitbl);
	if (ACPI_FAILURE(status)) {
		if (bootverbose)
			printf("nvdimm_spa_g_init: cannot find NFIT\n");
		return;
	}
	error = nvdimm_spa_init1(nfitbl);
	if (error != 0)
		printf("nvdimm_spa_g_init: error %d\n", error);
	AcpiPutTable(&nfitbl->Header);
}

static void
nvdimm_spa_g_fini(struct g_class *mp __unused)
{
	int i;

	if (spa_mappings == NULL)
		return;
	for (i = 0; i < spa_mappings_cnt; i++)
		nvdimm_spa_fini_one(&spa_mappings[i]);
	free(spa_mappings, M_NVDIMM);
	spa_mappings = NULL;
	spa_mappings_cnt = 0;
}
