/* drm_proc.h -- /proc support for DRM -*- linux-c -*-
 * Created: Mon Jan 11 09:48:47 1999 by faith@valinux.com
 *
 * Copyright 1999 Precision Insight, Inc., Cedar Park, Texas.
 * Copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Rickard E. (Rik) Faith <faith@valinux.com>
 *    Gareth Hughes <gareth@valinux.com>
 *
 * Acknowledgements:
 *    Matthew J Sottek <matthew.j.sottek@intel.com> sent in a patch to fix
 *    the problem with the proc files not outputting all their information.
 */

#include "drmP.h"

static int	   DRM(name_info)(char *buf, char **start, off_t offset,
				  int request, int *eof, void *data);
static int	   DRM(vm_info)(char *buf, char **start, off_t offset,
				int request, int *eof, void *data);
static int	   DRM(clients_info)(char *buf, char **start, off_t offset,
				     int request, int *eof, void *data);
static int	   DRM(queues_info)(char *buf, char **start, off_t offset,
				    int request, int *eof, void *data);
static int	   DRM(bufs_info)(char *buf, char **start, off_t offset,
				  int request, int *eof, void *data);
#if DRM_DEBUG_CODE
static int	   DRM(vma_info)(char *buf, char **start, off_t offset,
				 int request, int *eof, void *data);
#endif
#if __HAVE_DMA_HISTOGRAM
static int	   DRM(histo_info)(char *buf, char **start, off_t offset,
				   int request, int *eof, void *data);
#endif

struct drm_proc_list {
	const char *name;
	int	   (*f)(char *, char **, off_t, int, int *, void *);
} DRM(proc_list)[] = {
	{ "name",    DRM(name_info)    },
	{ "mem",     DRM(mem_info)     },
	{ "vm",	     DRM(vm_info)      },
	{ "clients", DRM(clients_info) },
	{ "queues",  DRM(queues_info)  },
	{ "bufs",    DRM(bufs_info)    },
#if DRM_DEBUG_CODE
	{ "vma",     DRM(vma_info)     },
#endif
#if __HAVE_DMA_HISTOGRAM
	{ "histo",   DRM(histo_info)   },
#endif
};
#define DRM_PROC_ENTRIES (sizeof(DRM(proc_list))/sizeof(DRM(proc_list)[0]))

struct proc_dir_entry *DRM(proc_init)(drm_device_t *dev, int minor,
				      struct proc_dir_entry *root,
				      struct proc_dir_entry **dev_root)
{
	struct proc_dir_entry *ent;
	int		      i, j;
	char                  name[64];

	if (!minor) root = create_proc_entry("dri", S_IFDIR, NULL);
	if (!root) {
		DRM_ERROR("Cannot create /proc/dri\n");
		return NULL;
	}

	sprintf(name, "%d", minor);
	*dev_root = create_proc_entry(name, S_IFDIR, root);
	if (!*dev_root) {
		DRM_ERROR("Cannot create /proc/%s\n", name);
		return NULL;
	}

	for (i = 0; i < DRM_PROC_ENTRIES; i++) {
		ent = create_proc_entry(DRM(proc_list)[i].name,
					S_IFREG|S_IRUGO, *dev_root);
		if (!ent) {
			DRM_ERROR("Cannot create /proc/dri/%s/%s\n",
				  name, DRM(proc_list)[i].name);
			for (j = 0; j < i; j++)
				remove_proc_entry(DRM(proc_list)[i].name,
						  *dev_root);
			remove_proc_entry(name, root);
			if (!minor) remove_proc_entry("dri", NULL);
			return NULL;
		}
		ent->read_proc = DRM(proc_list)[i].f;
		ent->data      = dev;
	}

	return root;
}


int DRM(proc_cleanup)(int minor, struct proc_dir_entry *root,
		      struct proc_dir_entry *dev_root)
{
	int  i;
	char name[64];

	if (!root || !dev_root) return 0;

	for (i = 0; i < DRM_PROC_ENTRIES; i++)
		remove_proc_entry(DRM(proc_list)[i].name, dev_root);
	sprintf(name, "%d", minor);
	remove_proc_entry(name, root);
	if (!minor) remove_proc_entry("dri", NULL);

	return 0;
}

