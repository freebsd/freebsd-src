/*
 * SN Platform FetchOp Support
 *
 * This driver exports the SN fetchop facility to user processes.
 * Fetchops are atomic memory operations that are implemented in the
 * memory controller on SGI SN hardware.
 */

/*
 * Copyright (C) 1999,2001-2003 Silicon Graphics, Inc. All rights
 * reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.  Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this program with
 * other software, or any other product whatsoever.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston MA
 * 02111-1307, USA.
 *
 * Contact information:  Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
 * Mountain View, CA  94043, or:
 *
 * http://www.sgi.com
 *
 * For further information regarding this notice, see:
 *
 * http://oss.sgi.com/projects/GenInfo/NoticeExplan
 */


#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/mm.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>
#include <linux/bitops.h>
#include <linux/efi.h>
#include <asm/system.h>
#include <asm/pgtable.h>
#include <asm/machvec.h>
#include <asm/sn/sgi.h>
#include <asm/sn/addrs.h>
#include <asm/sn/arch.h>
#include <asm/sn/fetchop.h>
#include <asm/sn/sn_cpuid.h>


#define DRIVER_ID_STR	"SGI Fetchop Device Driver"
#define REVISION		"1.03"


#define MSPEC_TO_NID(maddr)	nasid_to_cnodeid(NASID_GET(maddr))


static int fetchop_mmap(struct file *file, struct vm_area_struct *vma);
static void fetchop_open(struct vm_area_struct *vma);
static void fetchop_close(struct vm_area_struct *vma);

static struct file_operations fetchop_fops = {
	owner:		THIS_MODULE,
	mmap:		fetchop_mmap,
};

static struct miscdevice fetchop_miscdev = {
	MISC_DYNAMIC_MINOR,
	"fetchop",
	&fetchop_fops
};

static struct vm_operations_struct fetchop_vm_ops = {
	open:		fetchop_open,
	close:		fetchop_close,
};

/*
 * There is one of these structs per node. It is used to manage the fetchop
 * space that is available on the node. Current assumption is that there is
 * only 1 fetchop block of memory per node.
 */
struct node_fetchops {
	long		maddr;		/* MSPEC address of start of fetchops. */
	int		count;		/* Total number of fetchop pages. */
	atomic_t		free;		/* Number of pages currently free. */
	unsigned long	bits[1];		/* Bitmap for managing pages. */
};


/*
 * One of these structures is allocated when a fetchop region is mmaped. The
 * structure is pointed to by the vma->vm_private_data field in the vma struct. 
 * This structure is used to record the addresses of the fetchop pages.
 */
struct vma_data {
	int		count;		/* Number of pages allocated. */
	atomic_t		refcnt;		/* Number of vmas sharing the data. */
	unsigned long	maddr[1];	/* Array of MSPEC addresses. */
};


/*
 * Fetchop statistics.
 */
struct fetchop_stats {
	unsigned long  map_count;		/* Number of active mmap's */
	unsigned long  pages_in_use;		/* Number of fetchop pages in use */
	unsigned long  pages_total;		/* Total number of fetchop pages */
};

static struct fetchop_stats	fetchop_stats;
static struct node_fetchops	*node_fetchops[MAX_COMPACT_NODES];
static spinlock_t		fetchop_lock = SPIN_LOCK_UNLOCKED;

/*
 * NOTE: This is included here simply because the kernel doesn't have
 * a generally acceptable UC memory allocator.  See PV: 896479 for
 * more details. --cw
 *
 * efi_memmap_walk_uc
 *
 * This function walks the EFI memory map and calls 'callback' once
 * for each EFI memory descriptor that has memory that marked as only
 * EFI_MEMORY_UC.
 */
static void
efi_memmap_walk_uc (efi_freemem_callback_t callback, void *arg)
{
	void *efi_map_start, *efi_map_end, *p;
	efi_memory_desc_t *md;
	u64 efi_desc_size, start, end;

	efi_map_start = __va(ia64_boot_param->efi_memmap);
	efi_map_end   = efi_map_start + ia64_boot_param->efi_memmap_size;
	efi_desc_size = ia64_boot_param->efi_memdesc_size;

	for (p = efi_map_start; p < efi_map_end; p += efi_desc_size) {
		md = p;
		if (md->attribute == EFI_MEMORY_UC) {
			start = PAGE_ALIGN(md->phys_addr);
			end = PAGE_ALIGN((md->phys_addr+(md->num_pages << EFI_PAGE_SHIFT)) & PAGE_MASK);
			if ((*callback)(start, end, arg) < 0)
				return;
		}
	}
}


/*
 * fetchop_initialize_page
 *
 * Initial a page that is about to be used for fetchops. 
 * All fetchop variables in the page are set to 0.
 *
 */
static void
fetchop_initialize_page(unsigned long maddr)
{
	unsigned long	p, pe;

	for (p=FETCHOP_KADDR_TO_MSPEC_ADDR(maddr), pe=p+PAGE_SIZE; p<pe; p+=FETCHOP_VAR_SIZE)
		FETCHOP_STORE_OP(p,FETCHOP_STORE, 0);
}


