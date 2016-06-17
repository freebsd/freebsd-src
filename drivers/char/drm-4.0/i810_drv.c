/* i810_drv.c -- I810 driver -*- linux-c -*-
 * Created: Mon Dec 13 01:56:22 1999 by jhartmann@precisioninsight.com
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
 * Authors: Rickard E. (Rik) Faith <faith@valinux.com>
 *	    Jeff Hartmann <jhartmann@valinux.com>
 *
 */

#include <linux/config.h>
#include "drmP.h"
#include "i810_drv.h"

#define I810_NAME	 "i810"
#define I810_DESC	 "Intel I810"
#define I810_DATE	 "20000928"
#define I810_MAJOR	 1
#define I810_MINOR	 1
#define I810_PATCHLEVEL	 0

static drm_device_t	      i810_device;
drm_ctx_t		      i810_res_ctx;

static struct file_operations i810_fops = {
#if LINUX_VERSION_CODE >= 0x020400
				/* This started being used during 2.4.0-test */
	owner:   THIS_MODULE,
#endif
	open:	 i810_open,
	flush:	 drm_flush,
	release: i810_release,
	ioctl:	 i810_ioctl,
	mmap:	 drm_mmap,
	read:	 drm_read,
	fasync:	 drm_fasync,
      	poll:	 drm_poll,
};

static struct miscdevice      i810_misc = {
	minor: MISC_DYNAMIC_MINOR,
	name:  I810_NAME,
	fops:  &i810_fops,
};

static drm_ioctl_desc_t	      i810_ioctls[] = {
	[DRM_IOCTL_NR(DRM_IOCTL_VERSION)]     = { i810_version,	  0, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_GET_UNIQUE)]  = { drm_getunique,  0, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_GET_MAGIC)]   = { drm_getmagic,	  0, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_IRQ_BUSID)]   = { drm_irq_busid,  0, 1 },

	[DRM_IOCTL_NR(DRM_IOCTL_SET_UNIQUE)]  = { drm_setunique,  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_BLOCK)]	      = { drm_block,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_UNBLOCK)]     = { drm_unblock,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_CONTROL)]     = { i810_control,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_AUTH_MAGIC)]  = { drm_authmagic,  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_ADD_MAP)]     = { drm_addmap,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_ADD_BUFS)]    = { i810_addbufs,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_MARK_BUFS)]   = { i810_markbufs,  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_INFO_BUFS)]   = { i810_infobufs,  1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_FREE_BUFS)]   = { i810_freebufs,  1, 0 },

	[DRM_IOCTL_NR(DRM_IOCTL_ADD_CTX)]     = { i810_addctx,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_RM_CTX)]      = { i810_rmctx,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_MOD_CTX)]     = { i810_modctx,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_GET_CTX)]     = { i810_getctx,	  1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_SWITCH_CTX)]  = { i810_switchctx,  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_NEW_CTX)]     = { i810_newctx,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_RES_CTX)]     = { i810_resctx,	  1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_ADD_DRAW)]    = { drm_adddraw,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_RM_DRAW)]     = { drm_rmdraw,	  1, 1 },

	[DRM_IOCTL_NR(DRM_IOCTL_LOCK)]	      = { i810_lock,	  1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_UNLOCK)]      = { i810_unlock,	  1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_FINISH)]      = { drm_finish,	  1, 0 },

	[DRM_IOCTL_NR(DRM_IOCTL_AGP_ACQUIRE)] = { drm_agp_acquire, 1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_RELEASE)] = { drm_agp_release, 1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_ENABLE)]  = { drm_agp_enable,  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_INFO)]    = { drm_agp_info,    1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_ALLOC)]   = { drm_agp_alloc,   1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_FREE)]    = { drm_agp_free,    1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_BIND)]    = { drm_agp_bind,    1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_UNBIND)]  = { drm_agp_unbind,  1, 1 },

   	[DRM_IOCTL_NR(DRM_IOCTL_I810_INIT)]   = { i810_dma_init,   1, 1 },
   	[DRM_IOCTL_NR(DRM_IOCTL_I810_VERTEX)] = { i810_dma_vertex, 1, 0 },
   	[DRM_IOCTL_NR(DRM_IOCTL_I810_CLEAR)]  = { i810_clear_bufs, 1, 0 },
      	[DRM_IOCTL_NR(DRM_IOCTL_I810_FLUSH)]  = { i810_flush_ioctl,1, 0 },
   	[DRM_IOCTL_NR(DRM_IOCTL_I810_GETAGE)] = { i810_getage,     1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_I810_GETBUF)] = { i810_getbuf,     1, 0 },
   	[DRM_IOCTL_NR(DRM_IOCTL_I810_SWAP)]   = { i810_swap_bufs,  1, 0 },
   	[DRM_IOCTL_NR(DRM_IOCTL_I810_COPY)]   = { i810_copybuf,    1, 0 },
   	[DRM_IOCTL_NR(DRM_IOCTL_I810_DOCOPY)] = { i810_docopy,     1, 0 },
};

