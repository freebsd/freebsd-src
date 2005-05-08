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
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/smp.h>

#include <sys/queue.h>

#ifdef __i386__
#include <machine/segments.h>
#endif

#include <compat/ndis/pe_var.h>
#include <compat/ndis/cfg_var.h>
#include <compat/ndis/resource_var.h>
#include <compat/ndis/ntoskrnl_var.h>
#include <compat/ndis/ndis_var.h>
#include <compat/ndis/hal_var.h>
#include <compat/ndis/usbd_var.h>

struct mtx drvdb_mtx;
static STAILQ_HEAD(drvdb, drvdb_ent) drvdb_head;

static driver_object	fake_pci_driver; /* serves both PCI and cardbus */
static driver_object	fake_pccard_driver;

#ifdef __i386__
static void x86_oldldt(void *);
static void x86_newldt(void *);

struct tid {
	void			*tid_except_list;	/* 0x00 */
	uint32_t		tid_oldfs;		/* 0x04 */
	uint32_t		tid_selector;		/* 0x08 */
	struct tid		*tid_self;		/* 0x0C */
	int			tid_cpu;		/* 0x10 */
};

static struct tid	*my_tids;
#endif /* __i386__ */

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

#ifdef __i386__

	/*
	 * In order to properly support SMP machines, we have
	 * to modify the GDT on each CPU, since we never know
	 * on which one we'll end up running.
	 */

	my_tids = ExAllocatePoolWithTag(NonPagedPool,
	    sizeof(struct tid) * mp_ncpus, 0);
	if (my_tids == NULL)
		panic("failed to allocate thread info blocks");
	smp_rendezvous(NULL, x86_newldt, NULL, NULL);
#endif
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

#ifdef __i386__
	smp_rendezvous(NULL, x86_oldldt, NULL, NULL);
	ExFreePool(my_tids);
#endif
	return(0);
}

/*
 * Given the address of a driver image, find its corresponding
 * driver_object.
 */

driver_object *
windrv_lookup(img, name)
	vm_offset_t		img;
	char			*name;
{
	struct drvdb_ent	*d;
	unicode_string		us;

	bzero((char *)&us, sizeof(us));

	/* Damn unicode. */

	if (name != NULL) {
	 	us.us_len = strlen(name) * 2;
		us.us_maxlen = strlen(name) * 2;
		us.us_buf = NULL;
		ndis_ascii_to_unicode(name, &us.us_buf);
	}

	mtx_lock(&drvdb_mtx); 
	STAILQ_FOREACH(d, &drvdb_head, link) {
		if (d->windrv_object->dro_driverstart == (void *)img ||
		    (bcmp((char *)d->windrv_object->dro_drivername.us_buf,
		    (char *)us.us_buf, us.us_len) == 0 && us.us_len)) {
			mtx_unlock(&drvdb_mtx);
			if (name != NULL)
				ExFreePool(us.us_buf);
			return(d->windrv_object);
		}
	}
	mtx_unlock(&drvdb_mtx);

	if (name != NULL)
		ExFreePool(us.us_buf);

	return(NULL);
}

