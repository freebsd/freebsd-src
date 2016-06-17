/* proc.c -- /proc support for DRM -*- linux-c -*-
 * Created: Mon Jan 11 09:48:47 1999 by faith@precisioninsight.com
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
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Rickard E. (Rik) Faith <faith@valinux.com>
 */

#define __NO_VERSION__
#include "drmP.h"

static struct proc_dir_entry *drm_root	   = NULL;
static struct proc_dir_entry *drm_dev_root = NULL;
static char		     drm_slot_name[64];

static int	   drm_name_info(char *buf, char **start, off_t offset,
				 int len, int *eof, void *data);
static int	   drm_vm_info(char *buf, char **start, off_t offset,
			       int len, int *eof, void *data);
static int	   drm_clients_info(char *buf, char **start, off_t offset,
				    int len, int *eof, void *data);
static int	   drm_queues_info(char *buf, char **start, off_t offset,
				   int len, int *eof, void *data);
static int	   drm_bufs_info(char *buf, char **start, off_t offset,
				 int len, int *eof, void *data);
#if DRM_DEBUG_CODE
static int	   drm_vma_info(char *buf, char **start, off_t offset,
				int len, int *eof, void *data);
#endif
#if DRM_DMA_HISTOGRAM
static int	   drm_histo_info(char *buf, char **start, off_t offset,
				  int len, int *eof, void *data);
#endif

struct drm_proc_list {
	const char *name;
	int	   (*f)(char *, char **, off_t, int, int *, void *);
} drm_proc_list[] = {
	{ "name",    drm_name_info    },
	{ "mem",     drm_mem_info     },
	{ "vm",	     drm_vm_info      },
	{ "clients", drm_clients_info },
	{ "queues",  drm_queues_info  },
	{ "bufs",    drm_bufs_info    },
#if DRM_DEBUG_CODE
	{ "vma",     drm_vma_info     },
#endif
#if DRM_DMA_HISTOGRAM
	{ "histo",   drm_histo_info   },
#endif
};
#define DRM_PROC_ENTRIES (sizeof(drm_proc_list)/sizeof(drm_proc_list[0]))

int drm_proc_init(drm_device_t *dev)
{
	struct proc_dir_entry *ent;
	int		      i, j;

	drm_root = create_proc_entry("dri", S_IFDIR, NULL);
	if (!drm_root) {
		DRM_ERROR("Cannot create /proc/dri\n");
		return -1;
	}

				/* Instead of doing this search, we should
				   add some global support for /proc/dri. */
	for (i = 0; i < 8; i++) {
		sprintf(drm_slot_name, "dri/%d", i);
		drm_dev_root = create_proc_entry(drm_slot_name, S_IFDIR, NULL);
		if (!drm_dev_root) {
			DRM_ERROR("Cannot create /proc/%s\n", drm_slot_name);
			remove_proc_entry("dri", NULL);
			break;
		}
		if (drm_dev_root->nlink == 2) break;
		drm_dev_root = NULL;
	}
	if (!drm_dev_root) {
		DRM_ERROR("Cannot find slot in /proc/dri\n");
		return -1;
	}

	for (i = 0; i < DRM_PROC_ENTRIES; i++) {
		ent = create_proc_entry(drm_proc_list[i].name,
					S_IFREG|S_IRUGO, drm_dev_root);
		if (!ent) {
			DRM_ERROR("Cannot create /proc/%s/%s\n",
				  drm_slot_name, drm_proc_list[i].name);
			for (j = 0; j < i; j++)
				remove_proc_entry(drm_proc_list[i].name,
						  drm_dev_root);
			remove_proc_entry(drm_slot_name, NULL);
			remove_proc_entry("dri", NULL);
			return -1;
		}
		ent->read_proc = drm_proc_list[i].f;
		ent->data      = dev;
	}

	return 0;
}