#define I810_IOCTL_COUNT DRM_ARRAY_SIZE(i810_ioctls)

#ifdef MODULE
static char		      *i810 = NULL;
#endif

MODULE_AUTHOR("VA Linux Systems, Inc.");
MODULE_DESCRIPTION("Intel I810");
MODULE_LICENSE("GPL and additional rights");
MODULE_PARM(i810, "s");

#ifndef MODULE
/* i810_options is called by the kernel to parse command-line options
 * passed via the boot-loader (e.g., LILO).  It calls the insmod option
 * routine, drm_parse_drm.
 */

static int __init i810_options(char *str)
{
	drm_parse_options(str);
	return 1;
}

__setup("i810=", i810_options);
#endif

static int i810_setup(drm_device_t *dev)
{
	int i;

	atomic_set(&dev->ioctl_count, 0);
	atomic_set(&dev->vma_count, 0);
	dev->buf_use	  = 0;
	atomic_set(&dev->buf_alloc, 0);

	drm_dma_setup(dev);

	atomic_set(&dev->total_open, 0);
	atomic_set(&dev->total_close, 0);
	atomic_set(&dev->total_ioctl, 0);
	atomic_set(&dev->total_irq, 0);
	atomic_set(&dev->total_ctx, 0);
	atomic_set(&dev->total_locks, 0);
	atomic_set(&dev->total_unlocks, 0);
	atomic_set(&dev->total_contends, 0);
	atomic_set(&dev->total_sleeps, 0);

	for (i = 0; i < DRM_HASH_SIZE; i++) {
		dev->magiclist[i].head = NULL;
		dev->magiclist[i].tail = NULL;
	}
	dev->maplist	    = NULL;
	dev->map_count	    = 0;
	dev->vmalist	    = NULL;
	dev->lock.hw_lock   = NULL;
	init_waitqueue_head(&dev->lock.lock_queue);
	dev->queue_count    = 0;
	dev->queue_reserved = 0;
	dev->queue_slots    = 0;
	dev->queuelist	    = NULL;
	dev->irq	    = 0;
	dev->context_flag   = 0;
	dev->interrupt_flag = 0;
	dev->dma_flag	    = 0;
	dev->last_context   = 0;
	dev->last_switch    = 0;
	dev->last_checked   = 0;
	init_timer(&dev->timer);
	init_waitqueue_head(&dev->context_wait);
#if DRM_DMA_HISTO
	memset(&dev->histo, 0, sizeof(dev->histo));
#endif
	dev->ctx_start	    = 0;
	dev->lck_start	    = 0;

	dev->buf_rp	  = dev->buf;
	dev->buf_wp	  = dev->buf;
	dev->buf_end	  = dev->buf + DRM_BSZ;
	dev->buf_async	  = NULL;
	init_waitqueue_head(&dev->buf_readers);
	init_waitqueue_head(&dev->buf_writers);

	DRM_DEBUG("\n");

	/* The kernel's context could be created here, but is now created
	   in drm_dma_enqueue.	This is more resource-efficient for
	   hardware that does not do DMA, but may mean that
	   drm_select_queue fails between the time the interrupt is
	   initialized and the time the queues are initialized. */

	return 0;
}


