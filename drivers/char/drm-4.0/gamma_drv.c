/* gamma.c -- 3dlabs GMX 2000 driver -*- linux-c -*-
 * Created: Mon Jan  4 08:58:31 1999 by faith@precisioninsight.com
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
 *
 */

#include <linux/config.h>
#include "drmP.h"
#include "gamma_drv.h"

#ifndef PCI_DEVICE_ID_3DLABS_GAMMA
#define PCI_DEVICE_ID_3DLABS_GAMMA 0x0008
#endif
#ifndef PCI_DEVICE_ID_3DLABS_MX
#define PCI_DEVICE_ID_3DLABS_MX 0x0006
#endif

#define GAMMA_NAME	 "gamma"
#define GAMMA_DESC	 "3dlabs GMX 2000"
#define GAMMA_DATE	 "20000910"
#define GAMMA_MAJOR	 1
#define GAMMA_MINOR	 0
#define GAMMA_PATCHLEVEL 0

static drm_device_t	      gamma_device;

static struct file_operations gamma_fops = {
#if LINUX_VERSION_CODE >= 0x020400
				/* This started being used during 2.4.0-test */
	owner:   THIS_MODULE,
#endif
	open:	 gamma_open,
	flush:	 drm_flush,
	release: gamma_release,
	ioctl:	 gamma_ioctl,
	mmap:	 drm_mmap,
	read:	 drm_read,
	fasync:	 drm_fasync,
	poll:	 drm_poll,
};

static struct miscdevice      gamma_misc = {
	minor: MISC_DYNAMIC_MINOR,
	name:  GAMMA_NAME,
	fops:  &gamma_fops,
};

static drm_ioctl_desc_t	      gamma_ioctls[] = {
	[DRM_IOCTL_NR(DRM_IOCTL_VERSION)]    = { gamma_version,	  0, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_GET_UNIQUE)] = { drm_getunique,	  0, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_GET_MAGIC)]  = { drm_getmagic,	  0, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_IRQ_BUSID)]  = { drm_irq_busid,	  0, 1 },

	[DRM_IOCTL_NR(DRM_IOCTL_SET_UNIQUE)] = { drm_setunique,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_BLOCK)]	     = { drm_block,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_UNBLOCK)]    = { drm_unblock,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_CONTROL)]    = { gamma_control,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_AUTH_MAGIC)] = { drm_authmagic,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_ADD_MAP)]    = { drm_addmap,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_ADD_BUFS)]   = { drm_addbufs,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_MARK_BUFS)]  = { drm_markbufs,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_INFO_BUFS)]  = { drm_infobufs,	  1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_MAP_BUFS)]   = { drm_mapbufs,	  1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_FREE_BUFS)]  = { drm_freebufs,	  1, 0 },

	[DRM_IOCTL_NR(DRM_IOCTL_ADD_CTX)]    = { drm_addctx,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_RM_CTX)]     = { drm_rmctx,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_MOD_CTX)]    = { drm_modctx,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_GET_CTX)]    = { drm_getctx,	  1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_SWITCH_CTX)] = { drm_switchctx,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_NEW_CTX)]    = { drm_newctx,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_RES_CTX)]    = { drm_resctx,	  1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_ADD_DRAW)]   = { drm_adddraw,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_RM_DRAW)]    = { drm_rmdraw,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_DMA)]	     = { gamma_dma,	  1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_LOCK)]	     = { gamma_lock,	  1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_UNLOCK)]     = { gamma_unlock,	  1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_FINISH)]     = { drm_finish,	  1, 0 },
};
#define GAMMA_IOCTL_COUNT DRM_ARRAY_SIZE(gamma_ioctls)

#ifdef MODULE
static char		      *gamma = NULL;
#endif
static int 		      devices = 0;

MODULE_AUTHOR("VA Linux Systems, Inc.");
MODULE_DESCRIPTION("3dlabs GMX 2000");
MODULE_LICENSE("GPL and additional rights");
MODULE_PARM(gamma, "s");
MODULE_PARM(devices, "i");
MODULE_PARM_DESC(devices,
		 "devices=x, where x is the number of MX chips on card\n");
#ifndef MODULE
/* gamma_options is called by the kernel to parse command-line options
 * passed via the boot-loader (e.g., LILO).  It calls the insmod option
 * routine, drm_parse_options.
 */


static int __init gamma_options(char *str)
{
	drm_parse_options(str);
	return 1;
}

__setup("gamma=", gamma_options);
#endif

static int gamma_setup(drm_device_t *dev)
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


