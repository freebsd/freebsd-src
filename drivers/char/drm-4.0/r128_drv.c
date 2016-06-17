/* r128_drv.c -- ATI Rage 128 driver -*- linux-c -*-
 * Created: Mon Dec 13 09:47:27 1999 by faith@precisioninsight.com
 *
 * Copyright 1999, 2000 Precision Insight, Inc., Cedar Park, Texas.
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
 *    Kevin E. Martin <martin@valinux.com>
 *    Gareth Hughes <gareth@valinux.com>
 *
 */

#include <linux/config.h>
#include "drmP.h"
#include "r128_drv.h"

#define R128_NAME		"r128"
#define R128_DESC		"ATI Rage 128"
#define R128_DATE		"20001215"
#define R128_MAJOR		2
#define R128_MINOR		1
#define R128_PATCHLEVEL		2

static drm_device_t	r128_device;
drm_ctx_t		r128_res_ctx;

static struct file_operations r128_fops = {
#if LINUX_VERSION_CODE >= 0x020400
				/* This started being used during 2.4.0-test */
	owner:   THIS_MODULE,
#endif
	open:	 r128_open,
	flush:	 drm_flush,
	release: r128_release,
	ioctl:	 r128_ioctl,
	mmap:	 drm_mmap,
	read:	 drm_read,
	fasync:	 drm_fasync,
	poll:	 drm_poll,
};

static struct miscdevice      r128_misc = {
	minor: MISC_DYNAMIC_MINOR,
	name:  R128_NAME,
	fops:  &r128_fops,
};

static drm_ioctl_desc_t	      r128_ioctls[] = {
	[DRM_IOCTL_NR(DRM_IOCTL_VERSION)]      = { r128_version,      0, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_GET_UNIQUE)]   = { drm_getunique,     0, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_GET_MAGIC)]    = { drm_getmagic,      0, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_IRQ_BUSID)]    = { drm_irq_busid,     0, 1 },

	[DRM_IOCTL_NR(DRM_IOCTL_SET_UNIQUE)]   = { drm_setunique,     1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_BLOCK)]	       = { drm_block,         1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_UNBLOCK)]      = { drm_unblock,       1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_AUTH_MAGIC)]   = { drm_authmagic,     1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_ADD_MAP)]      = { drm_addmap,        1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_ADD_BUFS)]     = { r128_addbufs,      1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_MARK_BUFS)]    = { drm_markbufs,      1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_INFO_BUFS)]    = { drm_infobufs,      1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_MAP_BUFS)]     = { r128_mapbufs,      1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_FREE_BUFS)]    = { drm_freebufs,      1, 0 },

	[DRM_IOCTL_NR(DRM_IOCTL_ADD_CTX)]      = { r128_addctx,	      1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_RM_CTX)]       = { r128_rmctx,	      1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_MOD_CTX)]      = { r128_modctx,	      1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_GET_CTX)]      = { r128_getctx,	      1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_SWITCH_CTX)]   = { r128_switchctx,    1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_NEW_CTX)]      = { r128_newctx,	      1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_RES_CTX)]      = { r128_resctx,	      1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_ADD_DRAW)]     = { drm_adddraw,	      1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_RM_DRAW)]      = { drm_rmdraw,	      1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_DMA)]	       = { r128_cce_buffers,  1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_LOCK)]	       = { r128_lock,	      1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_UNLOCK)]       = { r128_unlock,	      1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_FINISH)]       = { drm_finish,	      1, 0 },

