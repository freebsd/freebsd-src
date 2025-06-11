/*-
 * Copyright (c) 2013 Hans Petter Selasky. All rights reserved.
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

#include <bsd_global.h>

struct usb_process usb_process[USB_PROC_MAX];

static device_t usb_pci_root;

int (*bus_alloc_resource_any_cb)(struct resource *res, device_t dev,
    int type, int *rid, unsigned int flags);
int (*ofw_bus_status_ok_cb)(device_t dev);
int (*ofw_bus_is_compatible_cb)(device_t dev, char *name);

/*------------------------------------------------------------------------*
 * Implementation of busdma API
 *------------------------------------------------------------------------*/
int
bus_dma_tag_create(bus_dma_tag_t parent, bus_size_t alignment,
		   bus_size_t boundary, bus_addr_t lowaddr,
		   bus_addr_t highaddr, bus_dma_filter_t *filter,
		   void *filterarg, bus_size_t maxsize, int nsegments,
		   bus_size_t maxsegsz, int flags, bus_dma_lock_t *lockfunc,
		   void *lockfuncarg, bus_dma_tag_t *dmat)
{
	struct bus_dma_tag *ret;

	ret = malloc(sizeof(struct bus_dma_tag), XXX, XXX);
	if (*dmat == NULL)
		return (ENOMEM);
	ret->alignment = alignment;
	ret->maxsize = maxsize;

	*dmat = ret;

	return (0);
}

int
bus_dmamem_alloc(bus_dma_tag_t dmat, void** vaddr, int flags,
    bus_dmamap_t *mapp)
{
	void *addr;

	addr = malloc(dmat->maxsize + dmat->alignment, XXX, XXX);
	if (addr == NULL)
		return (ENOMEM);

	*mapp = addr;
	addr = (void*)(((uintptr_t)addr + dmat->alignment - 1) & ~(dmat->alignment - 1));

	*vaddr = addr;
	return (0);
}

int
bus_dmamap_load(bus_dma_tag_t dmat, bus_dmamap_t map, void *buf,
    bus_size_t buflen, bus_dmamap_callback_t *callback,
    void *callback_arg, int flags)
{
	bus_dma_segment_t segs[1];

	segs[0].ds_addr = (uintptr_t)buf;
	segs[0].ds_len = buflen;

	(*callback)(callback_arg, segs, 1, 0);

	return (0);
}

void
bus_dmamap_sync(bus_dma_tag_t dmat, bus_dmamap_t map, int flags)
{
	/* Assuming coherent memory */
	__asm__ __volatile__("": : :"memory");
}

void
bus_dmamem_free(bus_dma_tag_t dmat, void *vaddr, bus_dmamap_t map)
{

	free(map, XXX);
}

int
bus_dma_tag_destroy(bus_dma_tag_t dmat)
{

	free(dmat, XXX);
	return (0);
}

/*------------------------------------------------------------------------*
 * Implementation of resource management API
 *------------------------------------------------------------------------*/

struct resource *
bus_alloc_resource_any(device_t dev, int type, int *rid, unsigned int flags)
{
	struct resource *res;
	int ret = EINVAL;

	res = malloc(sizeof(*res), XXX, XXX);
	if (res == NULL)
		return (NULL);

	res->__r_i = malloc(sizeof(struct resource_i), XXX, XXX);
	if (res->__r_i == NULL) {
		free(res, XXX);
		return (NULL);
	}

	if (bus_alloc_resource_any_cb != NULL)
		ret = (*bus_alloc_resource_any_cb)(res, dev, type, rid, flags);
	if (ret == 0)
		return (res);

	free(res->__r_i, XXX);
	free(res, XXX);
	return (NULL);
}

int
bus_alloc_resources(device_t dev, struct resource_spec *rs,
    struct resource **res)
{
	int i;

	for (i = 0; rs[i].type != -1; i++)
		res[i] = NULL;
	for (i = 0; rs[i].type != -1; i++) {
		res[i] = bus_alloc_resource_any(dev,
		    rs[i].type, &rs[i].rid, rs[i].flags);
		if (res[i] == NULL && !(rs[i].flags & RF_OPTIONAL)) {
			bus_release_resources(dev, rs, res);
			return (ENXIO);
		}
	}
	return (0);
}

void
bus_release_resources(device_t dev, const struct resource_spec *rs,
    struct resource **res)
{
	int i;