/*
 * fetchop_alloc_page
 *
 * Allocate 1 fetchop page. Allocates on the requested node. If no
 * fetchops are available on the requested node, roundrobin starting
 * with higher nodes,
 */
static unsigned long
fetchop_alloc_page(int nid)
{
	int i, bit;
	struct node_fetchops *fops;
	unsigned long maddr;

	if (nid < 0 || nid >= numnodes)
		nid = numa_node_id();
	for (i=0; i<numnodes; i++) {
		fops = node_fetchops[nid];
		while (fops && (bit = find_first_zero_bit(fops->bits, fops->count)) < fops->count) {
			if (test_and_set_bit(bit, fops->bits) == 0) {
				atomic_dec(&node_fetchops[nid]->free);
				maddr = fops->maddr + (bit<<PAGE_SHIFT);
				fetchop_initialize_page(maddr);
				return maddr;
			}
		}
		nid = (nid+1 < numnodes) ? nid+1 : 0;
	}
	return 0;

}


/*
 * fetchop_free_pages
 *
 * Free all fetchop pages that are linked to a vma struct.
 */
static void
fetchop_free_page(unsigned long maddr)
{
	int nid, bit;

	nid = MSPEC_TO_NID(maddr);
	bit = (maddr - node_fetchops[nid]->maddr) >> PAGE_SHIFT;
	clear_bit(bit, node_fetchops[nid]->bits);
	atomic_inc(&node_fetchops[nid]->free);
}

static void
fetchop_free_pages(struct vma_data *vdata)
{
	int i;

	for (i=0; i<vdata->count; i++)
		fetchop_free_page(vdata->maddr[i]);
}


/*
 * fetchop_update_stats
 *
 * Update statistics of the number of fetchop mappings & pages.
 * If creating a new mapping, ensure that we don't exceed the maximum allowed
 * number of fetchop pages.
 */
static int
fetchop_update_stats(int mmap, long count)
{
	int	ret = 0;

	spin_lock(&fetchop_lock);
	if (count > 0 && fetchop_stats.pages_in_use + count > fetchop_stats.pages_total)  {
		ret = -1;
	} else {
		fetchop_stats.map_count += mmap;
		fetchop_stats.pages_in_use += count;
	}
	spin_unlock(&fetchop_lock);

	return ret;
}


/*
 * fetchop_mmap
 *
 * Called when mmaping the device. Creates fetchop pages and map them
 * to user space.
 */
static int
fetchop_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long vm_start;
	unsigned long maddr;
	int pages;
	struct vma_data *vdata;

	if (vma->vm_pgoff != 0)
		return -EINVAL;

	if ((vma->vm_flags&VM_WRITE) == 0)
		return -EPERM;

	pages = (vma->vm_end - vma->vm_start) >> PAGE_SHIFT;
	if (fetchop_update_stats(1, pages) < 0)
		return -ENOSPC;

	if (!(vdata=vmalloc(sizeof(struct vma_data)+(pages-1)*sizeof(long)))) {
		fetchop_update_stats(-1, -pages);
		return -ENOMEM;
	}

	vdata->count = 0;
	vdata->refcnt = ATOMIC_INIT(1);
	vma->vm_private_data = vdata;

	vma->vm_flags |= (VM_IO | VM_SHM | VM_LOCKED | VM_NONCACHED);
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	vma->vm_ops = &fetchop_vm_ops;
	vm_start = vma->vm_start;

	while (vm_start < vma->vm_end) {
		maddr = fetchop_alloc_page(numa_node_id());
		if (maddr == 0)
			BUG();
		vdata->maddr[vdata->count++] = maddr;


		if (remap_page_range(vm_start, __pa(maddr), PAGE_SIZE, vma->vm_page_prot)) {
			fetchop_free_pages(vma->vm_private_data);
			vfree(vdata);
			fetchop_update_stats(-1, -pages);
			return -EAGAIN;
		}
		vm_start += PAGE_SIZE;
	}

	return 0;
}

/*
 * fetchop_open
 *
 * Called when a device mapping is created by a means other than mmap
 * (via fork, etc.).  Increments the reference count on the underlying
 * fetchop data so it is not freed prematurely.
 */
static void
fetchop_open(struct vm_area_struct *vma)
{
	struct vma_data *vdata;

	vdata = vma->vm_private_data;
	if (vdata && vdata->count) {
		atomic_inc(&vdata->refcnt);
	}
}

/*
 * fetchop_close
 *
 * Called when unmapping a device mapping. Frees all fetchop pages
 * belonging to the vma.
 */
static void
fetchop_close(struct vm_area_struct *vma)
{
	struct vma_data *vdata;

	vdata = vma->vm_private_data;
	if (vdata && vdata->count && !atomic_dec(&vdata->refcnt)) {
		fetchop_free_pages(vdata);
		fetchop_update_stats(-1, -vdata->count);
		vfree(vdata);
	}
}

#ifdef CONFIG_PROC_FS

static struct proc_dir_entry   *proc_fetchop;

/*
 * fetchop_read_proc
 *
 * Implements /proc/fetchop. Return statistics about fetchops.
 */