struct drvdb_ent *
windrv_match(matchfunc, ctx)
	matchfuncptr		matchfunc;
	void			*ctx;
{
	struct drvdb_ent	*d;
	int			match;

	mtx_lock(&drvdb_mtx); 
	STAILQ_FOREACH(d, &drvdb_head, link) {
		if (d->windrv_devlist == NULL)
			continue;
		match = matchfunc(d->windrv_bustype, d->windrv_devlist, ctx);
		if (match == TRUE) {
			mtx_unlock(&drvdb_mtx);
			return(d);
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
	struct drvdb_ent	*db, *r = NULL;
	driver_object		*drv;
	device_object		*d, *pdo;
	device_t		dev;
	list_entry		*e, *c;

	drv = windrv_lookup(img, NULL);

	/*
	 * When we unload a driver image, we need to force a
	 * detach of any devices that might be using it. We
	 * need the PDOs of all attached devices for this.
	 * Getting at them is a little hard. We basically
	 * have to walk the device lists of all our bus
	 * drivers.
	 */

	mtx_lock(&drvdb_mtx); 
	STAILQ_FOREACH(db, &drvdb_head, link) {
		/*
		 * Fake bus drivers have no devlist info.
		 * If this driver has devlist info, it's
		 * a loaded Windows driver and has no PDOs,
		 * so skip it.
		 */
		if (db->windrv_devlist != NULL)
			continue;
		pdo = db->windrv_object->dro_devobj;
		while (pdo != NULL) {
			d = pdo->do_attacheddev;
			if (d->do_drvobj != drv) {
				pdo = pdo->do_nextdev;
				continue;
			}
			dev = pdo->do_devext;
			pdo = pdo->do_nextdev;
			mtx_unlock(&drvdb_mtx); 
			device_detach(dev);
			mtx_lock(&drvdb_mtx);
		}
	}
 
	STAILQ_FOREACH(db, &drvdb_head, link) {
		if (db->windrv_object->dro_driverstart == (void *)img) {
			r = db;
			STAILQ_REMOVE(&drvdb_head, db, drvdb_ent, link);
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

#define WINDRV_LOADED		htonl(0x42534F44)

/*
 * Loader routine for actual Windows driver modules, ultimately
 * calls the driver's DriverEntry() routine.
 */

int
windrv_load(mod, img, len, bustype, devlist, regvals)
	module_t		mod;
	vm_offset_t		img;
	int			len;
	interface_type		bustype;
	void			*devlist;
	ndis_cfg		*regvals;
{
	image_import_descriptor	imp_desc;
	image_optional_header	opt_hdr;
	driver_entry		entry;
	struct drvdb_ent	*new;
	struct driver_object	*drv;
	int			status;
	uint32_t		*ptr;

	/*
	 * First step: try to relocate and dynalink the executable
	 * driver image.
	 */

	ptr = (uint32_t *)(img + 8);
        if (*ptr == WINDRV_LOADED)
		goto skipreloc;

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
	if (pe_get_import_descriptor(img, &imp_desc, "USBD") == 0) {
		if (pe_patch_imports(img, "USBD", usbd_functbl))
			return(ENOEXEC);
	}

	*ptr = WINDRV_LOADED;

skipreloc:

	/* Next step: find the driver entry point. */

	pe_get_optional_header(img, &opt_hdr);
	entry = (driver_entry)pe_translate_addr(img, opt_hdr.ioh_entryaddr);

	/* Next step: allocate and store a driver object. */

	new = malloc(sizeof(struct drvdb_ent), M_DEVBUF, M_NOWAIT|M_ZERO);
	if (new == NULL)
		return (ENOMEM);

	drv = malloc(sizeof(driver_object), M_DEVBUF, M_NOWAIT|M_ZERO);
	if (drv == NULL) {
		free (new, M_DEVBUF);
		return (ENOMEM);
	}
	
	/* Allocate a driver extension structure too. */

	drv->dro_driverext = malloc(sizeof(driver_extension),
	    M_DEVBUF, M_NOWAIT|M_ZERO);

	if (drv->dro_driverext == NULL) {
		free(new, M_DEVBUF);
		free(drv, M_DEVBUF);
		return(ENOMEM);
	}

	INIT_LIST_HEAD((&drv->dro_driverext->dre_usrext));

	drv->dro_driverstart = (void *)img;
	drv->dro_driversize = len;

	drv->dro_drivername.us_len = strlen(DUMMY_REGISTRY_PATH) * 2;
        drv->dro_drivername.us_maxlen = strlen(DUMMY_REGISTRY_PATH) * 2;
        drv->dro_drivername.us_buf = NULL;
        ndis_ascii_to_unicode(DUMMY_REGISTRY_PATH,
	    &drv->dro_drivername.us_buf);

	new->windrv_object = drv;
	new->windrv_regvals = regvals;
	new->windrv_devlist = devlist;
	new->windrv_bustype = bustype;

	/* Now call the DriverEntry() function. */

	status = MSCALL2(entry, drv, &drv->dro_drivername);

	if (status != STATUS_SUCCESS) {
		free(drv->dro_drivername.us_buf, M_DEVBUF);
		free(drv, M_DEVBUF);
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
	while (pdo != NULL) {
		if (pdo->do_devext == bsddev) {
			mtx_unlock(&drvdb_mtx);
			return(pdo);
		}
		pdo = pdo->do_nextdev;
	}
	mtx_unlock(&drvdb_mtx);

	return(NULL);
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

	new = malloc(sizeof(struct drvdb_ent), M_DEVBUF, M_NOWAIT|M_ZERO);
	if (new == NULL)
		return (ENOMEM);

	drv->dro_drivername.us_len = strlen(name) * 2;
        drv->dro_drivername.us_maxlen = strlen(name) * 2;
        drv->dro_drivername.us_buf = NULL;
        ndis_ascii_to_unicode(name, &drv->dro_drivername.us_buf);

	/*
	 * Set up a fake image pointer to avoid false matches
	 * in windrv_lookup().
	 */
	drv->dro_driverstart = (void *)0xFFFFFFFF;

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

int
windrv_wrap(func, wrap, argcnt, ftype)
	funcptr			func;
	funcptr			*wrap;
	int			argcnt;
	int			ftype;
{
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

	return(0);
}
#endif /* __amd64__ */


#ifdef __i386__

struct x86desc {
	uint16_t		x_lolimit;
	uint16_t		x_base0;
	uint8_t			x_base1;
	uint8_t			x_flags;
	uint8_t			x_hilimit;
	uint8_t			x_base2;
};

struct gdt {
	uint16_t		limit;
	void			*base;
} __attribute__((__packed__));

extern uint16_t	x86_getfs(void);
extern void x86_setfs(uint16_t);
extern void *x86_gettid(void);
extern void x86_critical_enter(void);
extern void x86_critical_exit(void);
extern void x86_getldt(struct gdt *, uint16_t *);
extern void x86_setldt(struct gdt *, uint16_t);

#define SEL_LDT	4		/* local descriptor table */
#define SEL_TO_FS(x)		(((x) << 3))

/*
 * FreeBSD 6.0 and later has a special GDT segment reserved
 * specifically for us, so if GNDIS_SEL is defined, use that.
 * If not, use GTGATE_SEL, which is uninitialized and infrequently
 * used.
 */

#ifdef GNDIS_SEL
#define FREEBSD_EMPTYSEL	GNDIS_SEL
#else
#define FREEBSD_EMPTYSEL	GTGATE_SEL	/* slot 7 */
#endif

/*
 * The meanings of various bits in a descriptor vary a little
 * depending on whether the descriptor will be used as a
 * code, data or system descriptor. (And that in turn depends
 * on which segment register selects the descriptor.)
 * We're only trying to create a data segment, so the definitions
 * below are the ones that apply to a data descriptor.
 */

#define SEGFLAGLO_PRESENT	0x80	/* segment is present */
#define SEGFLAGLO_PRIVLVL	0x60	/* privlevel needed for this seg */
#define SEGFLAGLO_CD		0x10	/* 1 = code/data, 0 = system */
#define SEGFLAGLO_MBZ		0x08	/* must be zero */
#define SEGFLAGLO_EXPANDDOWN	0x04	/* limit expands down */
#define SEGFLAGLO_WRITEABLE	0x02	/* segment is writeable */
#define SEGGLAGLO_ACCESSED	0x01	/* segment has been accessed */

#define SEGFLAGHI_GRAN		0x80	/* granularity, 1 = byte, 0 = page */
#define SEGFLAGHI_BIG		0x40	/* 1 = 32 bit stack, 0 = 16 bit */

/*
 * Context switch from UNIX to Windows. Save the existing value
 * of %fs for this processor, then change it to point to our
 * fake TID. Note that it is also possible to pin ourselves
 * to our current CPU, though I'm not sure this is really
 * necessary. It depends on whether or not an interrupt might
 * preempt us while Windows code is running and we wind up
 * scheduled onto another CPU as a result. So far, it doesn't
 * seem like this is what happens.
 */

void
ctxsw_utow(void)
{
	struct tid		*t;

	t = &my_tids[curthread->td_oncpu];
	t->tid_oldfs = x86_getfs();
	t->tid_cpu = curthread->td_oncpu;

	x86_setfs(SEL_TO_FS(t->tid_selector));

	/* Now entering Windows land, population: you. */

	return;
}

/*
 * Context switch from Windows back to UNIX. Restore %fs to
 * its previous value. This always occurs after a call to
 * ctxsw_utow().
 */

void
ctxsw_wtou(void)
{
	struct tid		*t;

	t = x86_gettid();
	x86_setfs(t->tid_oldfs);

	/* Welcome back to UNIX land, we missed you. */

#ifdef EXTRA_SANITY
	if (t->tid_cpu != curthread->td_oncpu)
		panic("ctxsw GOT MOVED TO OTHER CPU!");
#endif
	return;
}

static int	windrv_wrap_stdcall(funcptr, funcptr *, int);
static int	windrv_wrap_fastcall(funcptr, funcptr *, int);
static int	windrv_wrap_regparm(funcptr, funcptr *);

extern void     x86_fastcall_wrap(void);
extern void     x86_fastcall_wrap_call(void);
extern void     x86_fastcall_wrap_arg(void);
extern void     x86_fastcall_wrap_end(void);

static int
windrv_wrap_fastcall(func, wrap, argcnt)
        funcptr                 func;
        funcptr                 *wrap;
	int8_t			argcnt;
{
        funcptr                 p;
        vm_offset_t             *calladdr;
	uint8_t			*argaddr;
        vm_offset_t             wrapstart, wrapend, wrapcall, wraparg;

        wrapstart = (vm_offset_t)&x86_fastcall_wrap;
        wrapend = (vm_offset_t)&x86_fastcall_wrap_end;
        wrapcall = (vm_offset_t)&x86_fastcall_wrap_call;
        wraparg = (vm_offset_t)&x86_fastcall_wrap_arg;

        /* Allocate a new wrapper instance. */

        p = malloc((wrapend - wrapstart), M_DEVBUF, M_NOWAIT);
        if (p == NULL)
                return(ENOMEM);

        /* Copy over the code. */

	bcopy((char *)wrapstart, p, (wrapend - wrapstart));

        /* Insert the function address into the new wrapper instance. */

	calladdr = (vm_offset_t *)((char *)p + ((wrapcall - wrapstart) + 1));
        *calladdr = (vm_offset_t)func;

	argcnt -= 2;
	if (argcnt < 1)
		argcnt = 0;

	argaddr = (u_int8_t *)((char *)p + ((wraparg - wrapstart) + 1));
	*argaddr = argcnt * sizeof(uint32_t);

        *wrap = p;

        return(0);
}

extern void     x86_stdcall_wrap(void);
extern void     x86_stdcall_wrap_call(void);
extern void     x86_stdcall_wrap_arg(void);
extern void     x86_stdcall_wrap_end(void);

static int
windrv_wrap_stdcall(func, wrap, argcnt)
        funcptr                 func;
        funcptr                 *wrap;
	uint8_t			argcnt;
{
        funcptr                 p;
        vm_offset_t             *calladdr;
	uint8_t			*argaddr;
        vm_offset_t             wrapstart, wrapend, wrapcall, wraparg;

        wrapstart = (vm_offset_t)&x86_stdcall_wrap;
        wrapend = (vm_offset_t)&x86_stdcall_wrap_end;
        wrapcall = (vm_offset_t)&x86_stdcall_wrap_call;
        wraparg = (vm_offset_t)&x86_stdcall_wrap_arg;

        /* Allocate a new wrapper instance. */

        p = malloc((wrapend - wrapstart), M_DEVBUF, M_NOWAIT);
        if (p == NULL)
                return(ENOMEM);

        /* Copy over the code. */

	bcopy((char *)wrapstart, p, (wrapend - wrapstart));

        /* Insert the function address into the new wrapper instance. */

	calladdr = (vm_offset_t *)((char *)p + ((wrapcall - wrapstart) + 1));
        *calladdr = (vm_offset_t)func;

	argaddr = (u_int8_t *)((char *)p + ((wraparg - wrapstart) + 1));
	*argaddr = argcnt * sizeof(uint32_t);

        *wrap = p;

        return(0);
}

extern void     x86_regparm_wrap(void);
extern void     x86_regparm_wrap_call(void);
extern void     x86_regparm_wrap_end(void);

static int
windrv_wrap_regparm(func, wrap)
        funcptr                 func;
        funcptr                 *wrap;
{
        funcptr                 p;
        vm_offset_t             *calladdr;
        vm_offset_t             wrapstart, wrapend, wrapcall;

        wrapstart = (vm_offset_t)&x86_regparm_wrap;
        wrapend = (vm_offset_t)&x86_regparm_wrap_end;
        wrapcall = (vm_offset_t)&x86_regparm_wrap_call;

        /* Allocate a new wrapper instance. */

        p = malloc((wrapend - wrapstart), M_DEVBUF, M_NOWAIT);
        if (p == NULL)
                return(ENOMEM);

        /* Copy over the code. */

        bcopy(x86_regparm_wrap, p, (wrapend - wrapstart));

        /* Insert the function address into the new wrapper instance. */

	calladdr = (vm_offset_t *)((char *)p + ((wrapcall - wrapstart) + 1));
        *calladdr = (vm_offset_t)func;

        *wrap = p;

        return(0);
}

int
windrv_wrap(func, wrap, argcnt, ftype)
        funcptr                 func;
        funcptr                 *wrap;
	int			argcnt;
	int			ftype;
{
	switch(ftype) {
	case WINDRV_WRAP_FASTCALL:
		return(windrv_wrap_fastcall(func, wrap, argcnt));
	case WINDRV_WRAP_STDCALL:
		return(windrv_wrap_stdcall(func, wrap, argcnt));
	case WINDRV_WRAP_REGPARM:
		return(windrv_wrap_regparm(func, wrap));
	case WINDRV_WRAP_CDECL:
		return(windrv_wrap_stdcall(func, wrap, 0));
	default:
		break;
	}

	return(EINVAL);
}

static void
x86_oldldt(dummy)
	void			*dummy;
{
	struct thread		*t;
	struct x86desc		*gdt;
	struct gdt		gtable;
	uint16_t		ltable;

	mtx_lock_spin(&sched_lock);

	t = curthread;

	/* Grab location of existing GDT. */

	x86_getldt(&gtable, &ltable);

	/* Find the slot we updated. */

	gdt = gtable.base;
	gdt += FREEBSD_EMPTYSEL;

	/* Empty it out. */

	bzero((char *)gdt, sizeof(struct x86desc));

	/* Restore GDT. */

	x86_setldt(&gtable, ltable);

	mtx_unlock_spin(&sched_lock);

	return;
}

static void
x86_newldt(dummy)
	void			*dummy;
{
	struct gdt		gtable;
	uint16_t		ltable;
	struct x86desc		*l;
	struct thread		*t;

	mtx_lock_spin(&sched_lock);

	t = curthread;

	/* Grab location of existing GDT. */

	x86_getldt(&gtable, &ltable);

	/* Get pointer to the GDT table. */

	l = gtable.base;

	/* Get pointer to empty slot */

	l += FREEBSD_EMPTYSEL;

	/* Initialize TID for this CPU. */

	my_tids[t->td_oncpu].tid_selector = FREEBSD_EMPTYSEL;
	my_tids[t->td_oncpu].tid_self = &my_tids[t->td_oncpu];

	/* Set up new GDT entry. */

	l->x_lolimit = sizeof(struct tid);
	l->x_hilimit = SEGFLAGHI_GRAN|SEGFLAGHI_BIG;
	l->x_base0 = (vm_offset_t)(&my_tids[t->td_oncpu]) & 0xFFFF;
	l->x_base1 = ((vm_offset_t)(&my_tids[t->td_oncpu]) >> 16) & 0xFF;
	l->x_base2 = ((vm_offset_t)(&my_tids[t->td_oncpu]) >> 24) & 0xFF;
	l->x_flags = SEGFLAGLO_PRESENT|SEGFLAGLO_CD|SEGFLAGLO_WRITEABLE;

	/* Update the GDT. */

	x86_setldt(&gtable, ltable);

	mtx_unlock_spin(&sched_lock);

	/* Whew. */

	return;
}

#endif /* __i386__ */

int
windrv_unwrap(func)
	funcptr			func;
{
	free(func, M_DEVBUF);

	return(0);
}