static int gamma_takedown(drm_device_t *dev)
{
	int		  i;
	drm_magic_entry_t *pt, *next;
	drm_map_t	  *map;
	drm_vma_entry_t	  *vma, *vma_next;

	DRM_DEBUG("\n");

	if (dev->irq) gamma_irq_uninstall(dev);

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

int gamma_found(void)
{
	return devices;
}

int gamma_find_devices(void)
{
	struct pci_dev *d = NULL, *one = NULL, *two = NULL;

	d = pci_find_device(PCI_VENDOR_ID_3DLABS,PCI_DEVICE_ID_3DLABS_GAMMA,d);
	if (!d) return 0;

	one = pci_find_device(PCI_VENDOR_ID_3DLABS,PCI_DEVICE_ID_3DLABS_MX,d);
	if (!one) return 0;

	/* Make sure it's on the same card, if not - no MX's found */
	if (PCI_SLOT(d->devfn) != PCI_SLOT(one->devfn)) return 0;

	two = pci_find_device(PCI_VENDOR_ID_3DLABS,PCI_DEVICE_ID_3DLABS_MX,one);
	if (!two) return 1;

	/* Make sure it's on the same card, if not - only 1 MX found */
	if (PCI_SLOT(d->devfn) != PCI_SLOT(two->devfn)) return 1;

	/* Two MX's found - we don't currently support more than 2 */
	return 2;
}

/* gamma_init is called via init_module at module load time, or via
 * linux/init/main.c (this is not currently supported). */

static int __init gamma_init(void)
{
	int		      retcode;
	drm_device_t	      *dev = &gamma_device;

	DRM_DEBUG("\n");

	memset((void *)dev, 0, sizeof(*dev));
	dev->count_lock	  = SPIN_LOCK_UNLOCKED;
	sema_init(&dev->struct_sem, 1);

#ifdef MODULE
	drm_parse_options(gamma);
#endif
	devices = gamma_find_devices();
	if (devices == 0) return -1;

	if ((retcode = misc_register(&gamma_misc))) {
		DRM_ERROR("Cannot register \"%s\"\n", GAMMA_NAME);
		return retcode;
	}
	dev->device = MKDEV(MISC_MAJOR, gamma_misc.minor);
	dev->name   = GAMMA_NAME;

	drm_mem_init();
	drm_proc_init(dev);

	DRM_INFO("Initialized %s %d.%d.%d %s on minor %d with %d MX devices\n",
		 GAMMA_NAME,
		 GAMMA_MAJOR,
		 GAMMA_MINOR,
		 GAMMA_PATCHLEVEL,
		 GAMMA_DATE,
		 gamma_misc.minor,
		 devices);

	return 0;
}

/* gamma_cleanup is called via cleanup_module at module unload time. */

static void __exit gamma_cleanup(void)
{
	drm_device_t	      *dev = &gamma_device;

	DRM_DEBUG("\n");

	drm_proc_cleanup();
	if (misc_deregister(&gamma_misc)) {
		DRM_ERROR("Cannot unload module\n");
	} else {
		DRM_INFO("Module unloaded\n");
	}
	gamma_takedown(dev);
}

module_init(gamma_init);
module_exit(gamma_cleanup);


int gamma_version(struct inode *inode, struct file *filp, unsigned int cmd,
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

	version.version_major	   = GAMMA_MAJOR;
	version.version_minor	   = GAMMA_MINOR;
	version.version_patchlevel = GAMMA_PATCHLEVEL;

	DRM_COPY(version.name, GAMMA_NAME);
	DRM_COPY(version.date, GAMMA_DATE);
	DRM_COPY(version.desc, GAMMA_DESC);

	if (copy_to_user((drm_version_t *)arg,
			 &version,
			 sizeof(version)))
		return -EFAULT;
	return 0;
}

int gamma_open(struct inode *inode, struct file *filp)
{
	drm_device_t  *dev    = &gamma_device;
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
			return gamma_setup(dev);
		}
		spin_unlock(&dev->count_lock);
	}
	return retcode;
}

int gamma_release(struct inode *inode, struct file *filp)
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
			return gamma_takedown(dev);
		}
		spin_unlock(&dev->count_lock);
	}
	unlock_kernel();
	return retcode;
}

/* drm_ioctl is called whenever a process performs an ioctl on /dev/drm. */

int gamma_ioctl(struct inode *inode, struct file *filp, unsigned int cmd,
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

	if (nr >= GAMMA_IOCTL_COUNT) {
		retcode = -EINVAL;
	} else {
		ioctl	  = &gamma_ioctls[nr];
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


int gamma_unlock(struct inode *inode, struct file *filp, unsigned int cmd,
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
	gamma_dma_schedule(dev, 1);
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