	for (i = 0; rs[i].type != -1; i++)
		if (res[i] != NULL) {
			bus_release_resource(
			    dev, rs[i].type, rs[i].rid, res[i]);
			res[i] = NULL;
		}
}

int
bus_setup_intr(device_t dev, struct resource *r, int flags,
    driver_filter_t filter, driver_intr_t handler, void *arg, void **cookiep)
{

	dev->dev_irq_filter = filter;
	dev->dev_irq_fn = handler;
	dev->dev_irq_arg = arg;

	return (0);
}

int
bus_teardown_intr(device_t dev, struct resource *r, void *cookie)
{

	dev->dev_irq_filter = NULL;
	dev->dev_irq_fn = NULL;
	dev->dev_irq_arg = NULL;

	return (0);
}

int
bus_release_resource(device_t dev, int type, int rid, struct resource *r)
{
	/* Resource releasing is not supported */
	return (EINVAL);
}

void
bus_attach_children(device_t dev)
{
	device_t child;

	TAILQ_FOREACH(child, &dev->dev_children, dev_link) {
		device_probe_and_attach(child);
	}
}

bus_space_tag_t
rman_get_bustag(struct resource *r)
{

	return (r->r_bustag);
}

bus_space_handle_t
rman_get_bushandle(struct resource *r)
{

	return (r->r_bushandle);
}

u_long
rman_get_size(struct resource *r)
{

	return (r->__r_i->r_end - r->__r_i->r_start + 1);
}

int
ofw_bus_status_okay(device_t dev)
{
	if (ofw_bus_status_ok_cb == NULL)
		return (0);

	return ((*ofw_bus_status_ok_cb)(dev));
}

int
ofw_bus_is_compatible(device_t dev, char *name)
{
	if (ofw_bus_is_compatible_cb == NULL)
		return (0);

	return ((*ofw_bus_is_compatible_cb)(dev, name));
}

/*------------------------------------------------------------------------*
 * Implementation of mutex API
 *------------------------------------------------------------------------*/

struct mtx Giant;

static void
mtx_system_init(void *arg)
{
	mtx_init(&Giant, "Giant", NULL, MTX_DEF | MTX_RECURSE);
}
SYSINIT(mtx_system_init, SI_SUB_LOCK, SI_ORDER_MIDDLE, mtx_system_init, NULL);

void
mtx_init(struct mtx *mtx, const char *name, const char *type, int opt)
{
	mtx->owned = 0;
	mtx->parent = mtx;
}

void
mtx_lock(struct mtx *mtx)
{
	mtx = mtx->parent;
	mtx->owned++;
}

void
mtx_unlock(struct mtx *mtx)
{
	mtx = mtx->parent;
	mtx->owned--;
}

int
mtx_owned(struct mtx *mtx)
{
	mtx = mtx->parent;
	return (mtx->owned != 0);
}

void
mtx_destroy(struct mtx *mtx)
{
	/* NOP */
}

/*------------------------------------------------------------------------*
 * Implementation of shared/exclusive mutex API
 *------------------------------------------------------------------------*/

void
sx_init_flags(struct sx *sx, const char *name, int flags)
{
	sx->owned = 0;
}

void
sx_destroy(struct sx *sx)
{
	/* NOP */
}

void
sx_xlock(struct sx *sx)
{
	sx->owned++;
}

void
sx_xunlock(struct sx *sx)
{
	sx->owned--;
}

int
sx_xlocked(struct sx *sx)
{
	return (sx->owned != 0);
}

/*------------------------------------------------------------------------*
 * Implementaiton of condition variable API
 *------------------------------------------------------------------------*/

void
cv_init(struct cv *cv, const char *desc)
{
	cv->sleeping = 0;
}

void
cv_destroy(struct cv *cv)
{
	/* NOP */
}

void
cv_wait(struct cv *cv, struct mtx *mtx)
{
	cv_timedwait(cv, mtx, -1);
}

