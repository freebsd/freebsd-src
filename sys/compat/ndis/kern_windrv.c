/*-
 * Copyright (c) 2005
 *      Bill Paul <wpaul@windriver.com>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/unistd.h>
#include <sys/types.h>

#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/module.h>
#include <sys/conf.h>
#include <sys/mbuf.h>
#include <sys/bus.h>

#include <sys/queue.h>

#include <compat/ndis/pe_var.h>
#include <compat/ndis/cfg_var.h>
#include <compat/ndis/resource_var.h>
#include <compat/ndis/ntoskrnl_var.h>
#include <compat/ndis/ndis_var.h>
#include <compat/ndis/hal_var.h>

struct windrv_type {
	uint16_t		windrv_vid;	/* for PCI or USB */
	uint16_t		windrv_did;	/* for PCI or USB */
	uint32_t		windrv_subsys;	/* for PCI */
	char			*windrv_vname;	/* for pccard */
	char			*windrv_dname;	/* for pccard */
	char			*windrv_name;	/* for pccard, PCI or USB */
};

struct drvdb_ent {
	driver_object		*windrv_object;
	struct windrv_type	*windrv_devlist;
	ndis_cfg		*windrv_regvals;
	STAILQ_ENTRY(drvdb_ent)	link;
};

struct mtx drvdb_mtx;
static STAILQ_HEAD(drvdb, drvdb_ent) drvdb_head;

static driver_object	fake_pci_driver; /* serves both PCI and cardbus */
static driver_object	fake_pccard_driver;


#define DUMMY_REGISTRY_PATH "\\\\some\\bogus\\path"

int
windrv_libinit(void)
{
	STAILQ_INIT(&drvdb_head);
	mtx_init(&drvdb_mtx, "Windows driver DB lock",
           "Windows internal lock", MTX_DEF);

	/*
	 * PCI and pccard devices don't need to use IRPs to
	 * interact with their bus drivers (usually), so our
	 * emulated PCI and pccard drivers are just stubs.
	 * USB devices, on the other hand, do all their I/O
	 * by exchanging IRPs with the USB bus driver, so
	 * for that we need to provide emulator dispatcher
	 * routines, which are in a separate module.
	 */

	windrv_bus_attach(&fake_pci_driver, "PCI Bus");
	windrv_bus_attach(&fake_pccard_driver, "PCCARD Bus");

	return(0);
}

int
windrv_libfini(void)
{
	struct drvdb_ent	*d;

	mtx_lock(&drvdb_mtx); 
	while(STAILQ_FIRST(&drvdb_head) != NULL) {
		d = STAILQ_FIRST(&drvdb_head);
		STAILQ_REMOVE_HEAD(&drvdb_head, link);
		free(d, M_DEVBUF);
	}
	mtx_unlock(&drvdb_mtx);

	free(fake_pci_driver.dro_drivername.us_buf, M_DEVBUF);
	free(fake_pccard_driver.dro_drivername.us_buf, M_DEVBUF);

	mtx_destroy(&drvdb_mtx);
	return(0);
}

/*
 * Given the address of a driver image, find its corresponding
 * driver_object.
 */

driver_object *
windrv_lookup(img)
	vm_offset_t		img;
{
	struct drvdb_ent	*d;

	mtx_lock(&drvdb_mtx); 
	STAILQ_FOREACH(d, &drvdb_head, link) {
		if (d->windrv_object->dro_driverstart == (void *)img) {
			mtx_unlock(&drvdb_mtx);
			return(d->windrv_object);
		}
	}
	mtx_unlock(&drvdb_mtx);

	return(NULL);
}

/*
 * Remove a driver_object from our datatabase and destroy it. Throw
 * away any custom driver extension info that may have been added.
 */