#if defined(CONFIG_AGP) || defined(CONFIG_AGP_MODULE)
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_ACQUIRE)]  = { drm_agp_acquire,   1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_RELEASE)]  = { drm_agp_release,   1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_ENABLE)]   = { drm_agp_enable,    1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_INFO)]     = { drm_agp_info,      1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_ALLOC)]    = { drm_agp_alloc,     1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_FREE)]     = { drm_agp_free,      1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_BIND)]     = { drm_agp_bind,      1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_AGP_UNBIND)]   = { drm_agp_unbind,    1, 1 },
#endif

	[DRM_IOCTL_NR(DRM_IOCTL_R128_INIT)]    = { r128_cce_init,     1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_R128_CCE_START)] = { r128_cce_start,  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_R128_CCE_STOP)]  = { r128_cce_stop,   1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_R128_CCE_RESET)] = { r128_cce_reset,  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_R128_CCE_IDLE)]  = { r128_cce_idle,   1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_R128_RESET)]   = { r128_engine_reset, 1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_R128_SWAP)]    = { r128_cce_swap,     1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_R128_CLEAR)]   = { r128_cce_clear,    1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_R128_VERTEX)]  = { r128_cce_vertex,   1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_R128_INDICES)] = { r128_cce_indices,  1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_R128_BLIT)]    = { r128_cce_blit,     1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_R128_DEPTH)]   = { r128_cce_depth,    1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_R128_STIPPLE)] = { r128_cce_stipple,  1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_R128_PACKET)]  = { r128_cce_packet,   1, 0 },
};
#define R128_IOCTL_COUNT DRM_ARRAY_SIZE(r128_ioctls)

#ifdef MODULE
static char		      *r128 = NULL;
#endif

MODULE_AUTHOR("VA Linux Systems, Inc.");
MODULE_DESCRIPTION("r128");
MODULE_LICENSE("GPL and additional rights");
MODULE_PARM(r128, "s");

#ifndef MODULE
/* r128_options is called by the kernel to parse command-line options
 * passed via the boot-loader (e.g., LILO).  It calls the insmod option
 * routine, drm_parse_drm.
 */

static int __init r128_options(char *str)
{
	drm_parse_options(str);
	return 1;
}

__setup("r128=", r128_options);
#endif

static int r128_setup(drm_device_t *dev)
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

	dev->ctx_start	    = 0;
	dev->lck_start	    = 0;

	dev->buf_rp	  = dev->buf;
	dev->buf_wp	  = dev->buf;
	dev->buf_end	  = dev->buf + DRM_BSZ;
	dev->buf_async	  = NULL;
	init_waitqueue_head(&dev->buf_readers);
	init_waitqueue_head(&dev->buf_writers);

	r128_res_ctx.handle=-1;

	DRM_DEBUG("\n");

	/* The kernel's context could be created here, but is now created
	   in drm_dma_enqueue.	This is more resource-efficient for
	   hardware that does not do DMA, but may mean that
	   drm_select_queue fails between the time the interrupt is
	   initialized and the time the queues are initialized. */

	return 0;
}


static int r128_takedown(drm_device_t *dev)
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

		if (dev->agp->acquired)	_drm_agp_release();

		dev->agp->acquired = 0;
		dev->agp->enabled  = 0;
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

/* r128_init is called via init_module at module load time, or via
 * linux/init/main.c (this is not currently supported). */

static int __init r128_init(void)
{
	int		      retcode;
	drm_device_t	      *dev = &r128_device;

	DRM_DEBUG("\n");

	memset((void *)dev, 0, sizeof(*dev));
	dev->count_lock	  = SPIN_LOCK_UNLOCKED;
	sema_init(&dev->struct_sem, 1);

#ifdef MODULE
	drm_parse_options(r128);
#endif

	if ((retcode = misc_register(&r128_misc))) {
		DRM_ERROR("Cannot register \"%s\"\n", R128_NAME);
		return retcode;
	}
	dev->device = MKDEV(MISC_MAJOR, r128_misc.minor);
	dev->name   = R128_NAME;

	drm_mem_init();
	drm_proc_init(dev);

#if defined(CONFIG_AGP) || defined(CONFIG_AGP_MODULE)
	dev->agp    = drm_agp_init();
	if (dev->agp == NULL) {
		DRM_ERROR("Cannot initialize agpgart module.\n");
		drm_proc_cleanup();
		misc_deregister(&r128_misc);
		r128_takedown(dev);
		return -ENOMEM;
	}

#ifdef CONFIG_MTRR
	dev->agp->agp_mtrr = mtrr_add(dev->agp->agp_info.aper_base,
				      dev->agp->agp_info.aper_size*1024*1024,
				      MTRR_TYPE_WRCOMB,
				      1);
#endif
#endif

	if((retcode = drm_ctxbitmap_init(dev))) {
		DRM_ERROR("Cannot allocate memory for context bitmap.\n");
		drm_proc_cleanup();
		misc_deregister(&r128_misc);
		r128_takedown(dev);
		return retcode;
	}

	DRM_INFO("Initialized %s %d.%d.%d %s on minor %d\n",
		 R128_NAME,
		 R128_MAJOR,
		 R128_MINOR,
		 R128_PATCHLEVEL,
		 R128_DATE,
		 r128_misc.minor);

	return 0;
}

