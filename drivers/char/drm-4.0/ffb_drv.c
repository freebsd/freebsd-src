/* $Id: ffb_drv.c,v 1.14 2001/05/24 12:01:47 davem Exp $
 * ffb_drv.c: Creator/Creator3D direct rendering driver.
 *
 * Copyright (C) 2000 David S. Miller (davem@redhat.com)
 */

#include "drmP.h"

#include <linux/sched.h>
#include <linux/smp_lock.h>
#include <asm/shmparam.h>
#include <asm/oplib.h>
#include <asm/upa.h>

#include "ffb_drv.h"

#define FFB_NAME	"ffb"
#define FFB_DESC	"Creator/Creator3D"
#define FFB_DATE	"20000517"
#define FFB_MAJOR	0
#define FFB_MINOR	0
#define FFB_PATCHLEVEL	1

/* Forward declarations. */
int  ffb_init(void);
void ffb_cleanup(void);
static int  ffb_version(struct inode *inode, struct file *filp,
			unsigned int cmd, unsigned long arg);
static int  ffb_open(struct inode *inode, struct file *filp);
static int  ffb_release(struct inode *inode, struct file *filp);
static int  ffb_ioctl(struct inode *inode, struct file *filp,
		      unsigned int cmd, unsigned long arg);
static int  ffb_lock(struct inode *inode, struct file *filp,
		     unsigned int cmd, unsigned long arg);
static int  ffb_unlock(struct inode *inode, struct file *filp,
		       unsigned int cmd, unsigned long arg);
static int ffb_mmap(struct file *filp, struct vm_area_struct *vma);
static unsigned long ffb_get_unmapped_area(struct file *, unsigned long, unsigned long, unsigned long, unsigned long);

/* From ffb_context.c */
extern int ffb_resctx(struct inode *, struct file *, unsigned int, unsigned long);
extern int ffb_addctx(struct inode *, struct file *, unsigned int, unsigned long);
extern int ffb_modctx(struct inode *, struct file *, unsigned int, unsigned long);
extern int ffb_getctx(struct inode *, struct file *, unsigned int, unsigned long);
extern int ffb_switchctx(struct inode *, struct file *, unsigned int, unsigned long);
extern int ffb_newctx(struct inode *, struct file *, unsigned int, unsigned long);
extern int ffb_rmctx(struct inode *, struct file *, unsigned int, unsigned long);
extern int ffb_context_switch(drm_device_t *, int, int);

static struct file_operations ffb_fops = {
	owner:			THIS_MODULE,
	open:			ffb_open,
	flush:			drm_flush,
	release:		ffb_release,
	ioctl:			ffb_ioctl,
	mmap:			ffb_mmap,
	read:			drm_read,
	fasync:			drm_fasync,
	poll:			drm_poll,
	get_unmapped_area:	ffb_get_unmapped_area,
};

/* This is just a template, we make a new copy for each FFB
 * we discover at init time so that each one gets a unique
 * misc device minor number.
 */
static struct miscdevice ffb_misc = {
	minor:	MISC_DYNAMIC_MINOR,
	name:	FFB_NAME,
	fops:	&ffb_fops,
};