int drm_proc_cleanup(void)
{
	int i;
	
	if (drm_root) {
		if (drm_dev_root) {
			for (i = 0; i < DRM_PROC_ENTRIES; i++) {
				remove_proc_entry(drm_proc_list[i].name,
						  drm_dev_root);
			}
			remove_proc_entry(drm_slot_name, NULL);
		}
		remove_proc_entry("dri", NULL);
		remove_proc_entry(DRM_NAME, NULL);
	}
	drm_root = drm_dev_root = NULL;
	return 0;
}

static int drm_name_info(char *buf, char **start, off_t offset, int len,
			 int *eof, void *data)
{
	drm_device_t *dev = (drm_device_t *)data;

	if (offset > 0) return 0; /* no partial requests */
	len  = 0;
	*eof = 1;

	if (dev->unique) {
		DRM_PROC_PRINT("%s 0x%x %s\n",
			       dev->name, dev->device, dev->unique);
	} else {
		DRM_PROC_PRINT("%s 0x%x\n", dev->name, dev->device);
	}
	return len;
}

static int _drm_vm_info(char *buf, char **start, off_t offset, int len,
			int *eof, void *data)
{
	drm_device_t *dev = (drm_device_t *)data;
	drm_map_t    *map;
				/* Hardcoded from _DRM_FRAME_BUFFER,
                                   _DRM_REGISTERS, _DRM_SHM, and
                                   _DRM_AGP. */
	const char   *types[] = { "FB", "REG", "SHM", "AGP" };
	const char   *type;
	int	     i;

	if (offset > 0) return 0; /* no partial requests */
	len  = 0;
	*eof = 1;
	DRM_PROC_PRINT("slot	 offset	      size type flags	 "
		       "address mtrr\n\n");
	for (i = 0; i < dev->map_count; i++) {
		map = dev->maplist[i];
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
	}

	return len;
}

static int drm_vm_info(char *buf, char **start, off_t offset, int len,
		       int *eof, void *data)
{
	drm_device_t *dev = (drm_device_t *)data;
	int	     ret;

	down(&dev->struct_sem);
	ret = _drm_vm_info(buf, start, offset, len, eof, data);
	up(&dev->struct_sem);
	return ret;
}


static int _drm_queues_info(char *buf, char **start, off_t offset, int len,
			    int *eof, void *data)
{
	drm_device_t *dev = (drm_device_t *)data;
	int	     i;
	drm_queue_t  *q;

	if (offset > 0) return 0; /* no partial requests */
	len  = 0;
	*eof = 1;
	DRM_PROC_PRINT("  ctx/flags   use   fin"
		       "   blk/rw/rwf  wait    flushed	   queued"
		       "      locks\n\n");
	for (i = 0; i < dev->queue_count; i++) {
		q = dev->queuelist[i];
		atomic_inc(&q->use_count);
		DRM_PROC_PRINT_RET(atomic_dec(&q->use_count),
				   "%5d/0x%03x %5d %5d"
				   " %5d/%c%c/%c%c%c %5Zd %10d %10d %10d\n",
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
				   DRM_BUFCOUNT(&q->waitlist),
				   atomic_read(&q->total_flushed),
				   atomic_read(&q->total_queued),
				   atomic_read(&q->total_locks));
		atomic_dec(&q->use_count);
	}
	
	return len;
}

static int drm_queues_info(char *buf, char **start, off_t offset, int len,
			   int *eof, void *data)
{
	drm_device_t *dev = (drm_device_t *)data;
	int	     ret;

	down(&dev->struct_sem);
	ret = _drm_queues_info(buf, start, offset, len, eof, data);
	up(&dev->struct_sem);
	return ret;
}

/* drm_bufs_info is called whenever a process reads
   /dev/drm/<dev>/bufs. */

static int _drm_bufs_info(char *buf, char **start, off_t offset, int len,
			  int *eof, void *data)
{
	drm_device_t	 *dev = (drm_device_t *)data;
	drm_device_dma_t *dma = dev->dma;
	int		 i;

	if (!dma)	return 0;
	if (offset > 0) return 0; /* no partial requests */
	len  = 0;
	*eof = 1;
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

	return len;
}