int
cv_timedwait(struct cv *cv, struct mtx *mtx, int timo)
{
	int start = ticks;
	int delta;
	int time = 0;

	if (cv->sleeping)
		return (EWOULDBLOCK);	/* not allowed */

	cv->sleeping = 1;

	while (cv->sleeping) {
		if (timo >= 0) {
			delta = ticks - start;
			if (delta >= timo || delta < 0)
				break;
		}
		mtx_unlock(mtx);

		usb_idle();

		if (++time >= (1000000 / hz)) {
			time = 0;
			callout_process(1);
		}

		/* Sleep for 1 us */
		delay(1);

		mtx_lock(mtx);
	}

	if (cv->sleeping) {
		cv->sleeping = 0;
		return (EWOULDBLOCK);	/* not allowed */
	}
	return (0);
}

void
cv_signal(struct cv *cv)
{
	cv->sleeping = 0;
}

void
cv_broadcast(struct cv *cv)
{
	cv->sleeping = 0;
}

/*------------------------------------------------------------------------*
 * Implementation of callout API
 *------------------------------------------------------------------------*/

static void callout_proc_msg(struct usb_proc_msg *);

volatile int ticks = 0;

static LIST_HEAD(, callout) head_callout = LIST_HEAD_INITIALIZER(&head_callout);

static struct mtx mtx_callout;
static struct usb_proc_msg callout_msg[2];

static void
callout_system_init(void *arg)
{
	mtx_init(&mtx_callout, "callout-mtx", NULL, MTX_DEF | MTX_RECURSE);

	callout_msg[0].pm_callback = &callout_proc_msg;
	callout_msg[1].pm_callback = &callout_proc_msg;
}
SYSINIT(callout_system_init, SI_SUB_LOCK, SI_ORDER_MIDDLE, callout_system_init, NULL);

static void
callout_callback(struct callout *c)
{
	mtx_lock(c->mtx);

	mtx_lock(&mtx_callout);
	if (c->entry.le_prev != NULL) {
		LIST_REMOVE(c, entry);
		c->entry.le_prev = NULL;
	}
	mtx_unlock(&mtx_callout);

	if (c->c_func != NULL)
		(c->c_func) (c->c_arg);

	if (!(c->flags & CALLOUT_RETURNUNLOCKED))
		mtx_unlock(c->mtx);
}

void
callout_process(int timeout)
{
	ticks += timeout;
	usb_proc_msignal(usb_process + 2, &callout_msg[0], &callout_msg[1]);
}

static void
callout_proc_msg(struct usb_proc_msg *pmsg)
{
	struct callout *c;
	int delta;

repeat:
	mtx_lock(&mtx_callout);

	LIST_FOREACH(c, &head_callout, entry) {

		delta = c->timeout - ticks;
		if (delta < 0) {
			mtx_unlock(&mtx_callout);

			callout_callback(c);

			goto repeat;
		}
	}
	mtx_unlock(&mtx_callout);
}

void
callout_init_mtx(struct callout *c, struct mtx *mtx, int flags)
{
	memset(c, 0, sizeof(*c));

	if (mtx == NULL)
		mtx = &Giant;

	c->mtx = mtx;
	c->flags = (flags & CALLOUT_RETURNUNLOCKED);
}

void
callout_reset(struct callout *c, int to_ticks,
    void (*func) (void *), void *arg)
{
	callout_stop(c);

	c->c_func = func;
	c->c_arg = arg;
	c->timeout = ticks + to_ticks;

	mtx_lock(&mtx_callout);
	LIST_INSERT_HEAD(&head_callout, c, entry);
	mtx_unlock(&mtx_callout);
}

void
callout_stop(struct callout *c)
{
	mtx_lock(&mtx_callout);

	if (c->entry.le_prev != NULL) {
		LIST_REMOVE(c, entry);
		c->entry.le_prev = NULL;
	}
	mtx_unlock(&mtx_callout);

	c->c_func = NULL;
	c->c_arg = NULL;
}

void
callout_drain(struct callout *c)
{
	if (c->mtx == NULL)
		return;			/* not initialised */

	mtx_lock(c->mtx);
	callout_stop(c);
	mtx_unlock(c->mtx);
}

int
callout_pending(struct callout *c)
{
	int retval;

	mtx_lock(&mtx_callout);
	retval = (c->entry.le_prev != NULL);
	mtx_unlock(&mtx_callout);

	return (retval);
}

/*------------------------------------------------------------------------*
 * Implementation of device API
 *------------------------------------------------------------------------*/

static const char unknown_string[] = { "unknown" };

static TAILQ_HEAD(, module_data) module_head =
    TAILQ_HEAD_INITIALIZER(module_head);
