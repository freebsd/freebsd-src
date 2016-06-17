/* $Id: ffb_drv.c,v 1.16 2001/10/18 16:00:24 davem Exp $
 * ffb_drv.c: Creator/Creator3D direct rendering driver.
 *
 * Copyright (C) 2000 David S. Miller (davem@redhat.com)
 */

#include <linux/config.h>
#include "ffb.h"
#include "drmP.h"

#include "ffb_drv.h"

#include <linux/sched.h>
#include <linux/smp_lock.h>
#include <asm/shmparam.h>
#include <asm/oplib.h>
#include <asm/upa.h>

#define DRIVER_AUTHOR		"David S. Miller"

#define DRIVER_NAME		"ffb"
#define DRIVER_DESC		"Creator/Creator3D"
#define DRIVER_DATE		"20000517"

#define DRIVER_MAJOR		0
#define DRIVER_MINOR		0
#define DRIVER_PATCHLEVEL	1

#define DRIVER_FOPS						\
static struct file_operations	DRM(fops) = {			\
	owner:   		THIS_MODULE,			\
	open:	 		DRM(open),			\
	flush:	 		DRM(flush),			\
	release: 		DRM(release),			\
	ioctl:	 		DRM(ioctl),			\
	mmap:	 		DRM(mmap),			\
	read:	 		DRM(read),			\
	fasync:	 		DRM(fasync),			\
	poll:	 		DRM(poll),			\
	get_unmapped_area:	ffb_get_unmapped_area,		\
}

#define DRIVER_COUNT_CARDS()	ffb_count_card_instances()
/* Allocate private structure and fill it */
#define DRIVER_PRESETUP()	do {		\
	int _ret;				\
	_ret = ffb_presetup(dev);		\
	if (_ret != 0) return _ret;		\
} while(0)

/* Free private structure */
#define DRIVER_PRETAKEDOWN()	do {				\
	if (dev->dev_private) kfree(dev->dev_private);		\
} while(0)

#define DRIVER_POSTCLEANUP()	do {				\
	if (ffb_position != NULL) kfree(ffb_position);		\
} while(0)

/* We have to free up the rogue hw context state holding error or 
 * else we will leak it.
 */
#define DRIVER_RELEASE()	do {					\
	ffb_dev_priv_t *fpriv = (ffb_dev_priv_t *) dev->dev_private;	\
	int context = _DRM_LOCKING_CONTEXT(dev->lock.hw_lock->lock);	\
	int idx;							\
									\
	idx = context - 1;						\
	if (fpriv &&							\
	    context != DRM_KERNEL_CONTEXT &&				\
	    fpriv->hw_state[idx] != NULL) {				\
		kfree(fpriv->hw_state[idx]);				\
		fpriv->hw_state[idx] = NULL;				\
	}								\
} while(0)

/* For mmap customization */
#define DRIVER_GET_MAP_OFS()	(map->offset & 0xffffffff)
#define DRIVER_GET_REG_OFS()	ffb_get_reg_offset(dev)

typedef struct _ffb_position_t {
	int node;
	int root;
} ffb_position_t;

static ffb_position_t *ffb_position;

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

static void ffb_apply_upa_parent_ranges(int parent, 
					struct linux_prom64_registers *regs)
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

