/* tdfx_drv.c -- tdfx driver -*- linux-c -*-
 * Created: Thu Oct  7 10:38:32 1999 by faith@precisioninsight.com
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
 *    Daryll Strauss <daryll@valinux.com>
 *
 */

#include <linux/config.h>
#include "drmP.h"
#include "tdfx_drv.h"

#define TDFX_NAME	 "tdfx"
#define TDFX_DESC	 "3dfx Banshee/Voodoo3+"
#define TDFX_DATE	 "20000928"
#define TDFX_MAJOR	 1
#define TDFX_MINOR	 0
#define TDFX_PATCHLEVEL  0

static drm_device_t	      tdfx_device;
drm_ctx_t	              tdfx_res_ctx;

static struct file_operations tdfx_fops = {
#if LINUX_VERSION_CODE >= 0x020400
				/* This started being used during 2.4.0-test */
	owner:   THIS_MODULE,
#endif
	open:	 tdfx_open,
	flush:	 drm_flush,
	release: tdfx_release,
	ioctl:	 tdfx_ioctl,
	mmap:	 drm_mmap,
	read:	 drm_read,
	fasync:	 drm_fasync,
	poll:	 drm_poll,
};

static struct miscdevice      tdfx_misc = {
	minor: MISC_DYNAMIC_MINOR,
	name:  TDFX_NAME,
	fops:  &tdfx_fops,
};

static drm_ioctl_desc_t	      tdfx_ioctls[] = {
	[DRM_IOCTL_NR(DRM_IOCTL_VERSION)]    = { tdfx_version,	  0, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_GET_UNIQUE)] = { drm_getunique,	  0, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_GET_MAGIC)]  = { drm_getmagic,	  0, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_IRQ_BUSID)]  = { drm_irq_busid,	  0, 1 },

	[DRM_IOCTL_NR(DRM_IOCTL_SET_UNIQUE)] = { drm_setunique,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_BLOCK)]	     = { drm_block,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_UNBLOCK)]    = { drm_unblock,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_AUTH_MAGIC)] = { drm_authmagic,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_ADD_MAP)]    = { drm_addmap,	  1, 1 },

	[DRM_IOCTL_NR(DRM_IOCTL_ADD_CTX)]    = { tdfx_addctx,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_RM_CTX)]     = { tdfx_rmctx,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_MOD_CTX)]    = { tdfx_modctx,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_GET_CTX)]    = { tdfx_getctx,	  1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_SWITCH_CTX)] = { tdfx_switchctx,  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_NEW_CTX)]    = { tdfx_newctx,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_RES_CTX)]    = { tdfx_resctx,	  1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_ADD_DRAW)]   = { drm_adddraw,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_RM_DRAW)]    = { drm_rmdraw,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_LOCK)]	     = { tdfx_lock,	  1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_UNLOCK)]     = { tdfx_unlock,	  1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_FINISH)]     = { drm_finish,	  1, 0 },
#if defined(CONFIG_AGP) || defined(CONFIG_AGP_MODULE)
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_ACQUIRE)]   = {drm_agp_acquire, 1, 1},
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_RELEASE)]   = {drm_agp_release, 1, 1},
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_ENABLE)]    = {drm_agp_enable,  1, 1},
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_INFO)]      = {drm_agp_info,    1, 1},
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_ALLOC)]     = {drm_agp_alloc,   1, 1},
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_FREE)]      = {drm_agp_free,    1, 1},
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_BIND)]      = {drm_agp_unbind,  1, 1},
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_UNBIND)]    = {drm_agp_bind,    1, 1},
#endif
};
#define TDFX_IOCTL_COUNT DRM_ARRAY_SIZE(tdfx_ioctls)

#ifdef MODULE
static char		      *tdfx = NULL;
#endif

MODULE_AUTHOR("VA Linux Systems, Inc.");
MODULE_LICENSE("GPL and additional rights");
MODULE_DESCRIPTION("tdfx");
MODULE_PARM(tdfx, "s");

#ifndef MODULE
/* tdfx_options is called by the kernel to parse command-line options
 * passed via the boot-loader (e.g., LILO).  It calls the insmod option
 * routine, drm_parse_drm.
 */

static int __init tdfx_options(char *str)
{
	drm_parse_options(str);
	return 1;
}