static TAILQ_HEAD(, devclass) devclasses =
    TAILQ_HEAD_INITIALIZER(devclasses);

int
bus_generic_resume(device_t dev)
{
	return (0);
}

int
bus_generic_shutdown(device_t dev)
{
	return (0);
}

int
bus_generic_suspend(device_t dev)
{
	return (0);
}

int
bus_generic_print_child(device_t dev, device_t child)
{
	return (0);
}

void
bus_generic_driver_added(device_t dev, driver_t *driver)
{
	return;
}

device_t
device_get_parent(device_t dev)
{
	return (dev ? dev->dev_parent : NULL);
}

void
device_set_interrupt(device_t dev, driver_filter_t *filter,
    driver_intr_t *fn, void *arg)
{
	dev->dev_irq_filter = filter;
	dev->dev_irq_fn = fn;
	dev->dev_irq_arg = arg;
}

void
device_run_interrupts(device_t parent)
{
	device_t child;

	if (parent == NULL)
		return;

	TAILQ_FOREACH(child, &parent->dev_children, dev_link) {
		int status;
		if (child->dev_irq_filter != NULL)
			status = child->dev_irq_filter(child->dev_irq_arg);
		else
			status = FILTER_SCHEDULE_THREAD;

		if (status == FILTER_SCHEDULE_THREAD) {
			if (child->dev_irq_fn != NULL)
				(child->dev_irq_fn) (child->dev_irq_arg);
		}
	}
}

void
device_set_ivars(device_t dev, void *ivars)
{
	dev->dev_aux = ivars;
}

void   *
device_get_ivars(device_t dev)
{
	return (dev ? dev->dev_aux : NULL);
}

int
device_get_unit(device_t dev)
{
	return (dev ? dev->dev_unit : 0);
}

int
bus_detach_children(device_t dev)
{
	device_t child;
	int error;

	if (!dev->dev_attached)
		return (EBUSY);

	TAILQ_FOREACH(child, &dev->dev_children, dev_link) {
		if ((error = device_detach(child)) != 0)
			return (error);
	}
	return (0);
}

int
bus_generic_detach(device_t dev)
{
	int error;

	error = bus_detach_children(dev);
	if (error == 0)
		error = device_delete_children(dev);
	return (error);
}

const char *
device_get_nameunit(device_t dev)
{
	if (dev && dev->dev_nameunit[0])
		return (dev->dev_nameunit);

	return (unknown_string);
}

static devclass_t
devclass_create(const char *classname)
{
	devclass_t dc;

	dc = malloc(sizeof(*dc), M_DEVBUF, M_WAITOK | M_ZERO);
	if (dc == NULL) {
		return (NULL);
	}
	dc->name = classname;
	TAILQ_INSERT_TAIL(&devclasses, dc, link);
	return (dc);
}

static devclass_t
devclass_find_create(const char *classname)
{
	devclass_t dc;

	dc = devclass_find(classname);
	if (dc == NULL)
		dc = devclass_create(classname);
	return (dc);
}

static uint8_t
devclass_add_device(devclass_t dc, device_t dev)
{
	device_t *pp_dev;
	device_t *end;
	uint8_t unit;

	pp_dev = dc->dev_list;
	end = pp_dev + DEVCLASS_MAXUNIT;
	unit = 0;

	while (pp_dev != end) {
		if (*pp_dev == NULL) {
			*pp_dev = dev;
			dev->dev_class = dc;
			dev->dev_unit = unit;
			snprintf(dev->dev_nameunit,
			    sizeof(dev->dev_nameunit),
			    "%s%d", dc->name, unit);
			return (0);
		}
		pp_dev++;
		unit++;
	}
	DPRINTF("Could not add device to devclass.\n");
	return (1);
}

static void
devclass_delete_device(devclass_t dc, device_t dev)
{
	if (dc == NULL) {
		return;
	}
	dc->dev_list[dev->dev_unit] = NULL;
	dev->dev_class = NULL;
}