static int DRM(name_info)(char *buf, char **start, off_t offset, int request,
			  int *eof, void *data)
{
	drm_device_t *dev = (drm_device_t *)data;
	int          len  = 0;

	if (offset > DRM_PROC_LIMIT) {
		*eof = 1;
		return 0;
	}

	*start = &buf[offset];
	*eof   = 0;

	if (dev->unique) {
		DRM_PROC_PRINT("%s 0x%lx %s\n",
			       dev->name, (long)dev->device, dev->unique);
	} else {
		DRM_PROC_PRINT("%s 0x%lx\n", dev->name, (long)dev->device);
	}

	if (len > request + offset) return request;
	*eof = 1;
	return len - offset;
}

static int DRM(_vm_info)(char *buf, char **start, off_t offset, int request,
			 int *eof, void *data)
{
	drm_device_t *dev = (drm_device_t *)data;
	int          len  = 0;
	drm_map_t    *map;
	drm_map_list_t *r_list;
	struct list_head *list;

				/* Hardcoded from _DRM_FRAME_BUFFER,
                                   _DRM_REGISTERS, _DRM_SHM, and
                                   _DRM_AGP. */
	const char   *types[] = { "FB", "REG", "SHM", "AGP" };
	const char   *type;
	int	     i;

	if (offset > DRM_PROC_LIMIT) {
		*eof = 1;
		return 0;
	}

	*start = &buf[offset];
	*eof   = 0;

	DRM_PROC_PRINT("slot	 offset	      size type flags	 "
		       "address mtrr\n\n");
	i = 0;
	if (dev->maplist != NULL) list_for_each(list, &dev->maplist->head) {
		r_list = (drm_map_list_t *)list;
		map = r_list->map;
		if(!map) continue;
		if (map->type < 0 || map->type > 3) type = "??";
		else				    type = types[map->type];
		DRM_PROC_PRINT("%4d 0x%08lx 0x%08lx %4.4s  0x%02x 0x%08lx ",
			       i,
			       map->offset,
			       map->size,
			       type,
			       map->flags,
			       (unsigned long)map->handle);
		if (map->mtrr < 0) {
			DRM_PROC_PRINT("none\n");
		} else {
			DRM_PROC_PRINT("%4d\n", map->mtrr);
		}
		i++;
	}

	if (len > request + offset) return request;
	*eof = 1;
	return len - offset;
}

static int DRM(vm_info)(char *buf, char **start, off_t offset, int request,
			int *eof, void *data)
{
	drm_device_t *dev = (drm_device_t *)data;
	int	     ret;

	down(&dev->struct_sem);
	ret = DRM(_vm_info)(buf, start, offset, request, eof, data);
	up(&dev->struct_sem);
	return ret;
}


static int DRM(_queues_info)(char *buf, char **start, off_t offset,
			     int request, int *eof, void *data)
{
	drm_device_t *dev = (drm_device_t *)data;
	int          len  = 0;
	int	     i;
	drm_queue_t  *q;

	if (offset > DRM_PROC_LIMIT) {
		*eof = 1;
		return 0;
	}

	*start = &buf[offset];
	*eof   = 0;

	DRM_PROC_PRINT("  ctx/flags   use   fin"
		       "   blk/rw/rwf  wait    flushed	   queued"
		       "      locks\n\n");
	for (i = 0; i < dev->queue_count; i++) {
		q = dev->queuelist[i];
		atomic_inc(&q->use_count);
		DRM_PROC_PRINT_RET(atomic_dec(&q->use_count),
				   "%5d/0x%03x %5d %5d"
				   " %5d/%c%c/%c%c%c %5Zd\n",
				   i,
				   q->flags,
				   atomic_read(&q->use_count),
				   atomic_read(&q->finalization),
				   atomic_read(&q->block_count),
				   atomic_read(&q->block_read) ? 'r' : '-',
				   atomic_read(&q->block_write) ? 'w' : '-',
				   waitqueue_active(&q->read_queue) ? 'r':'-',
				   waitqueue_active(&q->write_queue) ? 'w':'-',
				   waitqueue_active(&q->flush_queue) ? 'f':'-',
				   DRM_BUFCOUNT(&q->waitlist));
		atomic_dec(&q->use_count);
	}

	if (len > request + offset) return request;
	*eof = 1;
	return len - offset;
}

static int DRM(queues_info)(char *buf, char **start, off_t offset, int request,
			    int *eof, void *data)
{
	drm_device_t *dev = (drm_device_t *)data;
	int	     ret;

	down(&dev->struct_sem);
	ret = DRM(_queues_info)(buf, start, offset, request, eof, data);
	up(&dev->struct_sem);
	return ret;
}

/* drm_bufs_info is called whenever a process reads
   /dev/dri/<dev>/bufs. */

