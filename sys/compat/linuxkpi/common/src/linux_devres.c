/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020-2025 The FreeBSD Foundation
 *
 * This software was developed by Bj\xc3\xb6rn Zeeb under sponsorship from
 * the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/list.h>

/*
 * Linux devres KPI implementation.
 */

struct devres {
	struct list_head	entry;
	void			(*release)(struct device *, void *);

	/* Must come last. */
	uint8_t			__drdata[0] __aligned(CACHE_LINE_SIZE);
};

void *
lkpi_devres_alloc(void(*release)(struct device *, void *),
    size_t size, gfp_t gfp)
{
	void *p;
	struct devres *dr;
	size_t total;

	if (size == 0)
		return (NULL);

	total = sizeof(*dr) + size;
	dr = kmalloc(total, gfp);
	if (dr == NULL)
		return (NULL);

	INIT_LIST_HEAD(&dr->entry);
	dr->release = release;
	p = (void *)(dr+1);

	return (p);
}

static void
lkpi_devres_free_dr(struct devres *dr)
{

	/*
	 * We have no dev, so cannot lock.  This means someone else has
	 * to do this prior to us if devres_add() had been called.
	 */
	KASSERT(list_empty_careful(&dr->entry),
	    ("%s: dr %p still on devres_head\n", __func__, dr));
	kfree(dr);
}

void
lkpi_devres_free(void *p)
{
	struct devres *dr;

	if (p == NULL)
		return;

	dr = container_of(p, struct devres, __drdata);
	lkpi_devres_free_dr(dr);
}

void
lkpi_devres_add(struct device *dev, void *p)
{
	struct devres *dr;

	KASSERT(dev != NULL && p != NULL, ("%s: dev %p p %p\n",
	    __func__, dev, p));

	dr = container_of(p, struct devres, __drdata);
	spin_lock(&dev->devres_lock);
	list_add(&dr->entry, &dev->devres_head);
	spin_unlock(&dev->devres_lock);
}

static struct devres *
lkpi_devres_find_dr(struct device *dev, void(*release)(struct device *, void *),
    int (*match)(struct device *, void *, void *), void *mp)
{
	struct devres *dr, *next;
	void *p;

	KASSERT(dev != NULL, ("%s: dev %p\n", __func__, dev));
	assert_spin_locked(&dev->devres_lock);

	list_for_each_entry_safe(dr, next, &dev->devres_head, entry) {
		if (dr->release != release)
			continue;
		p = (void *)(dr+1);
		if (match != NULL && match(dev, p, mp) == false)
			continue;
		return (dr);
	}

	return (NULL);
}

void *
lkpi_devres_find(struct device *dev, void(*release)(struct device *, void *),
    int (*match)(struct device *, void *, void *), void *mp)
{
	struct devres *dr;

	KASSERT(dev != NULL, ("%s: dev %p\n", __func__, dev));

	spin_lock(&dev->devres_lock);
	dr = lkpi_devres_find_dr(dev, release, match, mp);
	spin_unlock(&dev->devres_lock);

	if (dr == NULL)
		return (NULL);

	return ((void *)(dr + 1));
}

static void
lkpi_devres_unlink_locked(struct device *dev, struct devres *dr)
{
	KASSERT(dev != NULL, ("%s: dev %p\n", __func__, dev));
	KASSERT(dr != NULL, ("%s: dr %p\n", __func__, dr));
	assert_spin_locked(&dev->devres_lock);

	list_del_init(&dr->entry);
}

void
lkpi_devres_unlink(struct device *dev, void *p)
{
	struct devres *dr;

	KASSERT(dev != NULL && p != NULL, ("%s: dev %p p %p\n",
	    __func__, dev, p));

	dr = container_of(p, struct devres, __drdata);
	spin_lock(&dev->devres_lock);
	lkpi_devres_unlink_locked(dev, dr);
	spin_unlock(&dev->devres_lock);
}

/* This is called on device free. */
void
lkpi_devres_release_free_list(struct device *dev)
{
	struct devres *dr, *next;
	void *p;

	/* Free any resources allocated on the device. */
	/* No need to lock anymore. */
	list_for_each_entry_safe(dr, next, &dev->devres_head, entry) {
		p = (void *)(dr+1);
		if (dr->release != NULL)
			dr->release(dev, p);
		/* This should probably be a function of some kind. */
		list_del_init(&dr->entry);
		lkpi_devres_free(p);
	}
}

int
lkpi_devres_destroy(struct device *dev, void(*release)(struct device *, void *),
    int (*match)(struct device *, void *, void *), void *mp)
{
	struct devres *dr;

	spin_lock(&dev->devres_lock);
	dr = lkpi_devres_find_dr(dev, release, match, mp);
	if (dr != NULL)
		lkpi_devres_unlink_locked(dev, dr);
	spin_unlock(&dev->devres_lock);

	if (dr == NULL)
		return (-ENOENT);
	lkpi_devres_free_dr(dr);

	return (0);
}

/*
 * Devres release function for k*malloc().
 * While there is nothing to do here adding, e.g., tracing would be
 * possible so we leave the empty function here.
 * Also good for documentation as it is the simplest example.
 */
void
lkpi_devm_kmalloc_release(struct device *dev __unused, void *p __unused)
{

	/* Nothing to do.  Freed with the devres. */
}

static int
lkpi_devm_kmalloc_match(struct device *dev __unused, void *p, void *mp)
{
	return (p == mp);
}

void
lkpi_devm_kfree(struct device *dev, const void *p)
{
	void *mp;
	int error;

	if (p == NULL)
		return;

	/* I assume Linux simply casts the const away... */
	mp = __DECONST(void *, p);
	error = lkpi_devres_destroy(dev, lkpi_devm_kmalloc_release,
	    lkpi_devm_kmalloc_match, mp);
	if (error != 0)
		dev_warn(dev, "%s: lkpi_devres_destroy failed with %d\n",
		    __func__, error);
}

struct devres_action {
	void *data;
	void (*action)(void *);
};

static void
lkpi_devm_action_release(struct device *dev, void *res)
{
	struct devres_action	*devres;

	devres = (struct devres_action *)res;
	devres->action(devres->data);
}

int
lkpi_devm_add_action(struct device *dev, void (*action)(void *), void *data)
{
	struct devres_action *devres;

	KASSERT(action != NULL, ("%s: action is NULL\n", __func__));
	devres = lkpi_devres_alloc(lkpi_devm_action_release,
		sizeof(struct devres_action), GFP_KERNEL);
	if (devres == NULL)
		return (-ENOMEM);
	devres->data = data;
	devres->action = action;
	devres_add(dev, devres);

	return (0);
}

int
lkpi_devm_add_action_or_reset(struct device *dev, void (*action)(void *), void *data)
{
	int rv;

	rv = lkpi_devm_add_action(dev, action, data);
	if (rv != 0)
		action(data);

	return (rv);
}