static device_t
make_device(device_t parent, const char *name)
{
	device_t dev = NULL;
	devclass_t dc = NULL;

	if (name) {

		dc = devclass_find_create(name);

		if (!dc) {

			DPRINTF("%s:%d:%s: can't find device "
			    "class %s\n", __FILE__, __LINE__,
			    __FUNCTION__, name);

			goto done;
		}
	}
	dev = malloc(sizeof(*dev),
	    M_DEVBUF, M_WAITOK | M_ZERO);

	if (dev == NULL)
		goto done;

	dev->dev_parent = parent;
	TAILQ_INIT(&dev->dev_children);

	if (name) {
		dev->dev_fixed_class = 1;
		if (devclass_add_device(dc, dev)) {
			goto error;
		}
	}
done:
	return (dev);

error:
	if (dev) {
		free(dev, M_DEVBUF);
	}
	return (NULL);
}

device_t
device_add_child(device_t dev, const char *name, int unit)
{
	device_t child;

	if (unit != -1) {
		device_printf(dev, "Unit is not -1\n");
	}
	child = make_device(dev, name);
	if (child == NULL) {
		device_printf(dev, "Could not add child '%s'\n", name);
		goto done;
	}
	if (dev == NULL) {
		/* no parent */
		goto done;
	}
	TAILQ_INSERT_TAIL(&dev->dev_children, child, dev_link);
done:
	return (child);
}

int
device_delete_child(device_t dev, device_t child)
{
	int error = 0;
	device_t grandchild;

	/* detach parent before deleting children, if any */
	error = device_detach(child);
	if (error)
		goto done;

	/* remove children second */
	while ((grandchild = TAILQ_FIRST(&child->dev_children))) {
		error = device_delete_child(child, grandchild);
		if (error) {
			device_printf(dev, "Error deleting child!\n");
			goto done;
		}
	}

	if (child->dev_class != NULL)
		devclass_delete_device(child->dev_class, child);

	if (dev != NULL) {
		/* remove child from parent */
		TAILQ_REMOVE(&dev->dev_children, child, dev_link);
	}
	free(child, M_DEVBUF);

done:
	return (error);
}

int
device_delete_children(device_t dev)
{
	device_t child;
	int error = 0;

	while ((child = TAILQ_FIRST(&dev->dev_children))) {
		error = device_delete_child(dev, child);
		if (error) {
			device_printf(dev, "Error deleting child!\n");
			break;
		}
	}
	return (error);
}

void
device_quiet(device_t dev)
{
	dev->dev_quiet = 1;
}

const char *
device_get_desc(device_t dev)
{
	if (dev)
		return &(dev->dev_desc[0]);
	return (unknown_string);
}

static int
default_method(void)
{
	/* do nothing */
	DPRINTF("Default method called\n");
	return (0);
}

void   *
device_get_method(device_t dev, const char *what)
{
	const struct device_method *mtod;

	mtod = dev->dev_module->driver->methods;
	while (mtod->func != NULL) {
		if (strcmp(mtod->desc, what) == 0) {
			return (mtod->func);
		}
		mtod++;
	}
	return ((void *)&default_method);
}

const char *
device_get_name(device_t dev)
{
	if (dev == NULL || dev->dev_module == NULL)
		return (unknown_string);

	return (dev->dev_module->driver->name);
}

static int
device_allocate_softc(device_t dev)
{
	const struct module_data *mod;

	mod = dev->dev_module;

	if ((dev->dev_softc_alloc == 0) &&
	    (mod->driver->size != 0)) {
		dev->dev_sc = malloc(mod->driver->size,
		    M_DEVBUF, M_WAITOK | M_ZERO);

		if (dev->dev_sc == NULL)
			return (ENOMEM);

		dev->dev_softc_alloc = 1;
	}
	return (0);
}

int
device_probe_and_attach(device_t dev)
{
	const struct module_data *mod;
	const char *bus_name_parent;
	devclass_t dc;

	if (dev->dev_attached)
		return (0);		/* fail-safe */

	/*
         * Find a module for our device, if any
         */
	bus_name_parent = device_get_name(device_get_parent(dev));

	TAILQ_FOREACH(mod, &module_head, entry) {
		if (strcmp(mod->bus_name, bus_name_parent) != 0)
			continue;

		dc = devclass_find(mod->mod_name);

		/* Does this device need assigning to the new devclass? */
		if (dev->dev_class != dc) {
			if (dev->dev_fixed_class)
				continue;
			if (dev->dev_class != NULL)
				devclass_delete_device(dev->dev_class, dev);
			if (devclass_add_device(dc, dev)) {
				continue;
			}
		}

		dev->dev_module = mod;
		if (DEVICE_PROBE(dev) <= 0) {

			if (device_allocate_softc(dev) == 0) {

				if (DEVICE_ATTACH(dev) == 0) {
					/* success */
					dev->dev_attached = 1;
					return (0);
				}
			}
		}
		/* else try next driver */

		device_detach(dev);
	}

	return (ENODEV);
}