static drm_ioctl_desc_t ffb_ioctls[] = {
	[DRM_IOCTL_NR(DRM_IOCTL_VERSION)]    = { ffb_version,	  0, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_GET_UNIQUE)] = { drm_getunique,	  0, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_GET_MAGIC)]  = { drm_getmagic,	  0, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_IRQ_BUSID)]  = { drm_irq_busid,	  0, 1 }, /* XXX */

	[DRM_IOCTL_NR(DRM_IOCTL_SET_UNIQUE)] = { drm_setunique,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_BLOCK)]	     = { drm_block,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_UNBLOCK)]    = { drm_unblock,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_AUTH_MAGIC)] = { drm_authmagic,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_ADD_MAP)]    = { drm_addmap,	  1, 1 },
	
	/* The implementation is currently a nop just like on tdfx.
	 * Later we can do something more clever. -DaveM
	 */
	[DRM_IOCTL_NR(DRM_IOCTL_ADD_CTX)]    = { ffb_addctx,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_RM_CTX)]     = { ffb_rmctx,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_MOD_CTX)]    = { ffb_modctx,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_GET_CTX)]    = { ffb_getctx,	  1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_SWITCH_CTX)] = { ffb_switchctx,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_NEW_CTX)]    = { ffb_newctx,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_RES_CTX)]    = { ffb_resctx,	  1, 0 },

	[DRM_IOCTL_NR(DRM_IOCTL_ADD_DRAW)]   = { drm_adddraw,	  1, 1 },
	[DRM_IOCTL_NR(DRM_IOCTL_RM_DRAW)]    = { drm_rmdraw,	  1, 1 },

	[DRM_IOCTL_NR(DRM_IOCTL_LOCK)]	     = { ffb_lock,	  1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_UNLOCK)]     = { ffb_unlock,	  1, 0 },
	[DRM_IOCTL_NR(DRM_IOCTL_FINISH)]     = { drm_finish,	  1, 0 },
};
#define FFB_IOCTL_COUNT DRM_ARRAY_SIZE(ffb_ioctls)

#ifdef MODULE
static char *ffb = NULL;
#endif

MODULE_AUTHOR("David S. Miller (davem@redhat.com)");
MODULE_DESCRIPTION("Sun Creator/Creator3D DRI");
MODULE_LICENSE("GPL");