static int ffb_init_one(drm_device_t *dev, int prom_node, int parent_node,
			int instance)
{
	struct linux_prom64_registers regs[2*PROMREG_MAX];
	ffb_dev_priv_t *ffb_priv = (ffb_dev_priv_t *)dev->dev_private;
	int i;

	ffb_priv->prom_node = prom_node;
	if (prom_getproperty(ffb_priv->prom_node, "reg",
			     (void *)regs, sizeof(regs)) <= 0) {
		return -EINVAL;
	}
	ffb_apply_upa_parent_ranges(parent_node, &regs[0]);
	ffb_priv->card_phys_base = regs[0].phys_addr;
	ffb_priv->regs = (ffb_fbcPtr)
		(regs[0].phys_addr + 0x00600000UL);
	get_ffb_type(ffb_priv, instance);
	for (i = 0; i < FFB_MAX_CTXS; i++)
		ffb_priv->hw_state[i] = NULL;
	
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

static int __init ffb_scan_siblings(int root, int instance)
{
	int node, child;

	child = prom_getchild(root);
	for (node = prom_searchsiblings(child, "SUNW,ffb"); node;
	     node = prom_searchsiblings(prom_getsibling(node), "SUNW,ffb")) {
		ffb_position[instance].node = node;
		ffb_position[instance].root = root;
		instance++;
	}

	return instance;
}

static int ffb_presetup(drm_device_t *);

static int __init ffb_count_card_instances(void)
{
	int root, total, instance;

	total = ffb_count_siblings(prom_root_node);
	root = prom_getchild(prom_root_node);
	for (root = prom_searchsiblings(root, "upa"); root;
	     root = prom_searchsiblings(prom_getsibling(root), "upa"))
		total += ffb_count_siblings(root);

	ffb_position = kmalloc(sizeof(ffb_position_t) * total, GFP_KERNEL);

	/* Actual failure will be caught during ffb_presetup b/c we can't catch
	 * it easily here.
	 */
	if (!ffb_position)
		return -ENOMEM;

	instance = ffb_scan_siblings(prom_root_node, 0);

	root = prom_getchild(prom_root_node);
	for (root = prom_searchsiblings(root, "upa"); root;
	     root = prom_searchsiblings(prom_getsibling(root), "upa"))
		instance = ffb_scan_siblings(root, instance);

	return total;
}

static drm_map_t *ffb_find_map(struct file *filp, unsigned long off)
{
	drm_file_t	*priv	= filp->private_data;
	drm_device_t	*dev;
	drm_map_list_t  *r_list;
	struct list_head *list;
	drm_map_t	*map;

	if (!priv || (dev = priv->dev) == NULL)
		return NULL;

	list_for_each(list, &dev->maplist->head) {
		unsigned long uoff;

		r_list = (drm_map_list_t *)list;
		map = r_list->map;
		if (!map)
			continue;
		uoff = (map->offset & 0xffffffff);
		if (uoff == off)
			return map;
	}

	return NULL;
}

static unsigned long ffb_get_unmapped_area(struct file *filp,
					   unsigned long hint,
					   unsigned long len,
					   unsigned long pgoff,
					   unsigned long flags)
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

static unsigned long ffb_get_reg_offset(drm_device_t *dev)
{
	ffb_dev_priv_t *ffb_priv = (ffb_dev_priv_t *)dev->dev_private;

	if (ffb_priv)
		return ffb_priv->card_phys_base;

	return 0;
}

#include "drm_auth.h"
#include "drm_bufs.h"
#include "drm_dma.h"
#include "drm_drawable.h"
#include "drm_drv.h"

/* This functions must be here since it references DRM(numdevs)
 * which drm_drv.h declares.
 */
static int ffb_presetup(drm_device_t *dev)
{
	ffb_dev_priv_t	*ffb_priv;
	drm_device_t *temp_dev;
	int ret = 0;
	int i;

	/* Check for the case where no device was found. */
	if (ffb_position == NULL)
		return -ENODEV;

	/* Find our instance number by finding our device in dev structure */
	for (i = 0; i < DRM(numdevs); i++) {
		temp_dev = &(DRM(device)[i]);
		if(temp_dev == dev)
			break;
	}

	if (i == DRM(numdevs))
		return -ENODEV;

	ffb_priv = kmalloc(sizeof(ffb_dev_priv_t), GFP_KERNEL);
	if (!ffb_priv)
		return -ENOMEM;
	memset(ffb_priv, 0, sizeof(*ffb_priv));
	dev->dev_private = ffb_priv;

	ret = ffb_init_one(dev,
			   ffb_position[i].node,
			   ffb_position[i].root,
			   i);
	return ret;
}

#include "drm_fops.h"
#include "drm_init.h"
#include "drm_ioctl.h"
#include "drm_lock.h"
#include "drm_memory.h"
#include "drm_proc.h"
#include "drm_vm.h"
#include "drm_stub.h"