static int i810_takedown(drm_device_t *dev)
{
	int		  i;
	drm_magic_entry_t *pt, *next;
	drm_map_t	  *map;
	drm_vma_entry_t	  *vma, *vma_next;

	DRM_DEBUG("\n");

	if (dev->irq) i810_irq_uninstall(dev);

	down(&dev->struct_sem);
	del_timer(&dev->timer);

	if (dev->devname) {
		drm_free(dev->devname, strlen(dev->devname)+1, DRM_MEM_DRIVER);
		dev->devname = NULL;
	}

	if (dev->unique) {
		drm_free(dev->unique, strlen(dev->unique)+1, DRM_MEM_DRIVER);
		dev->unique = NULL;
		dev->unique_len = 0;
	}
				/* Clear pid list */
	for (i = 0; i < DRM_HASH_SIZE; i++) {
		for (pt = dev->magiclist[i].head; pt; pt = next) {
			next = pt->next;
			drm_free(pt, sizeof(*pt), DRM_MEM_MAGIC);
		}
		dev->magiclist[i].head = dev->magiclist[i].tail = NULL;
	}
   				/* Clear AGP information */
	if (dev->agp) {
		drm_agp_mem_t *entry;
		drm_agp_mem_t *nexte;

				/* Remove AGP resources, but leave dev->agp
                                   intact until r128_cleanup is called. */
		for (entry = dev->agp->memory; entry; entry = nexte) {
			nexte = entry->next;
			if (entry->bound) drm_unbind_agp(entry->memory);
			drm_free_agp(entry->memory, entry->pages);
			drm_free(entry, sizeof(*entry), DRM_MEM_AGPLISTS);
		}
		dev->agp->memory = NULL;

		if (dev->agp->acquired) _drm_agp_release();

		dev->agp->acquired = 0;
		dev->agp->enabled  = 0;
	}
				/* Clear vma list (only built for debugging) */
	if (dev->vmalist) {
		for (vma = dev->vmalist; vma; vma = vma_next) {
			vma_next = vma->next;
			drm_free(vma, sizeof(*vma), DRM_MEM_VMAS);
		}
		dev->vmalist = NULL;
	}

				/* Clear map area and mtrr information */
	if (dev->maplist) {
		for (i = 0; i < dev->map_count; i++) {
			map = dev->maplist[i];
			switch (map->type) {
			case _DRM_REGISTERS:
			case _DRM_FRAME_BUFFER:
#ifdef CONFIG_MTRR
				if (map->mtrr >= 0) {
					int retcode;
					retcode = mtrr_del(map->mtrr,
							   map->offset,
							   map->size);
					DRM_DEBUG("mtrr_del = %d\n", retcode);
				}
#endif
				drm_ioremapfree(map->handle, map->size, dev);
				break;
			case _DRM_SHM:
				drm_free_pages((unsigned long)map->handle,
					       drm_order(map->size)
					       - PAGE_SHIFT,
					       DRM_MEM_SAREA);
				break;
			case _DRM_AGP:
				break;
			}
			drm_free(map, sizeof(*map), DRM_MEM_MAPS);
		}
		drm_free(dev->maplist,
			 dev->map_count * sizeof(*dev->maplist),
			 DRM_MEM_MAPS);
		dev->maplist   = NULL;
		dev->map_count = 0;
	}

	if (dev->queuelist) {
		for (i = 0; i < dev->queue_count; i++) {
			drm_waitlist_destroy(&dev->queuelist[i]->waitlist);
			if (dev->queuelist[i]) {
				drm_free(dev->queuelist[i],
					 sizeof(*dev->queuelist[0]),
					 DRM_MEM_QUEUES);
				dev->queuelist[i] = NULL;
			}
		}
		drm_free(dev->queuelist,
			 dev->queue_slots * sizeof(*dev->queuelist),
			 DRM_MEM_QUEUES);
		dev->queuelist	 = NULL;
	}

	drm_dma_takedown(dev);

	dev->queue_count     = 0;
	if (dev->lock.hw_lock) {
		dev->lock.hw_lock    = NULL; /* SHM removed */
		dev->lock.pid	     = 0;
		wake_up_interruptible(&dev->lock.lock_queue);
	}
	up(&dev->struct_sem);

	return 0;
}