static int drm_bufs_info(char *buf, char **start, off_t offset, int len,
			 int *eof, void *data)
{
	drm_device_t *dev = (drm_device_t *)data;
	int	     ret;

	down(&dev->struct_sem);
	ret = _drm_bufs_info(buf, start, offset, len, eof, data);
	up(&dev->struct_sem);
	return ret;
}


static int _drm_clients_info(char *buf, char **start, off_t offset, int len,
			     int *eof, void *data)
{
	drm_device_t *dev = (drm_device_t *)data;
	drm_file_t   *priv;

	if (offset > 0) return 0; /* no partial requests */
	len  = 0;
	*eof = 1;
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

	return len;
}

static int drm_clients_info(char *buf, char **start, off_t offset, int len,
			    int *eof, void *data)
{
	drm_device_t *dev = (drm_device_t *)data;
	int	     ret;

	down(&dev->struct_sem);
	ret = _drm_clients_info(buf, start, offset, len, eof, data);
	up(&dev->struct_sem);
	return ret;
}

#if DRM_DEBUG_CODE

#define DRM_VMA_VERBOSE 0

static int _drm_vma_info(char *buf, char **start, off_t offset, int len,
			 int *eof, void *data)
{
	drm_device_t	      *dev = (drm_device_t *)data;
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

	if (offset > 0) return 0; /* no partial requests */
	len  = 0;
	*eof = 1;
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
	
	return len;
}

static int drm_vma_info(char *buf, char **start, off_t offset, int len,
			int *eof, void *data)
{
	drm_device_t *dev = (drm_device_t *)data;
	int	     ret;

	down(&dev->struct_sem);
	ret = _drm_vma_info(buf, start, offset, len, eof, data);
	up(&dev->struct_sem);
	return ret;
}
#endif


#if DRM_DMA_HISTOGRAM
static int _drm_histo_info(char *buf, char **start, off_t offset, int len,
			   int *eof, void *data)
{
	drm_device_t	 *dev = (drm_device_t *)data;
	drm_device_dma_t *dma = dev->dma;
	int		 i;
	unsigned long	 slot_value = DRM_DMA_HISTOGRAM_INITIAL;
	unsigned long	 prev_value = 0;
	drm_buf_t	 *buffer;

	if (offset > 0) return 0; /* no partial requests */
	len  = 0;
	*eof = 1;

	DRM_PROC_PRINT("general statistics:\n");
	DRM_PROC_PRINT("total	 %10u\n", atomic_read(&dev->histo.total));
	DRM_PROC_PRINT("open	 %10u\n", atomic_read(&dev->total_open));
	DRM_PROC_PRINT("close	 %10u\n", atomic_read(&dev->total_close));
	DRM_PROC_PRINT("ioctl	 %10u\n", atomic_read(&dev->total_ioctl));
	DRM_PROC_PRINT("irq	 %10u\n", atomic_read(&dev->total_irq));
	DRM_PROC_PRINT("ctx	 %10u\n", atomic_read(&dev->total_ctx));
	
	DRM_PROC_PRINT("\nlock statistics:\n");
	DRM_PROC_PRINT("locks	 %10u\n", atomic_read(&dev->total_locks));
	DRM_PROC_PRINT("unlocks	 %10u\n", atomic_read(&dev->total_unlocks));
	DRM_PROC_PRINT("contends %10u\n", atomic_read(&dev->total_contends));
	DRM_PROC_PRINT("sleeps	 %10u\n", atomic_read(&dev->total_sleeps));


	if (dma) {
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
	return len;
}

static int drm_histo_info(char *buf, char **start, off_t offset, int len,
			  int *eof, void *data)
{
	drm_device_t *dev = (drm_device_t *)data;
	int	     ret;

	down(&dev->struct_sem);
	ret = _drm_histo_info(buf, start, offset, len, eof, data);
	up(&dev->struct_sem);
	return ret;
}
#endif