static int DRM(_bufs_info)(char *buf, char **start, off_t offset, int request,
			   int *eof, void *data)
{
	drm_device_t	 *dev = (drm_device_t *)data;
	int              len  = 0;
	drm_device_dma_t *dma = dev->dma;
	int		 i;

	if (!dma || offset > DRM_PROC_LIMIT) {
		*eof = 1;
		return 0;
	}

	*start = &buf[offset];
	*eof   = 0;

	DRM_PROC_PRINT(" o     size count  free	 segs pages    kB\n\n");
	for (i = 0; i <= DRM_MAX_ORDER; i++) {
		if (dma->bufs[i].buf_count)
			DRM_PROC_PRINT("%2d %8d %5d %5d %5d %5d %5ld\n",
				       i,
				       dma->bufs[i].buf_size,
				       dma->bufs[i].buf_count,
				       atomic_read(&dma->bufs[i]
						   .freelist.count),
				       dma->bufs[i].seg_count,
				       dma->bufs[i].seg_count
				       *(1 << dma->bufs[i].page_order),
				       (dma->bufs[i].seg_count
					* (1 << dma->bufs[i].page_order))
				       * PAGE_SIZE / 1024);
	}
	DRM_PROC_PRINT("\n");
	for (i = 0; i < dma->buf_count; i++) {
		if (i && !(i%32)) DRM_PROC_PRINT("\n");
		DRM_PROC_PRINT(" %d", dma->buflist[i]->list);
	}
	DRM_PROC_PRINT("\n");

	if (len > request + offset) return request;
	*eof = 1;
	return len - offset;
}

static int DRM(bufs_info)(char *buf, char **start, off_t offset, int request,
			  int *eof, void *data)
{
	drm_device_t *dev = (drm_device_t *)data;
	int	     ret;

	down(&dev->struct_sem);
	ret = DRM(_bufs_info)(buf, start, offset, request, eof, data);
	up(&dev->struct_sem);
	return ret;
}


static int DRM(_clients_info)(char *buf, char **start, off_t offset,
			      int request, int *eof, void *data)
{
	drm_device_t *dev = (drm_device_t *)data;
	int          len  = 0;
	drm_file_t   *priv;

	if (offset > DRM_PROC_LIMIT) {
		*eof = 1;
		return 0;
	}

	*start = &buf[offset];
	*eof   = 0;

	DRM_PROC_PRINT("a dev	pid    uid	magic	  ioctls\n\n");
	for (priv = dev->file_first; priv; priv = priv->next) {
		DRM_PROC_PRINT("%c %3d %5d %5d %10u %10lu\n",
			       priv->authenticated ? 'y' : 'n',
			       priv->minor,
			       priv->pid,
			       priv->uid,
			       priv->magic,
			       priv->ioctl_count);
	}

	if (len > request + offset) return request;
	*eof = 1;
	return len - offset;
}

static int DRM(clients_info)(char *buf, char **start, off_t offset,
			     int request, int *eof, void *data)
{
	drm_device_t *dev = (drm_device_t *)data;
	int	     ret;

	down(&dev->struct_sem);
	ret = DRM(_clients_info)(buf, start, offset, request, eof, data);
	up(&dev->struct_sem);
	return ret;
}

#if DRM_DEBUG_CODE

#define DRM_VMA_VERBOSE 0