/* i810_init is called via init_module at module load time, or via
 * linux/init/main.c (this is not currently supported). */

static int __init i810_init(void)
{
	int		      retcode;
	drm_device_t	      *dev = &i810_device;

	DRM_DEBUG("\n");

	memset((void *)dev, 0, sizeof(*dev));
	dev->count_lock	  = SPIN_LOCK_UNLOCKED;
	sema_init(&dev->struct_sem, 1);

#ifdef MODULE
	drm_parse_options(i810);
#endif
	DRM_DEBUG("doing misc_register\n");
	if ((retcode = misc_register(&i810_misc))) {
		DRM_ERROR("Cannot register \"%s\"\n", I810_NAME);
		return retcode;
	}
	dev->device = MKDEV(MISC_MAJOR, i810_misc.minor);
	dev->name   = I810_NAME;

   	DRM_DEBUG("doing mem init\n");
	drm_mem_init();
	DRM_DEBUG("doing proc init\n");
	drm_proc_init(dev);
	DRM_DEBUG("doing agp init\n");
	dev->agp    = drm_agp_init();
   	if(dev->agp == NULL) {
	   	DRM_INFO("The i810 drm module requires the agpgart module"
			 " to function correctly\nPlease load the agpgart"
			 " module before you load the i810 module\n");
	   	drm_proc_cleanup();
	   	misc_deregister(&i810_misc);
	   	i810_takedown(dev);
	   	return -ENOMEM;
	}
	DRM_DEBUG("doing ctxbitmap init\n");
	if((retcode = drm_ctxbitmap_init(dev))) {
		DRM_ERROR("Cannot allocate memory for context bitmap.\n");
		drm_proc_cleanup();
		misc_deregister(&i810_misc);
		i810_takedown(dev);
		return retcode;
	}

	DRM_INFO("Initialized %s %d.%d.%d %s on minor %d\n",
		 I810_NAME,
		 I810_MAJOR,
		 I810_MINOR,
		 I810_PATCHLEVEL,
		 I810_DATE,
		 i810_misc.minor);

	return 0;
}

/* i810_cleanup is called via cleanup_module at module unload time. */

static void __exit i810_cleanup(void)
{
	drm_device_t	      *dev = &i810_device;

	DRM_DEBUG("\n");

	drm_proc_cleanup();
	if (misc_deregister(&i810_misc)) {
		DRM_ERROR("Cannot unload module\n");
	} else {
		DRM_INFO("Module unloaded\n");
	}
	drm_ctxbitmap_cleanup(dev);
	i810_takedown(dev);
	if (dev->agp) {
		drm_agp_uninit();
		drm_free(dev->agp, sizeof(*dev->agp), DRM_MEM_AGPLISTS);
		dev->agp = NULL;
	}
}

module_init(i810_init);
module_exit(i810_cleanup);