__setup("tdfx=", tdfx_options);
#endif

static int tdfx_setup(drm_device_t *dev)
{
	int i;

	atomic_set(&dev->ioctl_count, 0);
	atomic_set(&dev->vma_count, 0);
	dev->buf_use	  = 0;
	atomic_set(&dev->buf_alloc, 0);

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
	dev->dma            = 0;
	dev->dma_flag	    = 0;
	dev->last_context   = 0;
	dev->last_switch    = 0;
	dev->last_checked   = 0;
	init_timer(&dev->timer);
	init_waitqueue_head(&dev->context_wait);

	dev->ctx_start	    = 0;
	dev->lck_start	    = 0;

	dev->buf_rp	  = dev->buf;
	dev->buf_wp	  = dev->buf;
	dev->buf_end	  = dev->buf + DRM_BSZ;
	dev->buf_async	  = NULL;
	init_waitqueue_head(&dev->buf_readers);
	init_waitqueue_head(&dev->buf_writers);

	tdfx_res_ctx.handle=-1;

	DRM_DEBUG("\n");

	/* The kernel's context could be created here, but is now created
	   in drm_dma_enqueue.	This is more resource-efficient for
	   hardware that does not do DMA, but may mean that
	   drm_select_queue fails between the time the interrupt is
	   initialized and the time the queues are initialized. */

	return 0;
}


static int tdfx_takedown(drm_device_t *dev)
{
	int		  i;
	drm_magic_entry_t *pt, *next;
	drm_map_t	  *map;
	drm_vma_entry_t	  *vma, *vma_next;

	DRM_DEBUG("\n");

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
#if defined(CONFIG_AGP) || defined(CONFIG_AGP_MODULE)
				/* Clear AGP information */
	if (dev->agp) {
		drm_agp_mem_t *temp;
		drm_agp_mem_t *temp_next;

		temp = dev->agp->memory;
		while(temp != NULL) {
			temp_next = temp->next;
			drm_free_agp(temp->memory, temp->pages);
			drm_free(temp, sizeof(*temp), DRM_MEM_AGPLISTS);
			temp = temp_next;
		}
		if (dev->agp->acquired) _drm_agp_release();
	}
#endif
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
				/* Do nothing here, because this is all
                                   handled in the AGP/GART driver. */
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

	if (dev->lock.hw_lock) {
		dev->lock.hw_lock    = NULL; /* SHM removed */
		dev->lock.pid	     = 0;
		wake_up_interruptible(&dev->lock.lock_queue);
	}
	up(&dev->struct_sem);

	return 0;
}

/* tdfx_init is called via init_module at module load time, or via
 * linux/init/main.c (this is not currently supported). */

static int __init tdfx_init(void)
{
	int		      retcode;
	drm_device_t	      *dev = &tdfx_device;

	DRM_DEBUG("\n");

	memset((void *)dev, 0, sizeof(*dev));
	dev->count_lock	  = SPIN_LOCK_UNLOCKED;
	sema_init(&dev->struct_sem, 1);

#ifdef MODULE
	drm_parse_options(tdfx);
#endif

	if ((retcode = misc_register(&tdfx_misc))) {
		DRM_ERROR("Cannot register \"%s\"\n", TDFX_NAME);
		return retcode;
	}
	dev->device = MKDEV(MISC_MAJOR, tdfx_misc.minor);
	dev->name   = TDFX_NAME;

	drm_mem_init();
	drm_proc_init(dev);
#if defined(CONFIG_AGP) || defined(CONFIG_AGP_MODULE)
	dev->agp    = drm_agp_init();
#endif
	if((retcode = drm_ctxbitmap_init(dev))) {
		DRM_ERROR("Cannot allocate memory for context bitmap.\n");
		drm_proc_cleanup();
		misc_deregister(&tdfx_misc);
		tdfx_takedown(dev);
		return retcode;
	}

	DRM_INFO("Initialized %s %d.%d.%d %s on minor %d\n",
		 TDFX_NAME,
		 TDFX_MAJOR,
		 TDFX_MINOR,
		 TDFX_PATCHLEVEL,
		 TDFX_DATE,
		 tdfx_misc.minor);

	return 0;
}

