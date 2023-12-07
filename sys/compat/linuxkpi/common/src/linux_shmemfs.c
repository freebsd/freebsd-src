/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2016 Matthew Macy (mmacy@mattmacy.io)
 * Copyright (c) 2017 Mellanox Technologies, Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/rwlock.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_object.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>

#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/shmem_fs.h>

struct page *
linux_shmem_read_mapping_page_gfp(vm_object_t obj, int pindex, gfp_t gfp)
{
	struct page *page;
	int rv;

	if ((gfp & GFP_NOWAIT) != 0)
		panic("GFP_NOWAIT is unimplemented");

	VM_OBJECT_WLOCK(obj);
	rv = vm_page_grab_valid(&page, obj, pindex, VM_ALLOC_NORMAL |
	    VM_ALLOC_NOBUSY | VM_ALLOC_WIRED);
	VM_OBJECT_WUNLOCK(obj);
	if (rv != VM_PAGER_OK)
		return (ERR_PTR(-EINVAL));
	return (page);
}

struct linux_file *
linux_shmem_file_setup(const char *name, loff_t size, unsigned long flags)
{
	struct fileobj {
		struct linux_file file __aligned(sizeof(void *));
		struct vnode vnode __aligned(sizeof(void *));
	};
	struct fileobj *fileobj;
	struct linux_file *filp;
	struct vnode *vp;
	int error;

	fileobj = kzalloc(sizeof(*fileobj), GFP_KERNEL);
	if (fileobj == NULL) {
		error = -ENOMEM;
		goto err_0;
	}
	filp = &fileobj->file;
	vp = &fileobj->vnode;

	filp->f_count = 1;
	filp->f_vnode = vp;
	filp->f_shmem = vm_pager_allocate(OBJT_SWAP, NULL, size,
	    VM_PROT_READ | VM_PROT_WRITE, 0, curthread->td_ucred);
	if (filp->f_shmem == NULL) {
		error = -ENOMEM;
		goto err_1;
	}
	return (filp);
err_1:
	kfree(filp);
err_0:
	return (ERR_PTR(error));
}

static vm_ooffset_t
linux_invalidate_mapping_pages_sub(vm_object_t obj, vm_pindex_t start,
    vm_pindex_t end, int flags)
{
	int start_count, end_count;

	VM_OBJECT_WLOCK(obj);
	start_count = obj->resident_page_count;
	vm_object_page_remove(obj, start, end, flags);
	end_count = obj->resident_page_count;
	VM_OBJECT_WUNLOCK(obj);
	return (start_count - end_count);
}

unsigned long
linux_invalidate_mapping_pages(vm_object_t obj, pgoff_t start, pgoff_t end)
{

	return (linux_invalidate_mapping_pages_sub(obj, start, end, OBJPR_CLEANONLY));
}

void
linux_shmem_truncate_range(vm_object_t obj, loff_t lstart, loff_t lend)
{
	vm_pindex_t start = OFF_TO_IDX(lstart + PAGE_SIZE - 1);
	vm_pindex_t end = OFF_TO_IDX(lend + 1);

	(void) linux_invalidate_mapping_pages_sub(obj, start, end, 0);
}