int
device_detach(device_t dev)
{
	const struct module_data *mod = dev->dev_module;
	int error;

	if (dev->dev_attached) {

		error = DEVICE_DETACH(dev);
		if (error) {
			return error;
		}
		dev->dev_attached = 0;
	}
	device_set_softc(dev, NULL);
	dev->dev_module = NULL;

	if (dev->dev_fixed_class == 0)
		devclass_delete_device(dev->dev_class, dev);

	return (0);
}

void
device_set_softc(device_t dev, void *softc)
{
	if (dev->dev_softc_alloc) {
		free(dev->dev_sc, M_DEVBUF);
		dev->dev_sc = NULL;
	}
	dev->dev_sc = softc;
	dev->dev_softc_alloc = 0;
}

void   *
device_get_softc(device_t dev)
{
	if (dev == NULL)
		return (NULL);

	return (dev->dev_sc);
}

int
device_is_attached(device_t dev)
{
	return (dev->dev_attached);
}

void
device_set_desc(device_t dev, const char *desc)
{
	snprintf(dev->dev_desc, sizeof(dev->dev_desc), "%s", desc);
}

void
device_set_desc_copy(device_t dev, const char *desc)
{
	device_set_desc(dev, desc);
}

void   *
devclass_get_softc(devclass_t dc, int unit)
{
	return (device_get_softc(devclass_get_device(dc, unit)));
}

int
devclass_get_maxunit(devclass_t dc)
{
	int max_unit = 0;

	if (dc) {
		max_unit = DEVCLASS_MAXUNIT;
		while (max_unit--) {
			if (dc->dev_list[max_unit]) {
				break;
			}
		}
		max_unit++;
	}
	return (max_unit);
}

device_t
devclass_get_device(devclass_t dc, int unit)
{
	return (((unit < 0) || (unit >= DEVCLASS_MAXUNIT) || (dc == NULL)) ?
	    NULL : dc->dev_list[unit]);
}

devclass_t
devclass_find(const char *classname)
{
	devclass_t dc;

	TAILQ_FOREACH(dc, &devclasses, link) {
		if (strcmp(dc->name, classname) == 0)
			return (dc);
	}
	return (NULL);
}

void
module_register(void *data)
{
	struct module_data *mdata = data;

	TAILQ_INSERT_TAIL(&module_head, mdata, entry);
	(void)devclass_find_create(mdata->mod_name);
}

/*------------------------------------------------------------------------*
 * System startup
 *------------------------------------------------------------------------*/

static void
sysinit_run(const void **ppdata)
{
	const struct sysinit *psys;

	while ((psys = *ppdata) != NULL) {
		(psys->func) (psys->data);
		ppdata++;
	}
}

/*------------------------------------------------------------------------*
 * USB process API
 *------------------------------------------------------------------------*/

static int usb_do_process(struct usb_process *);
static int usb_proc_level = -1;
static struct mtx usb_proc_mtx;

void
usb_idle(void)
{
	int old_level = usb_proc_level;
	int old_giant = Giant.owned;
	int worked;

	device_run_interrupts(usb_pci_root);

	do {
		worked = 0;
		Giant.owned = 0;

		while (++usb_proc_level < USB_PROC_MAX)
			worked |= usb_do_process(usb_process + usb_proc_level);

		usb_proc_level = old_level;
		Giant.owned = old_giant;

	} while (worked);
}

void
usb_init(void)
{
	sysinit_run(sysinit_data);
}

void
usb_uninit(void)
{
	sysinit_run(sysuninit_data);
}

static void
usb_process_init_sub(struct usb_process *up)
{
	TAILQ_INIT(&up->up_qhead);

	cv_init(&up->up_cv, "-");
	cv_init(&up->up_drain, "usbdrain");

	up->up_mtx = &usb_proc_mtx;
}