int
windrv_unload(mod, img, len)
	module_t		mod;
	vm_offset_t		img;
	int			len;
{
	struct drvdb_ent	*d, *r = NULL;
	driver_object		*drv;
	list_entry		*e, *c;

	mtx_lock(&drvdb_mtx); 
	STAILQ_FOREACH(d, &drvdb_head, link) {
		if (d->windrv_object->dro_driverstart == (void *)img) {
			r = d;
			STAILQ_REMOVE(&drvdb_head, d, drvdb_ent, link);
			break;
		}
	}
	mtx_unlock(&drvdb_mtx);

	if (r == NULL)
		return (ENOENT);

        /*
	 * Destroy any custom extensions that may have been added.
	 */
	drv = r->windrv_object;
	e = drv->dro_driverext->dre_usrext.nle_flink;
	while (e != &drv->dro_driverext->dre_usrext) {
		c = e->nle_flink;
		REMOVE_LIST_ENTRY(e);
		ExFreePool(e);
		e = c;
	}

	/* Free the driver extension */
	free(drv->dro_driverext, M_DEVBUF);

	/* Free the driver name */
	free(drv->dro_drivername.us_buf, M_DEVBUF);

	/* Free driver object */
	free(drv, M_DEVBUF);

	/* Free our DB handle */
	free(r, M_DEVBUF);

	return(0);
}

/*
 * Loader routine for actual Windows driver modules, ultimately
 * calls the driver's DriverEntry() routine.
 */

int
windrv_load(mod, img, len)
	module_t		mod;
	vm_offset_t		img;
	int			len;
{
	image_import_descriptor	imp_desc;
	image_optional_header	opt_hdr;
	driver_entry		entry;
	struct drvdb_ent	*new;
	struct driver_object	*dobj;
	int			status;

	/*
	 * First step: try to relocate and dynalink the executable
	 * driver image.
	 */

	/* Perform text relocation */
	if (pe_relocate(img))
		return(ENOEXEC);

	/* Dynamically link the NDIS.SYS routines -- required. */
	if (pe_patch_imports(img, "NDIS", ndis_functbl))
		return(ENOEXEC);

	/* Dynamically link the HAL.dll routines -- also required. */
	if (pe_patch_imports(img, "HAL", hal_functbl))
		return(ENOEXEC);

	/* Dynamically link ntoskrnl.exe -- optional. */
	if (pe_get_import_descriptor(img, &imp_desc, "ntoskrnl") == 0) {
		if (pe_patch_imports(img, "ntoskrnl", ntoskrnl_functbl))
			return(ENOEXEC);
	}

	/* Dynamically link USBD.SYS -- optional */
#ifdef notyet
	if (pe_get_import_descriptor(img, &imp_desc, "USBD") == 0) {
		if (pe_patch_imports(img, "USBD", ntoskrnl_functbl))
			return(ENOEXEC);
	}
#endif

	/* Next step: find the driver entry point. */

	pe_get_optional_header(img, &opt_hdr);
	entry = (driver_entry)pe_translate_addr(img, opt_hdr.ioh_entryaddr);

	/* Next step: allocate and store a driver object. */

	new = malloc(sizeof(struct drvdb_ent), M_DEVBUF, M_NOWAIT);
	if (new == NULL)
		return (ENOMEM);

	dobj = malloc(sizeof(device_object), M_DEVBUF, M_NOWAIT|M_ZERO);
	if (dobj == NULL) {
		free (new, M_DEVBUF);
		return (ENOMEM);
	}

	/* Allocate a driver extension structure too. */

	dobj->dro_driverext = malloc(sizeof(driver_extension),
	    M_DEVBUF, M_NOWAIT|M_ZERO);

	if (dobj->dro_driverext == NULL) {
		free(new, M_DEVBUF);
		free(dobj, M_DEVBUF);
		return(ENOMEM);
	}

	INIT_LIST_HEAD((&dobj->dro_driverext->dre_usrext));

	dobj->dro_driverstart = (void *)img;
	dobj->dro_driversize = len;

	dobj->dro_drivername.us_len = strlen(DUMMY_REGISTRY_PATH) * 2;
        dobj->dro_drivername.us_maxlen = strlen(DUMMY_REGISTRY_PATH) * 2;
        dobj->dro_drivername.us_buf = NULL;
        ndis_ascii_to_unicode(DUMMY_REGISTRY_PATH,
	    &dobj->dro_drivername.us_buf);

	new->windrv_object = dobj;

	/* Now call the DriverEntry() function. */

	status = MSCALL2(entry, dobj, &dobj->dro_drivername);

	if (status != STATUS_SUCCESS) {
		free(dobj->dro_drivername.us_buf, M_DEVBUF);
		free(dobj, M_DEVBUF);
		free(new, M_DEVBUF);
		return(ENODEV);
	}

	mtx_lock(&drvdb_mtx); 
	STAILQ_INSERT_HEAD(&drvdb_head, new, link);
	mtx_unlock(&drvdb_mtx); 

	return (0);
}