static int ffb_takedown(drm_device_t *dev)
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
	
	/* Clear vma list (only built for debugging) */
	if (dev->vmalist) {
		for (vma = dev->vmalist; vma; vma = vma_next) {
			vma_next = vma->next;
			drm_free(vma, sizeof(*vma), DRM_MEM_VMAS);
		}
		dev->vmalist = NULL;
	}
	
	/* Clear map area information */
	if (dev->maplist) {
		for (i = 0; i < dev->map_count; i++) {
			map = dev->maplist[i];
			switch (map->type) {
			case _DRM_REGISTERS:
			case _DRM_FRAME_BUFFER:
				drm_ioremapfree(map->handle, map->size, dev);
				break;

			case _DRM_SHM:
				drm_free_pages((unsigned long)map->handle,
					       drm_order(map->size)
					       - PAGE_SHIFT,
					       DRM_MEM_SAREA);
				break;

			default:
				break;
			};

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

drm_device_t **ffb_dev_table;
static int ffb_dev_table_size;

static void get_ffb_type(ffb_dev_priv_t *ffb_priv, int instance)
{
	volatile unsigned char *strap_bits;
	unsigned char val;

	strap_bits = (volatile unsigned char *)
		(ffb_priv->card_phys_base + 0x00200000UL);

	/* Don't ask, you have to read the value twice for whatever
	 * reason to get correct contents.
	 */
	val = upa_readb(strap_bits);
	val = upa_readb(strap_bits);
	switch (val & 0x78) {
	case (0x0 << 5) | (0x0 << 3):
		ffb_priv->ffb_type = ffb1_prototype;
		printk("ffb%d: Detected FFB1 pre-FCS prototype\n", instance);
		break;
	case (0x0 << 5) | (0x1 << 3):
		ffb_priv->ffb_type = ffb1_standard;
		printk("ffb%d: Detected FFB1\n", instance);
		break;
	case (0x0 << 5) | (0x3 << 3):
		ffb_priv->ffb_type = ffb1_speedsort;
		printk("ffb%d: Detected FFB1-SpeedSort\n", instance);
		break;
	case (0x1 << 5) | (0x0 << 3):
		ffb_priv->ffb_type = ffb2_prototype;
		printk("ffb%d: Detected FFB2/vertical pre-FCS prototype\n", instance);
		break;
	case (0x1 << 5) | (0x1 << 3):
		ffb_priv->ffb_type = ffb2_vertical;
		printk("ffb%d: Detected FFB2/vertical\n", instance);
		break;
	case (0x1 << 5) | (0x2 << 3):
		ffb_priv->ffb_type = ffb2_vertical_plus;
		printk("ffb%d: Detected FFB2+/vertical\n", instance);
		break;
	case (0x2 << 5) | (0x0 << 3):
		ffb_priv->ffb_type = ffb2_horizontal;
		printk("ffb%d: Detected FFB2/horizontal\n", instance);
		break;
	case (0x2 << 5) | (0x2 << 3):
		ffb_priv->ffb_type = ffb2_horizontal;
		printk("ffb%d: Detected FFB2+/horizontal\n", instance);
		break;
	default:
		ffb_priv->ffb_type = ffb2_vertical;
		printk("ffb%d: Unknown boardID[%08x], assuming FFB2\n", instance, val);
		break;
	};
}

static void __init ffb_apply_upa_parent_ranges(int parent, struct linux_prom64_registers *regs)
{
	struct linux_prom64_ranges ranges[PROMREG_MAX];
	char name[128];
	int len, i;

	prom_getproperty(parent, "name", name, sizeof(name));
	if (strcmp(name, "upa") != 0)
		return;

	len = prom_getproperty(parent, "ranges", (void *) ranges, sizeof(ranges));
	if (len <= 0)
		return;

	len /= sizeof(struct linux_prom64_ranges);
	for (i = 0; i < len; i++) {
		struct linux_prom64_ranges *rng = &ranges[i];
		u64 phys_addr = regs->phys_addr;

		if (phys_addr >= rng->ot_child_base &&
		    phys_addr < (rng->ot_child_base + rng->or_size)) {
			regs->phys_addr -= rng->ot_child_base;
			regs->phys_addr += rng->ot_parent_base;
			return;
		}
	}

	return;
}

static int __init ffb_init_one(int prom_node, int parent_node, int instance)
{
	struct linux_prom64_registers regs[2*PROMREG_MAX];
	drm_device_t *dev;
	ffb_dev_priv_t *ffb_priv;
	int ret, i;

	dev = kmalloc(sizeof(drm_device_t) + sizeof(ffb_dev_priv_t), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	memset(dev, 0, sizeof(*dev));
	spin_lock_init(&dev->count_lock);
	sema_init(&dev->struct_sem, 1);

	ffb_priv = (ffb_dev_priv_t *) (dev + 1);
	ffb_priv->prom_node = prom_node;
	if (prom_getproperty(ffb_priv->prom_node, "reg",
			     (void *)regs, sizeof(regs)) <= 0) {
		kfree(dev);
		return -EINVAL;
	}
	ffb_apply_upa_parent_ranges(parent_node, &regs[0]);
	ffb_priv->card_phys_base = regs[0].phys_addr;
	ffb_priv->regs = (ffb_fbcPtr)
		(regs[0].phys_addr + 0x00600000UL);
	get_ffb_type(ffb_priv, instance);
	for (i = 0; i < FFB_MAX_CTXS; i++)
		ffb_priv->hw_state[i] = NULL;

	ffb_dev_table[instance] = dev;

#ifdef MODULE
	drm_parse_options(ffb);
#endif

	memcpy(&ffb_priv->miscdev, &ffb_misc, sizeof(ffb_misc));
	ret = misc_register(&ffb_priv->miscdev);
	if (ret) {
		ffb_dev_table[instance] = NULL;
		kfree(dev);
		return ret;
	}

	dev->device = MKDEV(MISC_MAJOR, ffb_priv->miscdev.minor);
	dev->name = FFB_NAME;

	drm_mem_init();
	drm_proc_init(dev);

	DRM_INFO("Initialized %s %d.%d.%d %s on minor %d at %016lx\n",
		 FFB_NAME,
		 FFB_MAJOR,
		 FFB_MINOR,
		 FFB_PATCHLEVEL,
		 FFB_DATE,
		 ffb_priv->miscdev.minor,
		 ffb_priv->card_phys_base);
	
	return 0;
}

static int __init ffb_count_siblings(int root)
{
	int node, child, count = 0;

	child = prom_getchild(root);
	for (node = prom_searchsiblings(child, "SUNW,ffb"); node;
	     node = prom_searchsiblings(prom_getsibling(node), "SUNW,ffb"))
		count++;

	return count;
}

static int __init ffb_init_dev_table(void)
{
	int root, total;

	total = ffb_count_siblings(prom_root_node);
	root = prom_getchild(prom_root_node);
	for (root = prom_searchsiblings(root, "upa"); root;
	     root = prom_searchsiblings(prom_getsibling(root), "upa"))
		total += ffb_count_siblings(root);

	if (!total)
		return -ENODEV;

	ffb_dev_table = kmalloc(sizeof(drm_device_t *) * total, GFP_KERNEL);
	if (!ffb_dev_table)
		return -ENOMEM;

	ffb_dev_table_size = total;

	return 0;
}

static int __init ffb_scan_siblings(int root, int instance)
{
	int node, child;

	child = prom_getchild(root);
	for (node = prom_searchsiblings(child, "SUNW,ffb"); node;
	     node = prom_searchsiblings(prom_getsibling(node), "SUNW,ffb")) {
		ffb_init_one(node, root, instance);
		instance++;
	}

	return instance;
}

int __init ffb_init(void)
{
	int root, instance, ret;

	ret = ffb_init_dev_table();
	if (ret)
		return ret;

	instance = ffb_scan_siblings(prom_root_node, 0);

	root = prom_getchild(prom_root_node);
	for (root = prom_searchsiblings(root, "upa"); root;
	     root = prom_searchsiblings(prom_getsibling(root), "upa"))
		instance = ffb_scan_siblings(root, instance);

	return 0;
}

void __exit ffb_cleanup(void)
{
	int instance;

	DRM_DEBUG("\n");
	
	drm_proc_cleanup();
	for (instance = 0; instance < ffb_dev_table_size; instance++) {
		drm_device_t *dev = ffb_dev_table[instance];
		ffb_dev_priv_t *ffb_priv;

		if (!dev)
			continue;

		ffb_priv = (ffb_dev_priv_t *) (dev + 1);
		if (misc_deregister(&ffb_priv->miscdev)) {
			DRM_ERROR("Cannot unload module\n");
		} else {
			DRM_INFO("Module unloaded\n");
		}
		ffb_takedown(dev);
		kfree(dev);
		ffb_dev_table[instance] = NULL;
	}
	kfree(ffb_dev_table);
	ffb_dev_table = NULL;
	ffb_dev_table_size = 0;
}

static int ffb_version(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
	drm_version_t version;
	int len, ret;

	ret = copy_from_user(&version, (drm_version_t *)arg, sizeof(version));
	if (ret)
		return -EFAULT;

	version.version_major		= FFB_MAJOR;
	version.version_minor		= FFB_MINOR;
	version.version_patchlevel	= FFB_PATCHLEVEL;

	len = strlen(FFB_NAME);
	if (len > version.name_len)
		len = version.name_len;
	version.name_len = len;
	if (len && version.name) {
		ret = copy_to_user(version.name, FFB_NAME, len);
		if (ret)
			return -EFAULT;
	}

	len = strlen(FFB_DATE);
	if (len > version.date_len)
		len = version.date_len;
	version.date_len = len;
	if (len && version.date) {
		ret = copy_to_user(version.date, FFB_DATE, len);
		if (ret)
			return -EFAULT;
	}

	len = strlen(FFB_DESC);
	if (len > version.desc_len)
		len = version.desc_len;
	version.desc_len = len;
	if (len && version.desc) {
		ret = copy_to_user(version.desc, FFB_DESC, len);
		if (ret)
			return -EFAULT;
	}

	ret = copy_to_user((drm_version_t *) arg, &version, sizeof(version));
	if (ret)
		ret = -EFAULT;

	return ret;
}

static int ffb_setup(drm_device_t *dev)
{
	int i;

	atomic_set(&dev->ioctl_count, 0);
	atomic_set(&dev->vma_count, 0);
	dev->buf_use = 0;
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

	return 0;
}

static int ffb_open(struct inode *inode, struct file *filp)
{
	drm_device_t *dev;
	int minor, i;
	int ret = 0;

	minor = MINOR(inode->i_rdev);
	for (i = 0; i < ffb_dev_table_size; i++) {
		ffb_dev_priv_t *ffb_priv;

		ffb_priv = (ffb_dev_priv_t *) (ffb_dev_table[i] + 1);

		if (ffb_priv->miscdev.minor == minor)
			break;
	}

	if (i >= ffb_dev_table_size)
		return -EINVAL;

	dev = ffb_dev_table[i];
	if (!dev)
		return -EINVAL;

	DRM_DEBUG("open_count = %d\n", dev->open_count);
	ret = drm_open_helper(inode, filp, dev);
	if (!ret) {
		atomic_inc(&dev->total_open);
		spin_lock(&dev->count_lock);
		if (!dev->open_count++) {
			spin_unlock(&dev->count_lock);
			return ffb_setup(dev);
		}
		spin_unlock(&dev->count_lock);
	}

	return ret;
}

static int ffb_release(struct inode *inode, struct file *filp)
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev;
	int ret = 0;

	lock_kernel();
	dev = priv->dev;
	DRM_DEBUG("open_count = %d\n", dev->open_count);
	if (dev->lock.hw_lock != NULL
	    && _DRM_LOCK_IS_HELD(dev->lock.hw_lock->lock)
	    && dev->lock.pid == current->pid) {
		ffb_dev_priv_t *fpriv = (ffb_dev_priv_t *) (dev + 1);
		int context = _DRM_LOCKING_CONTEXT(dev->lock.hw_lock->lock);
		int idx;

		/* We have to free up the rogue hw context state
		 * holding error or else we will leak it.
		 */
		idx = context - 1;
		if (fpriv->hw_state[idx] != NULL) {
			kfree(fpriv->hw_state[idx]);
			fpriv->hw_state[idx] = NULL;
		}
	}

	ret = drm_release(inode, filp);

	if (!ret) {
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
			ret = ffb_takedown(dev);
			unlock_kernel();
			return ret;
		}
		spin_unlock(&dev->count_lock);
	}

	unlock_kernel();
	return ret;
}

static int ffb_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
	int		 nr	 = DRM_IOCTL_NR(cmd);
	drm_file_t	 *priv	 = filp->private_data;
	drm_device_t	 *dev	 = priv->dev;
	drm_ioctl_desc_t *ioctl;
	drm_ioctl_t	 *func;
	int		 ret;

	atomic_inc(&dev->ioctl_count);
	atomic_inc(&dev->total_ioctl);
	++priv->ioctl_count;
	
	DRM_DEBUG("pid = %d, cmd = 0x%02x, nr = 0x%02x, dev 0x%x, auth = %d\n",
		  current->pid, cmd, nr, dev->device, priv->authenticated);

	if (nr >= FFB_IOCTL_COUNT) {
		ret = -EINVAL;
	} else {
		ioctl	  = &ffb_ioctls[nr];
		func	  = ioctl->func;

		if (!func) {
			DRM_DEBUG("no function\n");
			ret = -EINVAL;
		} else if ((ioctl->root_only && !capable(CAP_SYS_ADMIN))
			    || (ioctl->auth_needed && !priv->authenticated)) {
			ret = -EACCES;
		} else {
			ret = (func)(inode, filp, cmd, arg);
		}
	}
	
	atomic_dec(&dev->ioctl_count);

	return ret;
}