static void
usb_process_init(void *arg)
{
	uint8_t x;

	mtx_init(&usb_proc_mtx, "usb-proc-mtx", NULL, MTX_DEF | MTX_RECURSE);

	for (x = 0; x != USB_PROC_MAX; x++)
		usb_process_init_sub(&usb_process[x]);

}
SYSINIT(usb_process_init, SI_SUB_LOCK, SI_ORDER_MIDDLE, usb_process_init, NULL);

static int
usb_do_process(struct usb_process *up)
{
	struct usb_proc_msg *pm;
	int worked = 0;

	mtx_lock(&usb_proc_mtx);

repeat:
	pm = TAILQ_FIRST(&up->up_qhead);

	if (pm != NULL) {

		worked = 1;

		(pm->pm_callback) (pm);

		if (pm == TAILQ_FIRST(&up->up_qhead)) {
			/* nothing changed */
			TAILQ_REMOVE(&up->up_qhead, pm, pm_qentry);
			pm->pm_qentry.tqe_prev = NULL;
		}
		goto repeat;
	}
	mtx_unlock(&usb_proc_mtx);

	return (worked);
}

void   *
usb_proc_msignal(struct usb_process *up, void *_pm0, void *_pm1)
{
	struct usb_proc_msg *pm0 = _pm0;
	struct usb_proc_msg *pm1 = _pm1;
	struct usb_proc_msg *pm2;
	usb_size_t d;
	uint8_t t;

	t = 0;

	if (pm0->pm_qentry.tqe_prev) {
		t |= 1;
	}
	if (pm1->pm_qentry.tqe_prev) {
		t |= 2;
	}
	if (t == 0) {
		/*
		 * No entries are queued. Queue "pm0" and use the existing
		 * message number.
		 */
		pm2 = pm0;
	} else if (t == 1) {
		/* Check if we need to increment the message number. */
		if (pm0->pm_num == up->up_msg_num) {
			up->up_msg_num++;
		}
		pm2 = pm1;
	} else if (t == 2) {
		/* Check if we need to increment the message number. */
		if (pm1->pm_num == up->up_msg_num) {
			up->up_msg_num++;
		}
		pm2 = pm0;
	} else if (t == 3) {
		/*
		 * Both entries are queued. Re-queue the entry closest to
		 * the end.
		 */
		d = (pm1->pm_num - pm0->pm_num);

		/* Check sign after subtraction */
		if (d & 0x80000000) {
			pm2 = pm0;
		} else {
			pm2 = pm1;
		}

		TAILQ_REMOVE(&up->up_qhead, pm2, pm_qentry);
	} else {
		pm2 = NULL;		/* panic - should not happen */
	}

	/* Put message last on queue */

	pm2->pm_num = up->up_msg_num;
	TAILQ_INSERT_TAIL(&up->up_qhead, pm2, pm_qentry);

	return (pm2);
}

/*------------------------------------------------------------------------*
 *	usb_proc_is_gone
 *
 * Return values:
 *    0: USB process is running
 * Else: USB process is tearing down
 *------------------------------------------------------------------------*/
uint8_t
usb_proc_is_gone(struct usb_process *up)
{
	return (0);
}

/*------------------------------------------------------------------------*
 *	usb_proc_mwait
 *
 * This function will return when the USB process message pointed to
 * by "pm" is no longer on a queue. This function must be called
 * having "usb_proc_mtx" locked.
 *------------------------------------------------------------------------*/
void
usb_proc_mwait(struct usb_process *up, void *_pm0, void *_pm1)
{
	struct usb_proc_msg *pm0 = _pm0;
	struct usb_proc_msg *pm1 = _pm1;

	/* Just remove the messages from the queue. */
	if (pm0->pm_qentry.tqe_prev) {
		TAILQ_REMOVE(&up->up_qhead, pm0, pm_qentry);
		pm0->pm_qentry.tqe_prev = NULL;
	}
	if (pm1->pm_qentry.tqe_prev) {
		TAILQ_REMOVE(&up->up_qhead, pm1, pm_qentry);
		pm1->pm_qentry.tqe_prev = NULL;
	}
}

/*------------------------------------------------------------------------*
 * SYSTEM attach
 *------------------------------------------------------------------------*/

#ifdef USB_PCI_PROBE_LIST
static device_method_t pci_methods[] = {
	DEVMETHOD_END
};

static driver_t pci_driver = {
	.name = "pci",
	.methods = pci_methods,
};

static devclass_t pci_devclass;