static int DRM(_vma_info)(char *buf, char **start, off_t offset, int request,
			  int *eof, void *data)
{
	drm_device_t	      *dev = (drm_device_t *)data;
	int                   len  = 0;
	drm_vma_entry_t	      *pt;
	struct vm_area_struct *vma;
#if DRM_VMA_VERBOSE
	unsigned long	      i;
	unsigned long	      address;
	pgd_t		      *pgd;
	pmd_t		      *pmd;
	pte_t		      *pte;
#endif
#if defined(__i386__)
	unsigned int	      pgprot;
#endif

	if (offset > DRM_PROC_LIMIT) {
		*eof = 1;
		return 0;
	}

	*start = &buf[offset];
	*eof   = 0;

	DRM_PROC_PRINT("vma use count: %d, high_memory = %p, 0x%08lx\n",
		       atomic_read(&dev->vma_count),
		       high_memory, virt_to_phys(high_memory));
	for (pt = dev->vmalist; pt; pt = pt->next) {
		if (!(vma = pt->vma)) continue;
		DRM_PROC_PRINT("\n%5d 0x%08lx-0x%08lx %c%c%c%c%c%c 0x%08lx",
			       pt->pid,
			       vma->vm_start,
			       vma->vm_end,
			       vma->vm_flags & VM_READ	   ? 'r' : '-',
			       vma->vm_flags & VM_WRITE	   ? 'w' : '-',
			       vma->vm_flags & VM_EXEC	   ? 'x' : '-',
			       vma->vm_flags & VM_MAYSHARE ? 's' : 'p',
			       vma->vm_flags & VM_LOCKED   ? 'l' : '-',
			       vma->vm_flags & VM_IO	   ? 'i' : '-',
			       VM_OFFSET(vma));

#if defined(__i386__)
		pgprot = pgprot_val(vma->vm_page_prot);
		DRM_PROC_PRINT(" %c%c%c%c%c%c%c%c%c",
			       pgprot & _PAGE_PRESENT  ? 'p' : '-',
			       pgprot & _PAGE_RW       ? 'w' : 'r',
			       pgprot & _PAGE_USER     ? 'u' : 's',
			       pgprot & _PAGE_PWT      ? 't' : 'b',
			       pgprot & _PAGE_PCD      ? 'u' : 'c',
			       pgprot & _PAGE_ACCESSED ? 'a' : '-',
			       pgprot & _PAGE_DIRTY    ? 'd' : '-',
			       pgprot & _PAGE_PSE      ? 'm' : 'k',
			       pgprot & _PAGE_GLOBAL   ? 'g' : 'l' );
#endif
		DRM_PROC_PRINT("\n");
#if 0
		for (i = vma->vm_start; i < vma->vm_end; i += PAGE_SIZE) {
			pgd = pgd_offset(vma->vm_mm, i);
			pmd = pmd_offset(pgd, i);
			pte = pte_offset(pmd, i);
			if (pte_present(*pte)) {
				address = __pa(pte_page(*pte))
					+ (i & (PAGE_SIZE-1));
				DRM_PROC_PRINT("      0x%08lx -> 0x%08lx"
					       " %c%c%c%c%c\n",
					       i,
					       address,
					       pte_read(*pte)  ? 'r' : '-',
					       pte_write(*pte) ? 'w' : '-',
					       pte_exec(*pte)  ? 'x' : '-',
					       pte_dirty(*pte) ? 'd' : '-',
					       pte_young(*pte) ? 'a' : '-' );
			} else {
				DRM_PROC_PRINT("      0x%08lx\n", i);
			}
		}
#endif
	}

	if (len > request + offset) return request;
	*eof = 1;
	return len - offset;
}

static int DRM(vma_info)(char *buf, char **start, off_t offset, int request,
			 int *eof, void *data)
{
	drm_device_t *dev = (drm_device_t *)data;
	int	     ret;

	down(&dev->struct_sem);
	ret = DRM(_vma_info)(buf, start, offset, request, eof, data);
	up(&dev->struct_sem);
	return ret;
}
#endif