static int ffb_lock(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
        drm_file_t        *priv	= filp->private_data;
        drm_device_t      *dev	= priv->dev;
        DECLARE_WAITQUEUE(entry, current);
        int               ret	= 0;
        drm_lock_t        lock;

	ret = copy_from_user(&lock, (drm_lock_t *)arg, sizeof(lock));
	if (ret)
		return -EFAULT;

        if (lock.context == DRM_KERNEL_CONTEXT) {
                DRM_ERROR("Process %d using kernel context %d\n",
                          current->pid, lock.context);
                return -EINVAL;
        }

        DRM_DEBUG("%d (pid %d) requests lock (0x%08x), flags = 0x%08x\n",
                  lock.context, current->pid, dev->lock.hw_lock->lock,
                  lock.flags);

	add_wait_queue(&dev->lock.lock_queue, &entry);
	for (;;) {
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
		current->state = TASK_INTERRUPTIBLE;
		yield();
		if (signal_pending(current)) {
			ret = -ERESTARTSYS;
			break;
		}
	}
	current->state = TASK_RUNNING;
	remove_wait_queue(&dev->lock.lock_queue, &entry);

        if (!ret) {
		sigemptyset(&dev->sigmask);
		sigaddset(&dev->sigmask, SIGSTOP);
		sigaddset(&dev->sigmask, SIGTSTP);
		sigaddset(&dev->sigmask, SIGTTIN);
		sigaddset(&dev->sigmask, SIGTTOU);
		dev->sigdata.context = lock.context;
		dev->sigdata.lock = dev->lock.hw_lock;
		block_all_signals(drm_notifier, &dev->sigdata, &dev->sigmask);

		if (dev->last_context != lock.context)
			ffb_context_switch(dev, dev->last_context, lock.context);
	}

        DRM_DEBUG("%d %s\n", lock.context, ret ? "interrupted" : "has lock");

        return ret;
}