DRIVER_MODULE(pci, pci, pci_driver, pci_devclass, 0, 0);

static const char *usb_pci_devices[] = {
	USB_PCI_PROBE_LIST
};

#define	USB_PCI_USB_MAX	(sizeof(usb_pci_devices) / sizeof(void *))

static device_t usb_pci_dev[USB_PCI_USB_MAX];

static void
usb_pci_mod_load(void *arg)
{
	uint32_t x;

	usb_pci_root = device_add_child(NULL, "pci", DEVICE_UNIT_ANY);
	if (usb_pci_root == NULL)
		return;

	for (x = 0; x != USB_PCI_USB_MAX; x++) {
		usb_pci_dev[x] = device_add_child(usb_pci_root,
		    usb_pci_devices[x], DEVICE_UNIT_ANY);
		if (usb_pci_dev[x] == NULL)
			continue;
		if (device_probe_and_attach(usb_pci_dev[x])) {
			device_printf(usb_pci_dev[x],
			    "WARNING: Probe and attach failed!\n");
		}
	}
}
SYSINIT(usb_pci_mod_load, SI_SUB_RUN_SCHEDULER, SI_ORDER_MIDDLE, usb_pci_mod_load, 0);

static void
usb_pci_mod_unload(void *arg)
{
	uint32_t x;

	for (x = 0; x != USB_PCI_USB_MAX; x++) {
		if (usb_pci_dev[x]) {
			device_detach(usb_pci_dev[x]);
			device_delete_child(usb_pci_root, usb_pci_dev[x]);
		}
	}
	if (usb_pci_root)
		device_delete_child(NULL, usb_pci_root);
}
SYSUNINIT(usb_pci_mod_unload, SI_SUB_RUN_SCHEDULER, SI_ORDER_MIDDLE, usb_pci_mod_unload, 0);
#endif

/*------------------------------------------------------------------------*
 * MALLOC API
 *------------------------------------------------------------------------*/

#ifndef HAVE_MALLOC
#define	USB_POOL_ALIGN 8

static uint8_t usb_pool[USB_POOL_SIZE] __aligned(USB_POOL_ALIGN);
static uint32_t usb_pool_rem = USB_POOL_SIZE;
static uint32_t usb_pool_entries;

struct malloc_hdr {
	TAILQ_ENTRY(malloc_hdr) entry;
	uint32_t size;
} __aligned(USB_POOL_ALIGN);

static TAILQ_HEAD(, malloc_hdr) malloc_head =
	TAILQ_HEAD_INITIALIZER(malloc_head);

void   *
usb_malloc(unsigned long size)
{
	struct malloc_hdr *hdr;

	size = (size + USB_POOL_ALIGN - 1) & ~(USB_POOL_ALIGN - 1);
	size += sizeof(struct malloc_hdr);

	TAILQ_FOREACH(hdr, &malloc_head, entry) {
		if (hdr->size == size)
			break;
	}

	if (hdr) {
		DPRINTF("MALLOC: Entries = %d; Remainder = %d; Size = %d\n",
		    (int)usb_pool_entries, (int)usb_pool_rem, (int)size);

		TAILQ_REMOVE(&malloc_head, hdr, entry);
		memset(hdr + 1, 0, hdr->size - sizeof(*hdr));
		return (hdr + 1);
	}
	if (usb_pool_rem >= size) {
		hdr = (void *)(usb_pool + USB_POOL_SIZE - usb_pool_rem);
		hdr->size = size;

		usb_pool_rem -= size;
		usb_pool_entries++;

		DPRINTF("MALLOC: Entries = %d; Remainder = %d; Size = %d\n",
		    (int)usb_pool_entries, (int)usb_pool_rem, (int)size);

		memset(hdr + 1, 0, hdr->size - sizeof(*hdr));
		return (hdr + 1);
	}
	return (NULL);
}

void
usb_free(void *arg)
{
	struct malloc_hdr *hdr;

	if (arg == NULL)
		return;

	hdr = arg;
	hdr--;

	TAILQ_INSERT_TAIL(&malloc_head, hdr, entry);
}
#endif

char   *
usb_strdup(const char *str)
{
	char *tmp;
	int len;

	len = 1 + strlen(str);

	tmp = malloc(len,XXX,XXX);
	if (tmp == NULL)
		return (NULL);

	memcpy(tmp, str, len);
	return (tmp);
}