/* tdfx_cleanup is called via cleanup_module at module unload time. */

static void __exit tdfx_cleanup(void)
{
	drm_device_t	      *dev = &tdfx_device;

	DRM_DEBUG("\n");

	drm_proc_cleanup();
	if (misc_deregister(&tdfx_misc)) {
		DRM_ERROR("Cannot unload module\n");
	} else {
		DRM_INFO("Module unloaded\n");
	}
	drm_ctxbitmap_cleanup(dev);
	tdfx_takedown(dev);
#if defined(CONFIG_AGP) || defined(CONFIG_AGP_MODULE)
	if (dev->agp) {
		drm_agp_uninit();
		drm_free(dev->agp, sizeof(*dev->agp), DRM_MEM_AGPLISTS);
		dev->agp = NULL;
	}
#endif
}

module_init(tdfx_init);
module_exit(tdfx_cleanup);


int tdfx_version(struct inode *inode, struct file *filp, unsigned int cmd,
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
		if (copy_to_user(name, value, len))	     \
			return -EFAULT;			     \
	}

	version.version_major	   = TDFX_MAJOR;
	version.version_minor	   = TDFX_MINOR;
	version.version_patchlevel = TDFX_PATCHLEVEL;

	DRM_COPY(version.name, TDFX_NAME);
	DRM_COPY(version.date, TDFX_DATE);
	DRM_COPY(version.desc, TDFX_DESC);

	if (copy_to_user((drm_version_t *)arg,
			 &version,
			 sizeof(version)))
		return -EFAULT;
	return 0;
}

int tdfx_open(struct inode *inode, struct file *filp)
{
	drm_device_t  *dev    = &tdfx_device;
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
			return tdfx_setup(dev);
		}
		spin_unlock(&dev->count_lock);
	}
	return retcode;
}

int tdfx_release(struct inode *inode, struct file *filp)
{
	drm_file_t    *priv   = filp->private_data;
	drm_device_t  *dev;
	int	      retcode = 0;

	lock_kernel();
	dev = priv->dev;

	DRM_DEBUG("open_count = %d\n", dev->open_count);
	if (!(retcode = drm_release(inode, filp))) {
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
			return tdfx_takedown(dev);
		}
		spin_unlock(&dev->count_lock);
	}

	unlock_kernel();
	return retcode;
}

/* tdfx_ioctl is called whenever a process performs an ioctl on /dev/drm. */