int ffb_unlock(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
	drm_file_t	  *priv	  = filp->private_data;
	drm_device_t	  *dev	  = priv->dev;
	drm_lock_t	  lock;
	unsigned int old, new, prev, ctx;
	int ret;

	ret = copy_from_user(&lock, (drm_lock_t *)arg, sizeof(lock));
	if (ret)
		return -EFAULT;
	
	if ((ctx = lock.context) == DRM_KERNEL_CONTEXT) {
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

	/* We no longer really hold it, but if we are the next
	 * agent to request it then we should just be able to
	 * take it immediately and not eat the ioctl.
	 */
	dev->lock.pid = 0;
	{
		__volatile__ unsigned int *plock = &dev->lock.hw_lock->lock;

		do {
			old  = *plock;
			new  = ctx;
			prev = cmpxchg(plock, old, new);
		} while (prev != old);
	}

	wake_up_interruptible(&dev->lock.lock_queue);
	
	unblock_all_signals();
	return 0;
}

extern struct vm_operations_struct drm_vm_ops;
extern struct vm_operations_struct drm_vm_shm_ops;
extern struct vm_operations_struct drm_vm_shm_lock_ops;

static int ffb_mmap(struct file *filp, struct vm_area_struct *vma)
{
	drm_file_t	*priv	= filp->private_data;
	drm_device_t	*dev	= priv->dev;
	drm_map_t	*map	= NULL;
	ffb_dev_priv_t	*ffb_priv;
	int		i, minor;
	
	DRM_DEBUG("start = 0x%lx, end = 0x%lx, offset = 0x%lx\n",
		  vma->vm_start, vma->vm_end, VM_OFFSET(vma));

	minor = MINOR(filp->f_dentry->d_inode->i_rdev);
	ffb_priv = NULL;
	for (i = 0; i < ffb_dev_table_size; i++) {
		ffb_priv = (ffb_dev_priv_t *) (ffb_dev_table[i] + 1);
		if (ffb_priv->miscdev.minor == minor)
			break;
	}
	if (i >= ffb_dev_table_size)
		return -EINVAL;

	/* We don't support/need dma mappings, so... */
	if (!VM_OFFSET(vma))
		return -EINVAL;

	for (i = 0; i < dev->map_count; i++) {
		unsigned long off;

		map = dev->maplist[i];

		/* Ok, a little hack to make 32-bit apps work. */
		off = (map->offset & 0xffffffff);
		if (off == VM_OFFSET(vma))
			break;
	}

	if (i >= dev->map_count)
		return -EINVAL;

	if (!map ||
	    ((map->flags & _DRM_RESTRICTED) && !capable(CAP_SYS_ADMIN)))
		return -EPERM;

	if (map->size != (vma->vm_end - vma->vm_start))
		return -EINVAL;

	/* Set read-only attribute before mappings are created
	 * so it works for fb/reg maps too.
	 */
	if (map->flags & _DRM_READ_ONLY)
		vma->vm_page_prot = __pgprot(pte_val(pte_wrprotect(
			__pte(pgprot_val(vma->vm_page_prot)))));

	switch (map->type) {
	case _DRM_FRAME_BUFFER:
		/* FALLTHROUGH */

	case _DRM_REGISTERS:
		/* In order to handle 32-bit drm apps/xserver we
		 * play a trick.  The mappings only really specify
		 * the 32-bit offset from the cards 64-bit base
		 * address, and we just add in the base here.
		 */
		vma->vm_flags |= VM_IO;
		if (io_remap_page_range(vma->vm_start,
					ffb_priv->card_phys_base + VM_OFFSET(vma),
					vma->vm_end - vma->vm_start,
					vma->vm_page_prot, 0))
			return -EAGAIN;

		vma->vm_ops = &drm_vm_ops;
		break;
	case _DRM_SHM:
		if (map->flags & _DRM_CONTAINS_LOCK)
			vma->vm_ops = &drm_vm_shm_lock_ops;
		else {
			vma->vm_ops = &drm_vm_shm_ops;
			vma->vm_private_data = (void *) map;
		}

		/* Don't let this area swap.  Change when
		 * DRM_KERNEL advisory is supported.
		 */
		vma->vm_flags |= VM_LOCKED;
		break;
	default:
		return -EINVAL;	/* This should never happen. */
	};

	vma->vm_flags |= VM_LOCKED | VM_SHM; /* Don't swap */

	vma->vm_file = filp; /* Needed for drm_vm_open() */
	drm_vm_open(vma);
	return 0;
}

static drm_map_t *ffb_find_map(struct file *filp, unsigned long off)
{
	drm_file_t	*priv	= filp->private_data;
	drm_device_t	*dev;
	drm_map_t	*map;
	int		i;

	if (!priv || (dev = priv->dev) == NULL)
		return NULL;

	for (i = 0; i < dev->map_count; i++) {
		unsigned long uoff;

		map = dev->maplist[i];

		/* Ok, a little hack to make 32-bit apps work. */
		uoff = (map->offset & 0xffffffff);
		if (uoff == off)
			return map;
	}
	return NULL;
}

static unsigned long ffb_get_unmapped_area(struct file *filp, unsigned long hint, unsigned long len, unsigned long pgoff, unsigned long flags)
{
	drm_map_t *map = ffb_find_map(filp, pgoff << PAGE_SHIFT);
	unsigned long addr = -ENOMEM;

	if (!map)
		return get_unmapped_area(NULL, hint, len, pgoff, flags);

	if (map->type == _DRM_FRAME_BUFFER ||
	    map->type == _DRM_REGISTERS) {
#ifdef HAVE_ARCH_FB_UNMAPPED_AREA
		addr = get_fb_unmapped_area(filp, hint, len, pgoff, flags);
#else
		addr = get_unmapped_area(NULL, hint, len, pgoff, flags);
#endif
	} else if (map->type == _DRM_SHM && SHMLBA > PAGE_SIZE) {
		unsigned long slack = SHMLBA - PAGE_SIZE;

		addr = get_unmapped_area(NULL, hint, len + slack, pgoff, flags);
		if (!(addr & ~PAGE_MASK)) {
			unsigned long kvirt = (unsigned long) map->handle;

			if ((kvirt & (SHMLBA - 1)) != (addr & (SHMLBA - 1))) {
				unsigned long koff, aoff;

				koff = kvirt & (SHMLBA - 1);
				aoff = addr & (SHMLBA - 1);
				if (koff < aoff)
					koff += SHMLBA;

				addr += (koff - aoff);
			}
		}
	} else {
		addr = get_unmapped_area(NULL, hint, len, pgoff, flags);
	}

	return addr;
}

module_init(ffb_init);
module_exit(ffb_cleanup);