/*
 * Make a new Physical Device Object for a device that was
 * detected/plugged in. For us, the PDO is just a way to
 * get at the device_t.
 */

int
windrv_create_pdo(drv, bsddev)
	driver_object		*drv;
	device_t		bsddev;
{
	device_object		*dev;

	/*
	 * This is a new physical device object, which technically
	 * is the "top of the stack." Consequently, we don't do
	 * an IoAttachDeviceToDeviceStack() here.
	 */

	mtx_lock(&drvdb_mtx);
	IoCreateDevice(drv, 0, NULL, FILE_DEVICE_UNKNOWN, 0, FALSE, &dev);
	mtx_unlock(&drvdb_mtx);

	/* Stash pointer to our BSD device handle. */

	dev->do_devext = bsddev;

	return(STATUS_SUCCESS);
}

void
windrv_destroy_pdo(drv, bsddev)
	driver_object		*drv;
	device_t		bsddev;
{
	device_object		*pdo;

	pdo = windrv_find_pdo(drv, bsddev);

	/* Remove reference to device_t */

	pdo->do_devext = NULL;

	mtx_lock(&drvdb_mtx);
	IoDeleteDevice(pdo);
	mtx_unlock(&drvdb_mtx);

	return;
}

/*
 * Given a device_t, find the corresponding PDO in a driver's
 * device list.
 */

device_object *
windrv_find_pdo(drv, bsddev)
	driver_object		*drv;
	device_t		bsddev;
{
	device_object		*pdo;

	mtx_lock(&drvdb_mtx);
	pdo = drv->dro_devobj;
	if (pdo->do_devext != bsddev) {
		mtx_unlock(&drvdb_mtx);
		panic("PDO wasn't first device in list");
	}
	mtx_unlock(&drvdb_mtx);

	return(pdo);
}

/*
 * Add an internally emulated driver to the database. We need this
 * to set up an emulated bus driver so that it can receive IRPs.
 */

int
windrv_bus_attach(drv, name)
	driver_object		*drv;
	char			*name;
{
	struct drvdb_ent	*new;

	new = malloc(sizeof(struct drvdb_ent), M_DEVBUF, M_NOWAIT);
	if (new == NULL)
		return (ENOMEM);

	drv->dro_drivername.us_len = strlen(name) * 2;
        drv->dro_drivername.us_maxlen = strlen(name) * 2;
        drv->dro_drivername.us_buf = NULL;
        ndis_ascii_to_unicode(name, &drv->dro_drivername.us_buf);

	new->windrv_object = drv;
	new->windrv_devlist = NULL;
	new->windrv_regvals = NULL;

	mtx_lock(&drvdb_mtx); 
	STAILQ_INSERT_HEAD(&drvdb_head, new, link);
	mtx_unlock(&drvdb_mtx);

	return(0);
}

#ifdef __amd64__

extern void	x86_64_wrap(void);
extern void	x86_64_wrap_call(void);
extern void	x86_64_wrap_end(void);

#endif /* __amd64__ */

int
windrv_wrap(func, wrap)
	funcptr			func;
	funcptr			*wrap;
{
#ifdef __amd64__
	funcptr			p;
	vm_offset_t		*calladdr;
	vm_offset_t		wrapstart, wrapend, wrapcall;

	wrapstart = (vm_offset_t)&x86_64_wrap;
	wrapend = (vm_offset_t)&x86_64_wrap_end;
	wrapcall = (vm_offset_t)&x86_64_wrap_call;

	/* Allocate a new wrapper instance. */

	p = malloc((wrapend - wrapstart), M_DEVBUF, M_NOWAIT);
	if (p == NULL)
		return(ENOMEM);

	/* Copy over the code. */

	bcopy((char *)wrapstart, p, (wrapend - wrapstart));

	/* Insert the function address into the new wrapper instance. */

	calladdr = (uint64_t *)((char *)p + (wrapcall - wrapstart) + 2);
	*calladdr = (vm_offset_t)func;

	*wrap = p;
#else /* __amd64__ */
	*wrap = func;
#endif /* __amd64__ */
	return(0);
}

int
windrv_unwrap(func)
	funcptr			func;
{
#ifdef __amd64__
	free(func, M_DEVBUF);
#endif /* __amd64__ */
	return(0);
}