int i810_version(struct inode *inode, struct file *filp, unsigned int cmd,
		  unsigned long arg)
{
	drm_version_t version;
	int	      len;

	if (copy_from_user(&version,
			   (drm_version_t *)arg,
			   sizeof(version)))
		return -EFAULT;

#define DRM_COPY(name,value)				     \
	len = strlen(value);				     \
	if (len > name##_len) len = name##_len;		     \
	name##_len = strlen(value);			     \
	if (len && name) {				     \
		if (copy_to_user(name, value, len))          \
			return -EFAULT;			     \
	}

	version.version_major	   = I810_MAJOR;
	version.version_minor	   = I810_MINOR;
	version.version_patchlevel = I810_PATCHLEVEL;

	DRM_COPY(version.name, I810_NAME);
	DRM_COPY(version.date, I810_DATE);
	DRM_COPY(version.desc, I810_DESC);

	if (copy_to_user((drm_version_t *)arg,
			 &version,
			 sizeof(version)))
		return -EFAULT;
	return 0;
}

int i810_open(struct inode *inode, struct file *filp)
{
	drm_device_t  *dev    = &i810_device;
	int	      retcode = 0;

	DRM_DEBUG("open_count = %d\n", dev->open_count);
	if (!(retcode = drm_open_helper(inode, filp, dev))) {
#if LINUX_VERSION_CODE < 0x020333
		MOD_INC_USE_COUNT; /* Needed before Linux 2.3.51 */
#endif
		atomic_inc(&dev->total_open);
		spin_lock(&dev->count_lock);
		if (!dev->open_count++) {
			spin_unlock(&dev->count_lock);
			return i810_setup(dev);
		}
		spin_unlock(&dev->count_lock);
	}
	return retcode;
}

int i810_release(struct inode *inode, struct file *filp)
{
	drm_file_t    *priv   = filp->private_data;
	drm_device_t  *dev;
	int	      retcode = 0;

	lock_kernel();
	dev    = priv->dev;
	DRM_DEBUG("pid = %d, device = 0x%x, open_count = %d\n",
		  current->pid, dev->device, dev->open_count);

	if (dev->lock.hw_lock && _DRM_LOCK_IS_HELD(dev->lock.hw_lock->lock)
	    && dev->lock.pid == current->pid) {
	      	i810_reclaim_buffers(dev, priv->pid);
		DRM_ERROR("Process %d dead, freeing lock for context %d\n",
			  current->pid,
			  _DRM_LOCKING_CONTEXT(dev->lock.hw_lock->lock));
		drm_lock_free(dev,
			      &dev->lock.hw_lock->lock,
			      _DRM_LOCKING_CONTEXT(dev->lock.hw_lock->lock));

				/* FIXME: may require heavy-handed reset of
                                   hardware at this point, possibly
                                   processed via a callback to the X
                                   server. */
	} else if (dev->lock.hw_lock) {
	   	/* The lock is required to reclaim buffers */
	   	DECLARE_WAITQUEUE(entry, current);
	   	add_wait_queue(&dev->lock.lock_queue, &entry);
		for (;;) {
			current->state = TASK_INTERRUPTIBLE;
			if (!dev->lock.hw_lock) {
				/* Device has been unregistered */
				retcode = -EINTR;
				break;
			}
			if (drm_lock_take(&dev->lock.hw_lock->lock,
					  DRM_KERNEL_CONTEXT)) {
				dev->lock.pid	    = priv->pid;
				dev->lock.lock_time = jiffies;
				atomic_inc(&dev->total_locks);
				break;	/* Got lock */
			}
				/* Contention */
			atomic_inc(&dev->total_sleeps);
			schedule();
			if (signal_pending(current)) {
				retcode = -ERESTARTSYS;
				break;
			}
		}
		current->state = TASK_RUNNING;
		remove_wait_queue(&dev->lock.lock_queue, &entry);
	   	if(!retcode) {
		   	i810_reclaim_buffers(dev, priv->pid);
		   	drm_lock_free(dev, &dev->lock.hw_lock->lock,
				      DRM_KERNEL_CONTEXT);
		}
	}
	drm_fasync(-1, filp, 0);

	down(&dev->struct_sem);
	if (priv->prev) priv->prev->next = priv->next;
	else		dev->file_first	 = priv->next;
	if (priv->next) priv->next->prev = priv->prev;
	else		dev->file_last	 = priv->prev;
	up(&dev->struct_sem);

	drm_free(priv, sizeof(*priv), DRM_MEM_FILES);
#if LINUX_VERSION_CODE < 0x020333
	MOD_DEC_USE_COUNT; /* Needed before Linux 2.3.51 */
#endif
   	atomic_inc(&dev->total_close);
   	spin_lock(&dev->count_lock);
   	if (!--dev->open_count) {
	   	if (atomic_read(&dev->ioctl_count) || dev->blocked) {
		   	DRM_ERROR("Device busy: %d %d\n",
				  atomic_read(&dev->ioctl_count),
				  dev->blocked);
		   	spin_unlock(&dev->count_lock);
			unlock_kernel();
		   	return -EBUSY;
		}
	   	spin_unlock(&dev->count_lock);
		unlock_kernel();
		return i810_takedown(dev);
	}
	spin_unlock(&dev->count_lock);
	unlock_kernel();
	return retcode;
}

/* drm_ioctl is called whenever a process performs an ioctl on /dev/drm. */

int i810_ioctl(struct inode *inode, struct file *filp, unsigned int cmd,
		unsigned long arg)
{
	int		 nr	 = DRM_IOCTL_NR(cmd);
	drm_file_t	 *priv	 = filp->private_data;
	drm_device_t	 *dev	 = priv->dev;
	int		 retcode = 0;
	drm_ioctl_desc_t *ioctl;
	drm_ioctl_t	 *func;

	atomic_inc(&dev->ioctl_count);
	atomic_inc(&dev->total_ioctl);
	++priv->ioctl_count;

	DRM_DEBUG("pid = %d, cmd = 0x%02x, nr = 0x%02x, dev 0x%x, auth = %d\n",
		  current->pid, cmd, nr, dev->device, priv->authenticated);

	if (nr >= I810_IOCTL_COUNT) {
		retcode = -EINVAL;
	} else {
		ioctl	  = &i810_ioctls[nr];
		func	  = ioctl->func;

		if (!func) {
			DRM_DEBUG("no function\n");
			retcode = -EINVAL;
		} else if ((ioctl->root_only && !capable(CAP_SYS_ADMIN))
			    || (ioctl->auth_needed && !priv->authenticated)) {
			retcode = -EACCES;
		} else {
			retcode = (func)(inode, filp, cmd, arg);
		}
	}

	atomic_dec(&dev->ioctl_count);
	return retcode;
}

int i810_unlock(struct inode *inode, struct file *filp, unsigned int cmd,
		 unsigned long arg)
{
	drm_file_t	  *priv	  = filp->private_data;
	drm_device_t	  *dev	  = priv->dev;
	drm_lock_t	  lock;

	if (copy_from_user(&lock, (drm_lock_t *)arg, sizeof(lock)))
		return -EFAULT;

	if (lock.context == DRM_KERNEL_CONTEXT) {
		DRM_ERROR("Process %d using kernel context %d\n",
			  current->pid, lock.context);
		return -EINVAL;
	}

	DRM_DEBUG("%d frees lock (%d holds)\n",
		  lock.context,
		  _DRM_LOCKING_CONTEXT(dev->lock.hw_lock->lock));
	atomic_inc(&dev->total_unlocks);
	if (_DRM_LOCK_IS_CONT(dev->lock.hw_lock->lock))
		atomic_inc(&dev->total_contends);
   	drm_lock_transfer(dev, &dev->lock.hw_lock->lock, DRM_KERNEL_CONTEXT);
	if (!dev->context_flag) {
		if (drm_lock_free(dev, &dev->lock.hw_lock->lock,
				  DRM_KERNEL_CONTEXT)) {
			DRM_ERROR("\n");
		}
	}
#if DRM_DMA_HISTOGRAM
	atomic_inc(&dev->histo.lhld[drm_histogram_slot(get_cycles()
						       - dev->lck_start)]);
#endif

	unblock_all_signals();
	return 0;
}