int tdfx_ioctl(struct inode *inode, struct file *filp, unsigned int cmd,
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

	if (nr >= TDFX_IOCTL_COUNT) {
		retcode = -EINVAL;
	} else {
		ioctl	  = &tdfx_ioctls[nr];
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

int tdfx_lock(struct inode *inode, struct file *filp, unsigned int cmd,
	      unsigned long arg)
{
        drm_file_t        *priv   = filp->private_data;
        drm_device_t      *dev    = priv->dev;
        DECLARE_WAITQUEUE(entry, current);
        int               ret   = 0;
        drm_lock_t        lock;
#if DRM_DMA_HISTOGRAM
        cycles_t          start;

        dev->lck_start = start = get_cycles();
#endif

        if (copy_from_user(&lock, (drm_lock_t *)arg, sizeof(lock)))
		return -EFAULT;

        if (lock.context == DRM_KERNEL_CONTEXT) {
                DRM_ERROR("Process %d using kernel context %d\n",
                          current->pid, lock.context);
                return -EINVAL;
        }

        DRM_DEBUG("%d (pid %d) requests lock (0x%08x), flags = 0x%08x\n",
                  lock.context, current->pid, dev->lock.hw_lock->lock,
                  lock.flags);

#if 0
				/* dev->queue_count == 0 right now for
                                   tdfx.  FIXME? */
        if (lock.context < 0 || lock.context >= dev->queue_count)
                return -EINVAL;
#endif

        if (!ret) {
#if 0
                if (_DRM_LOCKING_CONTEXT(dev->lock.hw_lock->lock)
                    != lock.context) {
                        long j = jiffies - dev->lock.lock_time;

                        if (lock.context == tdfx_res_ctx.handle &&
				j >= 0 && j < DRM_LOCK_SLICE) {
                                /* Can't take lock if we just had it and
                                   there is contention. */
                                DRM_DEBUG("%d (pid %d) delayed j=%d dev=%d jiffies=%d\n",
					lock.context, current->pid, j,
					dev->lock.lock_time, jiffies);
                                current->state = TASK_INTERRUPTIBLE;
				current->policy |= SCHED_YIELD;
                                schedule_timeout(DRM_LOCK_SLICE-j);
				DRM_DEBUG("jiffies=%d\n", jiffies);
                        }
                }
#endif
                add_wait_queue(&dev->lock.lock_queue, &entry);
                for (;;) {
                        current->state = TASK_INTERRUPTIBLE;
                        if (!dev->lock.hw_lock) {
                                /* Device has been unregistered */
                                ret = -EINTR;
                                break;
                        }
                        if (drm_lock_take(&dev->lock.hw_lock->lock,
                                          lock.context)) {
                                dev->lock.pid       = current->pid;
                                dev->lock.lock_time = jiffies;
                                atomic_inc(&dev->total_locks);
                                break;  /* Got lock */
                        }

                                /* Contention */
                        atomic_inc(&dev->total_sleeps);
			yield();
                        if (signal_pending(current)) {
                                ret = -ERESTARTSYS;
                                break;
                        }
                }
                current->state = TASK_RUNNING;
                remove_wait_queue(&dev->lock.lock_queue, &entry);
        }

#if 0
	if (!ret && dev->last_context != lock.context &&
		lock.context != tdfx_res_ctx.handle &&
		dev->last_context != tdfx_res_ctx.handle) {
		add_wait_queue(&dev->context_wait, &entry);
	        current->state = TASK_INTERRUPTIBLE;
                /* PRE: dev->last_context != lock.context */
	        tdfx_context_switch(dev, dev->last_context, lock.context);
		/* POST: we will wait for the context
                   switch and will dispatch on a later call
                   when dev->last_context == lock.context
                   NOTE WE HOLD THE LOCK THROUGHOUT THIS
                   TIME! */
		yield();
	        current->state = TASK_RUNNING;
	        remove_wait_queue(&dev->context_wait, &entry);
	        if (signal_pending(current)) {
	                ret = -EINTR;
	        } else if (dev->last_context != lock.context) {
			DRM_ERROR("Context mismatch: %d %d\n",
                        	dev->last_context, lock.context);
	        }
	}
#endif

        if (!ret) {
		sigemptyset(&dev->sigmask);
		sigaddset(&dev->sigmask, SIGSTOP);
		sigaddset(&dev->sigmask, SIGTSTP);
		sigaddset(&dev->sigmask, SIGTTIN);
		sigaddset(&dev->sigmask, SIGTTOU);
		dev->sigdata.context = lock.context;
		dev->sigdata.lock    = dev->lock.hw_lock;
		block_all_signals(drm_notifier, &dev->sigdata, &dev->sigmask);

                if (lock.flags & _DRM_LOCK_READY) {
				/* Wait for space in DMA/FIFO */
		}
                if (lock.flags & _DRM_LOCK_QUIESCENT) {
				/* Make hardware quiescent */
#if 0
                        tdfx_quiescent(dev);
#endif
		}
        }

#if LINUX_VERSION_CODE < 0x020400
	if (lock.context != tdfx_res_ctx.handle) {
		current->counter = 5;
		current->priority = DEF_PRIORITY/4;
	}
#endif
        DRM_DEBUG("%d %s\n", lock.context, ret ? "interrupted" : "has lock");

#if DRM_DMA_HISTOGRAM
        atomic_inc(&dev->histo.lacq[drm_histogram_slot(get_cycles() - start)]);
#endif

        return ret;
}


int tdfx_unlock(struct inode *inode, struct file *filp, unsigned int cmd,
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
				/* FIXME: Try to send data to card here */
	if (!dev->context_flag) {
		if (drm_lock_free(dev, &dev->lock.hw_lock->lock,
				  DRM_KERNEL_CONTEXT)) {
			DRM_ERROR("\n");
		}
	}

#if LINUX_VERSION_CODE < 0x020400
	if (lock.context != tdfx_res_ctx.handle) {
		current->counter = 5;
		current->priority = DEF_PRIORITY;
	}
#endif

	unblock_all_signals();
	return 0;
}