static int
fetchop_read_proc(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	struct node_fetchops *fops;
	int len = 0, nid;

	len += sprintf(page + len, "mappings               : %lu\n", fetchop_stats.map_count);
	len += sprintf(page + len, "current fetchop pages  : %lu\n", fetchop_stats.pages_in_use);
	len += sprintf(page + len, "maximum fetchop pages  : %lu\n", fetchop_stats.pages_total);

	len += sprintf(page + len, "%4s %7s %7s\n", "node", "total", "free");
	for (nid = 0; nid < numnodes; nid++) {
		fops = node_fetchops[nid];
		len += sprintf(page + len, "%4d %7d %7d\n", nid, fops ? fops->count : 0, fops ? atomic_read(&fops->free) : 0);
	}

	if (len <= off+count) *eof = 1;
	*start = page + off;
	len   -= off;
	if (len>count) len = count;
	if (len<0) len = 0;
	return len;
}

static int
fetchop_write_proc (struct file *file, const char *userbuf, unsigned long count, void *data)
{
    char buf[80];

    if (copy_from_user(buf, userbuf, count < sizeof(buf) ? count : sizeof(buf)))
        return -EFAULT;

    return count;
}
#endif /* CONFIG_PROC_FS */

/*
 * fetchop_build_memmap,
 *
 * Called at boot time to build a map of pages that can be used for
 * fetchops.
 */
static int __init
fetchop_build_memmap(unsigned long start, unsigned long end, void *arg)
{
	struct node_fetchops *fops;
	long count, bytes;

	count = (end - start) >> PAGE_SHIFT;
	bytes = sizeof(struct node_fetchops) + count/8;
	fops = vmalloc(bytes);
	memset(fops, 0, bytes);
	fops->maddr = FETCHOP_KADDR_TO_MSPEC_ADDR(start);
	fops->count = count;
	atomic_add(count, &fops->free);
	fetchop_stats.pages_total += count;
	node_fetchops[MSPEC_TO_NID(start)] = fops;

	sn_flush_all_caches((long)__va(start), end - start);

	return 0;
}



/*
 * fetchop_init
 *
 * Called at boot time to initialize the fetchop facility.
 */
static int __init
fetchop_init(void)
{
	int ret;
	devfs_handle_t  hnd;

	if (!ia64_platform_is("sn2"))
		return -ENODEV;

#ifdef CONFIG_DEVFS_FS
	if (!devfs_register(NULL, FETCHOP_BASENAME, DEVFS_FL_AUTO_DEVNUM,
			    0, 0, S_IFCHR | S_IRUGO | S_IWUGO, &fetchop_fops, NULL)) {
		printk(KERN_ERR "%s:  failed to register device\n", DRIVER_ID_STR);
		return -ENODEV;
	}
#endif

	if ((ret = misc_register(&fetchop_miscdev))) {
		printk(KERN_ERR "%s: failed to register device\n", DRIVER_ID_STR);
		return ret;
	}
	printk(KERN_DEBUG "%s:  registered misc-device with minor %d\n", DRIVER_ID_STR, fetchop_miscdev.minor);

	if ((proc_fetchop = create_proc_entry(FETCHOP_BASENAME, 0644, NULL)) == NULL) {
		printk(KERN_ERR "%s: unable to create proc entry", DRIVER_ID_STR);
		devfs_unregister(hnd);
		return -EINVAL;
	}

#ifdef CONFIG_PROC_FS
	if ((proc_fetchop = create_proc_entry(FETCHOP_BASENAME, 0644, NULL)) == NULL) {
		printk(KERN_ERR "%s: unable to create proc entry", DRIVER_ID_STR);
			devfs_unregister(hnd);
		return -EINVAL;
	}
	proc_fetchop->read_proc = fetchop_read_proc;
	proc_fetchop->write_proc = fetchop_write_proc;
#endif /* CONFIG_PROC_FS */

	efi_memmap_walk_uc(fetchop_build_memmap, 0);
	printk(KERN_INFO "%s: v%s\n", DRIVER_ID_STR, REVISION);

	return 0;
}



/*-----------------------------------------------------------------------------
 * KERNEL APIs
 * 	Note: right now, these APIs return a full page of fetchops.  If these
 *	interfaces are used often for tasks which do not require a full page of
 *	fetchops, new APIs should be added to suballocate out of a single page.
 */

unsigned long
fetchop_kalloc_page(int nid)
{
	if (fetchop_update_stats(1, 1) < 0)
		return 0;
	return fetchop_alloc_page(nid);
}
EXPORT_SYMBOL(fetchop_kalloc_page);


void
fetchop_kfree_page(unsigned long maddr)
{
	fetchop_free_page(maddr);
	fetchop_update_stats(-1, -1);
}
EXPORT_SYMBOL(fetchop_kfree_page);

module_init(fetchop_init);

MODULE_AUTHOR("Silicon Graphics Inc.");
MODULE_DESCRIPTION("Driver for SGI SN 'fetchop' atomic memory operations");
MODULE_LICENSE("GPL");