#if __HAVE_DMA_HISTOGRAM
static int DRM(_histo_info)(char *buf, char **start, off_t offset, int request,
			    int *eof, void *data)
{
	drm_device_t	 *dev = (drm_device_t *)data;
	int              len  = 0;
	drm_device_dma_t *dma = dev->dma;
	int		 i;
	unsigned long	 slot_value = DRM_DMA_HISTOGRAM_INITIAL;
	unsigned long	 prev_value = 0;
	drm_buf_t	 *buffer;

	if (offset > DRM_PROC_LIMIT) {
		*eof = 1;
		return 0;
	}

	*start = &buf[offset];
	*eof   = 0;

	DRM_PROC_PRINT("general statistics:\n");
	DRM_PROC_PRINT("total	 %10u\n", atomic_read(&dev->histo.total));
	DRM_PROC_PRINT("open	 %10u\n",
		       atomic_read(&dev->counts[_DRM_STAT_OPENS]));
	DRM_PROC_PRINT("close	 %10u\n",
		       atomic_read(&dev->counts[_DRM_STAT_CLOSES]));
	DRM_PROC_PRINT("ioctl	 %10u\n",
		       atomic_read(&dev->counts[_DRM_STAT_IOCTLS]));

	DRM_PROC_PRINT("\nlock statistics:\n");
	DRM_PROC_PRINT("locks	 %10u\n",
		       atomic_read(&dev->counts[_DRM_STAT_LOCKS]));
	DRM_PROC_PRINT("unlocks	 %10u\n",
		       atomic_read(&dev->counts[_DRM_STAT_UNLOCKS]));

	if (dma) {
#if 0
		DRM_PROC_PRINT("\ndma statistics:\n");
		DRM_PROC_PRINT("prio	 %10u\n",
			       atomic_read(&dma->total_prio));
		DRM_PROC_PRINT("bytes	 %10u\n",
			       atomic_read(&dma->total_bytes));
		DRM_PROC_PRINT("dmas	 %10u\n",
			       atomic_read(&dma->total_dmas));
		DRM_PROC_PRINT("missed:\n");
		DRM_PROC_PRINT("  dma	 %10u\n",
			       atomic_read(&dma->total_missed_dma));
		DRM_PROC_PRINT("  lock	 %10u\n",
			       atomic_read(&dma->total_missed_lock));
		DRM_PROC_PRINT("  free	 %10u\n",
			       atomic_read(&dma->total_missed_free));
		DRM_PROC_PRINT("  sched	 %10u\n",
			       atomic_read(&dma->total_missed_sched));
		DRM_PROC_PRINT("tried	 %10u\n",
			       atomic_read(&dma->total_tried));
		DRM_PROC_PRINT("hit	 %10u\n",
			       atomic_read(&dma->total_hit));
		DRM_PROC_PRINT("lost	 %10u\n",
			       atomic_read(&dma->total_lost));
#endif

		buffer = dma->next_buffer;
		if (buffer) {
			DRM_PROC_PRINT("next_buffer %7d\n", buffer->idx);
		} else {
			DRM_PROC_PRINT("next_buffer    none\n");
		}
		buffer = dma->this_buffer;
		if (buffer) {
			DRM_PROC_PRINT("this_buffer %7d\n", buffer->idx);
		} else {
			DRM_PROC_PRINT("this_buffer    none\n");
		}
	}


	DRM_PROC_PRINT("\nvalues:\n");
	if (dev->lock.hw_lock) {
		DRM_PROC_PRINT("lock	       0x%08x\n",
			       dev->lock.hw_lock->lock);
	} else {
		DRM_PROC_PRINT("lock		     none\n");
	}
	DRM_PROC_PRINT("context_flag   0x%08lx\n", dev->context_flag);
	DRM_PROC_PRINT("interrupt_flag 0x%08lx\n", dev->interrupt_flag);
	DRM_PROC_PRINT("dma_flag       0x%08lx\n", dev->dma_flag);

	DRM_PROC_PRINT("queue_count    %10d\n",	 dev->queue_count);
	DRM_PROC_PRINT("last_context   %10d\n",	 dev->last_context);
	DRM_PROC_PRINT("last_switch    %10lu\n", dev->last_switch);
	DRM_PROC_PRINT("last_checked   %10d\n",	 dev->last_checked);


	DRM_PROC_PRINT("\n		       q2d	  d2c	     c2f"
		       "	q2c	   q2f	      dma	 sch"
		       "	ctx	  lacq	     lhld\n\n");
	for (i = 0; i < DRM_DMA_HISTOGRAM_SLOTS; i++) {
		DRM_PROC_PRINT("%s %10lu %10u %10u %10u %10u %10u"
			       " %10u %10u %10u %10u %10u\n",
			       i == DRM_DMA_HISTOGRAM_SLOTS - 1 ? ">=" : "< ",
			       i == DRM_DMA_HISTOGRAM_SLOTS - 1
			       ? prev_value : slot_value ,

			       atomic_read(&dev->histo
					   .queued_to_dispatched[i]),
			       atomic_read(&dev->histo
					   .dispatched_to_completed[i]),
			       atomic_read(&dev->histo
					   .completed_to_freed[i]),

			       atomic_read(&dev->histo
					   .queued_to_completed[i]),
			       atomic_read(&dev->histo
					   .queued_to_freed[i]),
			       atomic_read(&dev->histo.dma[i]),
			       atomic_read(&dev->histo.schedule[i]),
			       atomic_read(&dev->histo.ctx[i]),
			       atomic_read(&dev->histo.lacq[i]),
			       atomic_read(&dev->histo.lhld[i]));
		prev_value = slot_value;
		slot_value = DRM_DMA_HISTOGRAM_NEXT(slot_value);
	}

	if (len > request + offset) return request;
	*eof = 1;
	return len - offset;
}

static int DRM(histo_info)(char *buf, char **start, off_t offset, int request,
			   int *eof, void *data)
{
	drm_device_t *dev = (drm_device_t *)data;
	int	     ret;

	down(&dev->struct_sem);
	ret = DRM(_histo_info)(buf, start, offset, request, eof, data);
	up(&dev->struct_sem);
	return ret;
}
#endif