/* r128_cleanup is called via cleanup_module at module unload time. */

static void __exit r128_cleanup(void)
{
	drm_device_t	      *dev = &r128_device;

	DRM_DEBUG("\n");

	drm_proc_cleanup();
	if (misc_deregister(&r128_misc)) {
		DRM_ERROR("Cannot unload module\n");
	} else {
		DRM_INFO("Module unloaded\n");
	}
	drm_ctxbitmap_cleanup(dev);
	r128_takedown(dev);
#if defined(CONFIG_AGP) || defined(CONFIG_AGP_MODULE)
	if (dev->agp) {
		drm_agp_uninit();
		drm_free(dev->agp, sizeof(*dev->agp), DRM_MEM_AGPLISTS);
		dev->agp = NULL;
	}
#endif
}

module_init(r128_init);
module_exit(r128_cleanup);


int r128_version(struct inode *inode, struct file *filp,
		 unsigned int cmd, unsigned long arg)
{
	drm_version_t version;
	int	      len;

	if (copy_from_user(&version,
			   (drm_version_t *)arg,
			   sizeof(version)))
		return -EFAULT;

#define DRM_COPY(name,value)					\
	len = strlen(value);					\
	if (len > name##_len) len = name##_len;			\
	name##_len = strlen(value);				\
	if (len && name) {					\
		if (copy_to_user(name, value, len))		\
			return -EFAULT;				\
	}

	version.version_major	   = R128_MAJOR;
	version.version_minor	   = R128_MINOR;
	version.version_patchlevel = R128_PATCHLEVEL;

	DRM_COPY(version.name, R128_NAME);
	DRM_COPY(version.date, R128_DATE);
	DRM_COPY(version.desc, R128_DESC);

	if (copy_to_user((drm_version_t *)arg,
			 &version,
			 sizeof(version)))
		return -EFAULT;
	return 0;
}

int r128_open(struct inode *inode, struct file *filp)
{
	drm_device_t  *dev    = &r128_device;
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
			return r128_setup(dev);
		}
		spin_unlock(&dev->count_lock);
	}

	return retcode;
}

int r128_release(struct inode *inode, struct file *filp)
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
			return r128_takedown(dev);
		}
		spin_unlock(&dev->count_lock);
	}

	unlock_kernel();
	return retcode;
}

/* r128_ioctl is called whenever a process performs an ioctl on /dev/drm. */
int r128_ioctl(struct inode *inode, struct file *filp,
	       unsigned int cmd, unsigned long arg)
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

	if (nr >= R128_IOCTL_COUNT) {
		retcode = -EINVAL;
	} else {
		ioctl	  = &r128_ioctls[nr];
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

#if 0
	if ( retcode ) {
		DRM_INFO( "%s 0x%x ret = %d\n", __FUNCTION__, nr, retcode );
	}
#endif

	atomic_dec(&dev->ioctl_count);
	return retcode;
}

int r128_lock(struct inode *inode, struct file *filp,
	      unsigned int cmd, unsigned long arg)
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

        if (lock.context < 0)
                return -EINVAL;

        if (!ret) {
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
                        schedule();
                        if (signal_pending(current)) {
                                ret = -ERESTARTSYS;
                                break;
                        }
                }
                current->state = TASK_RUNNING;
                remove_wait_queue(&dev->lock.lock_queue, &entry);
        }

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
			DRM_DEBUG( "not quiescent!\n" );
#if 0
                        r128_quiescent(dev);
#endif
		}
        }

#if LINUX_VERSION_CODE < 0x020400
	if (lock.context != r128_res_ctx.handle) {
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


int r128_unlock(struct inode *inode, struct file *filp,
		unsigned int cmd, unsigned long arg)
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
	if (lock.context != r128_res_ctx.handle) {
		current->counter = 5;
		current->priority = DEF_PRIORITY;
	}
#endif
	unblock_all_signals();
	return 0;
}
