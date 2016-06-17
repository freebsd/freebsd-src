/*
 *  linux/fs/block_dev.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/slab.h>

#define HASH_BITS	6
#define HASH_SIZE	(1UL << HASH_BITS)
#define HASH_MASK	(HASH_SIZE-1)
static struct list_head cdev_hashtable[HASH_SIZE];
static spinlock_t cdev_lock = SPIN_LOCK_UNLOCKED;
static kmem_cache_t * cdev_cachep;

#define alloc_cdev() \
	 ((struct char_device *) kmem_cache_alloc(cdev_cachep, SLAB_KERNEL))
#define destroy_cdev(cdev) kmem_cache_free(cdev_cachep, (cdev))

static void init_once(void * foo, kmem_cache_t * cachep, unsigned long flags)
{
	struct char_device * cdev = (struct char_device *) foo;

	if ((flags & (SLAB_CTOR_VERIFY|SLAB_CTOR_CONSTRUCTOR)) ==
	    SLAB_CTOR_CONSTRUCTOR)
	{
		memset(cdev, 0, sizeof(*cdev));
		sema_init(&cdev->sem, 1);
	}
}

void __init cdev_cache_init(void)
{
	int i;
	struct list_head *head = cdev_hashtable;

	i = HASH_SIZE;
	do {
		INIT_LIST_HEAD(head);
		head++;
		i--;
	} while (i);

	cdev_cachep = kmem_cache_create("cdev_cache",
					 sizeof(struct char_device),
					 0, SLAB_HWCACHE_ALIGN, init_once,
					 NULL);
	if (!cdev_cachep)
		panic("Cannot create cdev_cache SLAB cache");
}

/*
 * Most likely _very_ bad one - but then it's hardly critical for small
 * /dev and can be fixed when somebody will need really large one.
 */
static inline unsigned long hash(dev_t dev)
{
	unsigned long tmp = dev;
	tmp = tmp + (tmp >> HASH_BITS) + (tmp >> HASH_BITS*2);
	return tmp & HASH_MASK;
}

static struct char_device *cdfind(dev_t dev, struct list_head *head)
{
	struct list_head *p;
	struct char_device *cdev;
	for (p=head->next; p!=head; p=p->next) {
		cdev = list_entry(p, struct char_device, hash);
		if (cdev->dev != dev)
			continue;
		atomic_inc(&cdev->count);
		return cdev;
	}
	return NULL;
}

struct char_device *cdget(dev_t dev)
{
	struct list_head * head = cdev_hashtable + hash(dev);
	struct char_device *cdev, *new_cdev;
	spin_lock(&cdev_lock);
	cdev = cdfind(dev, head);
	spin_unlock(&cdev_lock);
	if (cdev)
		return cdev;
	new_cdev = alloc_cdev();
	if (!new_cdev)
		return NULL;
	atomic_set(&new_cdev->count,1);
	new_cdev->dev = dev;
	spin_lock(&cdev_lock);
	cdev = cdfind(dev, head);
	if (!cdev) {
		list_add(&new_cdev->hash, head);
		spin_unlock(&cdev_lock);
		return new_cdev;
	}
	spin_unlock(&cdev_lock);
	destroy_cdev(new_cdev);
	return cdev;
}

void cdput(struct char_device *cdev)
{
	if (atomic_dec_and_lock(&cdev->count, &cdev_lock)) {
		list_del(&cdev->hash);
		spin_unlock(&cdev_lock);
		destroy_cdev(cdev);
	}
}

