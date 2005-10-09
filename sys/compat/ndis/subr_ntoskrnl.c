/*-
 * Copyright (c) 2003
 *	Bill Paul <wpaul@windriver.com>.  All rights reserved.
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
 *	This product includes software developed by Bill Paul.
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

#include <sys/ctype.h>
#include <sys/unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <sys/callout.h>
#if __FreeBSD_version > 502113
#include <sys/kdb.h>
#endif
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/kthread.h>
#include <sys/module.h>
#include <sys/smp.h>
#include <sys/sched.h>

#include <machine/atomic.h>
#include <machine/clock.h>
#include <machine/bus.h>
#include <machine/stdarg.h>

#include <sys/bus.h>
#include <sys/rman.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/uma.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>

#include <compat/ndis/pe_var.h>
#include <compat/ndis/cfg_var.h>
#include <compat/ndis/resource_var.h>
#include <compat/ndis/ntoskrnl_var.h>
#include <compat/ndis/hal_var.h>
#include <compat/ndis/ndis_var.h>

struct kdpc_queue {
	list_entry		kq_high;
	list_entry		kq_low;
	list_entry		kq_med;
	struct thread		*kq_td;
	int			kq_state;
	int			kq_cpu;
	int			kq_exit;
	struct mtx		kq_lock;
	nt_kevent		kq_proc;
	nt_kevent		kq_done;
	nt_kevent		kq_dead;
};

typedef struct kdpc_queue kdpc_queue;

static uint8_t RtlEqualUnicodeString(ndis_unicode_string *,
	ndis_unicode_string *, uint8_t);
static void RtlCopyUnicodeString(ndis_unicode_string *,
	ndis_unicode_string *);
static ndis_status RtlUnicodeStringToAnsiString(ndis_ansi_string *,
	ndis_unicode_string *, uint8_t);
static ndis_status RtlAnsiStringToUnicodeString(ndis_unicode_string *,
	ndis_ansi_string *, uint8_t);
static irp *IoBuildSynchronousFsdRequest(uint32_t, device_object *,
	 void *, uint32_t, uint64_t *, nt_kevent *, io_status_block *);
static irp *IoBuildAsynchronousFsdRequest(uint32_t,
	device_object *, void *, uint32_t, uint64_t *, io_status_block *);
static irp *IoBuildDeviceIoControlRequest(uint32_t,
	device_object *, void *, uint32_t, void *, uint32_t,
	uint8_t, nt_kevent *, io_status_block *);
static irp *IoAllocateIrp(uint8_t, uint8_t);
static void IoReuseIrp(irp *, uint32_t);
static void IoFreeIrp(irp *);
static void IoInitializeIrp(irp *, uint16_t, uint8_t);
static irp *IoMakeAssociatedIrp(irp *, uint8_t);
static uint32_t KeWaitForMultipleObjects(uint32_t,
	nt_dispatch_header **, uint32_t, uint32_t, uint32_t, uint8_t,
	int64_t *, wait_block *);
static void ntoskrnl_wakeup(void *);
static void ntoskrnl_timercall(void *);
static void ntoskrnl_run_dpc(void *);
static void ntoskrnl_dpc_thread(void *);
static void ntoskrnl_destroy_dpc_threads(void);
static void ntoskrnl_destroy_workitem_threads(void);
static void ntoskrnl_workitem_thread(void *);
static void ntoskrnl_workitem(device_object *, void *);
static uint8_t ntoskrnl_insert_dpc(list_entry *, kdpc *);
static void WRITE_REGISTER_USHORT(uint16_t *, uint16_t);
static uint16_t READ_REGISTER_USHORT(uint16_t *);
static void WRITE_REGISTER_ULONG(uint32_t *, uint32_t);
static uint32_t READ_REGISTER_ULONG(uint32_t *);
static void WRITE_REGISTER_UCHAR(uint8_t *, uint8_t);
static uint8_t READ_REGISTER_UCHAR(uint8_t *);
static int64_t _allmul(int64_t, int64_t);
static int64_t _alldiv(int64_t, int64_t);
static int64_t _allrem(int64_t, int64_t);
static int64_t _allshr(int64_t, uint8_t);
static int64_t _allshl(int64_t, uint8_t);
static uint64_t _aullmul(uint64_t, uint64_t);
static uint64_t _aulldiv(uint64_t, uint64_t);
static uint64_t _aullrem(uint64_t, uint64_t);
static uint64_t _aullshr(uint64_t, uint8_t);
static uint64_t _aullshl(uint64_t, uint8_t);
static slist_entry *ntoskrnl_pushsl(slist_header *, slist_entry *);
static slist_entry *ntoskrnl_popsl(slist_header *);
static void ExInitializePagedLookasideList(paged_lookaside_list *,
	lookaside_alloc_func *, lookaside_free_func *,
	uint32_t, size_t, uint32_t, uint16_t);
static void ExDeletePagedLookasideList(paged_lookaside_list *);
static void ExInitializeNPagedLookasideList(npaged_lookaside_list *,
	lookaside_alloc_func *, lookaside_free_func *,
	uint32_t, size_t, uint32_t, uint16_t);
static void ExDeleteNPagedLookasideList(npaged_lookaside_list *);
static slist_entry
	*InterlockedPushEntrySList(slist_header *, slist_entry *);
static slist_entry *InterlockedPopEntrySList(slist_header *);
static slist_entry
	*ExInterlockedPushEntrySList(slist_header *,
	slist_entry *, kspin_lock *);
static slist_entry
	*ExInterlockedPopEntrySList(slist_header *, kspin_lock *);
static uint16_t ExQueryDepthSList(slist_header *);
static uint32_t InterlockedIncrement(volatile uint32_t *);
static uint32_t InterlockedDecrement(volatile uint32_t *);
static void ExInterlockedAddLargeStatistic(uint64_t *, uint32_t);
static uint32_t MmSizeOfMdl(void *, size_t);
static void MmBuildMdlForNonPagedPool(mdl *);
static void *MmMapLockedPages(mdl *, uint8_t);
static void *MmMapLockedPagesSpecifyCache(mdl *,
	uint8_t, uint32_t, void *, uint32_t, uint32_t);
static void MmUnmapLockedPages(void *, mdl *);
static uint8_t MmIsAddressValid(void *);
static size_t RtlCompareMemory(const void *, const void *, size_t);
static void RtlInitAnsiString(ndis_ansi_string *, char *);
static void RtlInitUnicodeString(ndis_unicode_string *,
	uint16_t *);
static void RtlFreeUnicodeString(ndis_unicode_string *);
static void RtlFreeAnsiString(ndis_ansi_string *);
static ndis_status RtlUnicodeStringToInteger(ndis_unicode_string *,
	uint32_t, uint32_t *);
static int atoi (const char *);
static long atol (const char *);
static int rand(void);
static void srand(unsigned int);
static void ntoskrnl_time(uint64_t *);
static uint8_t IoIsWdmVersionAvailable(uint8_t, uint8_t);
static void ntoskrnl_thrfunc(void *);
static ndis_status PsCreateSystemThread(ndis_handle *,
	uint32_t, void *, ndis_handle, void *, void *, void *);
static ndis_status PsTerminateSystemThread(ndis_status);
static ndis_status IoGetDeviceProperty(device_object *, uint32_t,
	uint32_t, void *, uint32_t *);
static void KeInitializeMutex(kmutant *, uint32_t);
static uint32_t KeReleaseMutex(kmutant *, uint8_t);
static uint32_t KeReadStateMutex(kmutant *);
static ndis_status ObReferenceObjectByHandle(ndis_handle,
	uint32_t, void *, uint8_t, void **, void **);
static void ObfDereferenceObject(void *);
static uint32_t ZwClose(ndis_handle);
static void *ntoskrnl_memset(void *, int, size_t);
static char *ntoskrnl_strstr(char *, char *);
static funcptr ntoskrnl_findwrap(funcptr);
static uint32_t DbgPrint(char *, ...);
static void DbgBreakPoint(void);
static void dummy(void);

static struct mtx ntoskrnl_dispatchlock;
static kspin_lock ntoskrnl_global;
static kspin_lock ntoskrnl_cancellock;
static int ntoskrnl_kth = 0;
static struct nt_objref_head ntoskrnl_reflist;
static uma_zone_t mdl_zone;
static uma_zone_t iw_zone;
static struct kdpc_queue *kq_queues;
static struct kdpc_queue *wq_queues;
static int wq_idx = 0;

int
ntoskrnl_libinit()
{
	image_patch_table	*patch;
	int			error;
	struct proc		*p;
	kdpc_queue		*kq;
	int			i;
	char			name[64];

	mtx_init(&ntoskrnl_dispatchlock,
	    "ntoskrnl dispatch lock", MTX_NDIS_LOCK, MTX_DEF|MTX_RECURSE);
	KeInitializeSpinLock(&ntoskrnl_global);
	KeInitializeSpinLock(&ntoskrnl_cancellock);
	TAILQ_INIT(&ntoskrnl_reflist);

	kq_queues = ExAllocatePoolWithTag(NonPagedPool,
	    sizeof(kdpc_queue) * mp_ncpus, 0);

	if (kq_queues == NULL)
		return(ENOMEM);

	wq_queues = ExAllocatePoolWithTag(NonPagedPool,
	    sizeof(kdpc_queue) * WORKITEM_THREADS, 0);

	if (wq_queues == NULL)
		return(ENOMEM);

	bzero((char *)kq_queues, sizeof(kdpc_queue) * mp_ncpus);
	bzero((char *)wq_queues, sizeof(kdpc_queue) * WORKITEM_THREADS);

	/*
	 * Launch the DPC threads.
	 */

        for (i = 0; i < mp_ncpus; i++) {
		kq = kq_queues + i;
		kq->kq_cpu = i;
		sprintf(name, "Windows DPC %d", i);
		error = kthread_create(ntoskrnl_dpc_thread, kq, &p,
		    RFHIGHPID, NDIS_KSTACK_PAGES, name);
		if (error)
			panic("failed to launch DPC thread");
        }

	/*
	 * Launch the workitem threads.
	 */

        for (i = 0; i < WORKITEM_THREADS; i++) {
		kq = wq_queues + i;
		sprintf(name, "Windows Workitem %d", i);
		error = kthread_create(ntoskrnl_workitem_thread, kq, &p,
                    RFHIGHPID, NDIS_KSTACK_PAGES, name);
		if (error)
			panic("failed to launch workitem thread");
	}

	patch = ntoskrnl_functbl;
	while (patch->ipt_func != NULL) {
		windrv_wrap((funcptr)patch->ipt_func,
		    (funcptr *)&patch->ipt_wrap,
		    patch->ipt_argcnt, patch->ipt_ftype);
		patch++;
	}

	/*
	 * MDLs are supposed to be variable size (they describe
	 * buffers containing some number of pages, but we don't
	 * know ahead of time how many pages that will be). But
	 * always allocating them off the heap is very slow. As
	 * a compromise, we create an MDL UMA zone big enough to
	 * handle any buffer requiring up to 16 pages, and we
	 * use those for any MDLs for buffers of 16 pages or less
	 * in size. For buffers larger than that (which we assume
	 * will be few and far between, we allocate the MDLs off
	 * the heap.
	 */

	mdl_zone = uma_zcreate("Windows MDL", MDL_ZONE_SIZE,
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);

	iw_zone = uma_zcreate("Windows WorkItem", sizeof(io_workitem),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);

	return(0);
}

int
ntoskrnl_libfini()
{
	image_patch_table	*patch;

	patch = ntoskrnl_functbl;
	while (patch->ipt_func != NULL) {
		windrv_unwrap(patch->ipt_wrap);
		patch++;
	}

	/* Stop the DPC queues. */
	ntoskrnl_destroy_dpc_threads();
	/* Stop the workitem queues. */
	ntoskrnl_destroy_workitem_threads();

	ExFreePool(kq_queues);
	ExFreePool(wq_queues);

	uma_zdestroy(mdl_zone);
	uma_zdestroy(iw_zone);

	mtx_destroy(&ntoskrnl_dispatchlock);

	return(0);
}

/*
 * We need to be able to reference this externally from the wrapper;
 * GCC only generates a local implementation of memset.
 */
static void *
ntoskrnl_memset(buf, ch, size)
	void			*buf;
	int			ch;
	size_t			size;
{
	return(memset(buf, ch, size));
}

static char *
ntoskrnl_strstr(s, find)
	char *s, *find;
{
	char c, sc;
	size_t len;

	if ((c = *find++) != 0) {
		len = strlen(find);
		do {
			do {
				if ((sc = *s++) == 0)
					return (NULL);
			} while (sc != c);
		} while (strncmp(s, find, len) != 0);
		s--;
	}
	return ((char *)s);
}


static uint8_t 
RtlEqualUnicodeString(str1, str2, caseinsensitive)
	ndis_unicode_string	*str1;
	ndis_unicode_string	*str2;
	uint8_t			caseinsensitive;
{
	int			i;

	if (str1->us_len != str2->us_len)
		return(FALSE);

	for (i = 0; i < str1->us_len; i++) {
		if (caseinsensitive == TRUE) {
			if (toupper((char)(str1->us_buf[i] & 0xFF)) !=
			    toupper((char)(str2->us_buf[i] & 0xFF)))
				return(FALSE);
		} else {
			if (str1->us_buf[i] != str2->us_buf[i])
				return(FALSE);
		}
	}

	return(TRUE);
}

static void
RtlCopyUnicodeString(dest, src)
	ndis_unicode_string	*dest;
	ndis_unicode_string	*src;
{

	if (dest->us_maxlen >= src->us_len)
		dest->us_len = src->us_len;
	else
		dest->us_len = dest->us_maxlen;
	memcpy(dest->us_buf, src->us_buf, dest->us_len);
	return;
}

static ndis_status
RtlUnicodeStringToAnsiString(dest, src, allocate)
	ndis_ansi_string	*dest;
	ndis_unicode_string	*src;
	uint8_t			allocate;
{
	char			*astr = NULL;

	if (dest == NULL || src == NULL)
		return(NDIS_STATUS_FAILURE);

	if (allocate == TRUE) {
		if (ndis_unicode_to_ascii(src->us_buf, src->us_len, &astr))
			return(NDIS_STATUS_FAILURE);
		dest->nas_buf = astr;
		dest->nas_len = dest->nas_maxlen = strlen(astr);
	} else {
		dest->nas_len = src->us_len / 2; /* XXX */
		if (dest->nas_maxlen < dest->nas_len)
			dest->nas_len = dest->nas_maxlen;
		ndis_unicode_to_ascii(src->us_buf, dest->nas_len * 2,
		    &dest->nas_buf);
	}
	return (NDIS_STATUS_SUCCESS);
}

static ndis_status
RtlAnsiStringToUnicodeString(dest, src, allocate)
	ndis_unicode_string	*dest;
	ndis_ansi_string	*src;
	uint8_t			allocate;
{
	uint16_t		*ustr = NULL;

	if (dest == NULL || src == NULL)
		return(NDIS_STATUS_FAILURE);

	if (allocate == TRUE) {
		if (ndis_ascii_to_unicode(src->nas_buf, &ustr))
			return(NDIS_STATUS_FAILURE);
		dest->us_buf = ustr;
		dest->us_len = dest->us_maxlen = strlen(src->nas_buf) * 2;
	} else {
		dest->us_len = src->nas_len * 2; /* XXX */
		if (dest->us_maxlen < dest->us_len)
			dest->us_len = dest->us_maxlen;
		ndis_ascii_to_unicode(src->nas_buf, &dest->us_buf);
	}
	return (NDIS_STATUS_SUCCESS);
}

void *
ExAllocatePoolWithTag(pooltype, len, tag)
	uint32_t		pooltype;
	size_t			len;
	uint32_t		tag;
{
	void			*buf;

	buf = malloc(len, M_DEVBUF, M_NOWAIT);
	if (buf == NULL)
		return(NULL);
	return(buf);
}

void
ExFreePool(buf)
	void			*buf;
{
	free(buf, M_DEVBUF);
	return;
}

uint32_t
IoAllocateDriverObjectExtension(drv, clid, extlen, ext)
	driver_object		*drv;
	void			*clid;
	uint32_t		extlen;
	void			**ext;
{
	custom_extension	*ce;

	ce = ExAllocatePoolWithTag(NonPagedPool, sizeof(custom_extension)
	    + extlen, 0);

	if (ce == NULL)
		return(STATUS_INSUFFICIENT_RESOURCES);

	ce->ce_clid = clid;
	INSERT_LIST_TAIL((&drv->dro_driverext->dre_usrext), (&ce->ce_list));

	*ext = (void *)(ce + 1);

	return(STATUS_SUCCESS);
}

void *
IoGetDriverObjectExtension(drv, clid)
	driver_object		*drv;
	void			*clid;
{
	list_entry		*e;
	custom_extension	*ce;

	/*
	 * Sanity check. Our dummy bus drivers don't have
	 * any driver extentions.
	 */

	if (drv->dro_driverext == NULL)
		return(NULL);

	e = drv->dro_driverext->dre_usrext.nle_flink;
	while (e != &drv->dro_driverext->dre_usrext) {
		ce = (custom_extension *)e;
		if (ce->ce_clid == clid)
			return((void *)(ce + 1));
		e = e->nle_flink;
	}

	return(NULL);
}


uint32_t
IoCreateDevice(drv, devextlen, devname, devtype, devchars, exclusive, newdev)
	driver_object		*drv;
	uint32_t		devextlen;
	unicode_string		*devname;
	uint32_t		devtype;
	uint32_t		devchars;
	uint8_t			exclusive;
	device_object		**newdev;
{
	device_object		*dev;

	dev = ExAllocatePoolWithTag(NonPagedPool, sizeof(device_object), 0);
	if (dev == NULL)
		return(STATUS_INSUFFICIENT_RESOURCES);

	dev->do_type = devtype;
	dev->do_drvobj = drv;
	dev->do_currirp = NULL;
	dev->do_flags = 0;

	if (devextlen) {
		dev->do_devext = ExAllocatePoolWithTag(NonPagedPool,
		    devextlen, 0);

		if (dev->do_devext == NULL) {
			ExFreePool(dev);
			return(STATUS_INSUFFICIENT_RESOURCES);
		}

		bzero(dev->do_devext, devextlen);
	} else
		dev->do_devext = NULL;

	dev->do_size = sizeof(device_object) + devextlen;
	dev->do_refcnt = 1;
	dev->do_attacheddev = NULL;
	dev->do_nextdev = NULL;
	dev->do_devtype = devtype;
	dev->do_stacksize = 1;
	dev->do_alignreq = 1;
	dev->do_characteristics = devchars;
	dev->do_iotimer = NULL;
	KeInitializeEvent(&dev->do_devlock, EVENT_TYPE_SYNC, TRUE);

	/*
	 * Vpd is used for disk/tape devices,
	 * but we don't support those. (Yet.)
	 */
	dev->do_vpb = NULL;

	dev->do_devobj_ext = ExAllocatePoolWithTag(NonPagedPool,
	    sizeof(devobj_extension), 0);

	if (dev->do_devobj_ext == NULL) {
		if (dev->do_devext != NULL)
			ExFreePool(dev->do_devext);
		ExFreePool(dev);
		return(STATUS_INSUFFICIENT_RESOURCES);
	}

	dev->do_devobj_ext->dve_type = 0;
	dev->do_devobj_ext->dve_size = sizeof(devobj_extension);
	dev->do_devobj_ext->dve_devobj = dev;

	/*
	 * Attach this device to the driver object's list
	 * of devices. Note: this is not the same as attaching
	 * the device to the device stack. The driver's AddDevice
	 * routine must explicitly call IoAddDeviceToDeviceStack()
	 * to do that.
	 */

	if (drv->dro_devobj == NULL) {
		drv->dro_devobj = dev;
		dev->do_nextdev = NULL;
	} else {
		dev->do_nextdev = drv->dro_devobj;
		drv->dro_devobj = dev;
	}

	*newdev = dev;

	return(STATUS_SUCCESS);
}

void
IoDeleteDevice(dev)
	device_object		*dev;
{
	device_object		*prev;

	if (dev == NULL)
		return;

	if (dev->do_devobj_ext != NULL)
		ExFreePool(dev->do_devobj_ext);

	if (dev->do_devext != NULL)
		ExFreePool(dev->do_devext);

	/* Unlink the device from the driver's device list. */

	prev = dev->do_drvobj->dro_devobj;
	if (prev == dev)
		dev->do_drvobj->dro_devobj = dev->do_nextdev;
	else {
		while (prev->do_nextdev != dev)
			prev = prev->do_nextdev;
		prev->do_nextdev = dev->do_nextdev;
	}

	ExFreePool(dev);

	return;
}

device_object *
IoGetAttachedDevice(dev)
	device_object		*dev;
{
	device_object		*d;

	if (dev == NULL)
		return (NULL);

	d = dev;

	while (d->do_attacheddev != NULL)
		d = d->do_attacheddev;

	return (d);
}

static irp *
IoBuildSynchronousFsdRequest(func, dobj, buf, len, off, event, status)
	uint32_t		func;
	device_object		*dobj;
	void			*buf;
	uint32_t		len;
	uint64_t		*off;
	nt_kevent		*event;
	io_status_block		*status;
{
	irp			*ip;

	ip = IoBuildAsynchronousFsdRequest(func, dobj, buf, len, off, status);
	if (ip == NULL)
		return(NULL);
	ip->irp_usrevent = event;

	return(ip);
}

static irp *
IoBuildAsynchronousFsdRequest(func, dobj, buf, len, off, status)
	uint32_t		func;
	device_object		*dobj;
	void			*buf;
	uint32_t		len;
	uint64_t		*off;
	io_status_block		*status;
{
	irp			*ip;
	io_stack_location	*sl;

	ip = IoAllocateIrp(dobj->do_stacksize, TRUE);
	if (ip == NULL)
		return(NULL);

	ip->irp_usriostat = status;
	ip->irp_tail.irp_overlay.irp_thread = NULL;

	sl = IoGetNextIrpStackLocation(ip);
	sl->isl_major = func;
	sl->isl_minor = 0;
	sl->isl_flags = 0;
	sl->isl_ctl = 0;
	sl->isl_devobj = dobj;
	sl->isl_fileobj = NULL;
	sl->isl_completionfunc = NULL;

	ip->irp_userbuf = buf;

	if (dobj->do_flags & DO_BUFFERED_IO) {
		ip->irp_assoc.irp_sysbuf =
		    ExAllocatePoolWithTag(NonPagedPool, len, 0);
		if (ip->irp_assoc.irp_sysbuf == NULL) {
			IoFreeIrp(ip);
			return(NULL);
		}
		bcopy(buf, ip->irp_assoc.irp_sysbuf, len);
	}

	if (dobj->do_flags & DO_DIRECT_IO) {
		ip->irp_mdl = IoAllocateMdl(buf, len, FALSE, FALSE, ip);
		if (ip->irp_mdl == NULL) {
			if (ip->irp_assoc.irp_sysbuf != NULL)
				ExFreePool(ip->irp_assoc.irp_sysbuf);
			IoFreeIrp(ip);
			return(NULL);
		}
		ip->irp_userbuf = NULL;
		ip->irp_assoc.irp_sysbuf = NULL;
	}

	if (func == IRP_MJ_READ) {
		sl->isl_parameters.isl_read.isl_len = len;
		if (off != NULL)
			sl->isl_parameters.isl_read.isl_byteoff = *off;
		else
			sl->isl_parameters.isl_read.isl_byteoff = 0;
	}

	if (func == IRP_MJ_WRITE) {
		sl->isl_parameters.isl_write.isl_len = len;
		if (off != NULL)
			sl->isl_parameters.isl_write.isl_byteoff = *off;
		else
			sl->isl_parameters.isl_write.isl_byteoff = 0;
	}	

	return(ip);
}

static irp *
IoBuildDeviceIoControlRequest(iocode, dobj, ibuf, ilen, obuf, olen,
    isinternal, event, status)
	uint32_t		iocode;
	device_object		*dobj;
	void			*ibuf;
	uint32_t		ilen;
	void			*obuf;
	uint32_t		olen;
	uint8_t			isinternal;
	nt_kevent		*event;
	io_status_block		*status;
{
	irp			*ip;
	io_stack_location	*sl;
	uint32_t		buflen;

	ip = IoAllocateIrp(dobj->do_stacksize, TRUE);
	if (ip == NULL)
		return(NULL);
	ip->irp_usrevent = event;
	ip->irp_usriostat = status;
	ip->irp_tail.irp_overlay.irp_thread = NULL;

	sl = IoGetNextIrpStackLocation(ip);
	sl->isl_major = isinternal == TRUE ?
	    IRP_MJ_INTERNAL_DEVICE_CONTROL : IRP_MJ_DEVICE_CONTROL;
	sl->isl_minor = 0;
	sl->isl_flags = 0;
	sl->isl_ctl = 0;
	sl->isl_devobj = dobj;
	sl->isl_fileobj = NULL;
	sl->isl_completionfunc = NULL;
	sl->isl_parameters.isl_ioctl.isl_iocode = iocode;
	sl->isl_parameters.isl_ioctl.isl_ibuflen = ilen;
	sl->isl_parameters.isl_ioctl.isl_obuflen = olen;

	switch(IO_METHOD(iocode)) {
	case METHOD_BUFFERED:
		if (ilen > olen)
			buflen = ilen;
		else
			buflen = olen;
		if (buflen) {
			ip->irp_assoc.irp_sysbuf =
			    ExAllocatePoolWithTag(NonPagedPool, buflen, 0);
			if (ip->irp_assoc.irp_sysbuf == NULL) {
				IoFreeIrp(ip);
				return(NULL);
			}
		}
		if (ilen && ibuf != NULL) {
			bcopy(ibuf, ip->irp_assoc.irp_sysbuf, ilen);
			bzero((char *)ip->irp_assoc.irp_sysbuf + ilen,
			    buflen - ilen);
		} else
			bzero(ip->irp_assoc.irp_sysbuf, ilen);
		ip->irp_userbuf = obuf;
		break;
	case METHOD_IN_DIRECT:
	case METHOD_OUT_DIRECT:
		if (ilen && ibuf != NULL) {
			ip->irp_assoc.irp_sysbuf =
			    ExAllocatePoolWithTag(NonPagedPool, ilen, 0);
			if (ip->irp_assoc.irp_sysbuf == NULL) {
				IoFreeIrp(ip);
				return(NULL);
			}
			bcopy(ibuf, ip->irp_assoc.irp_sysbuf, ilen);
		}
		if (olen && obuf != NULL) {
			ip->irp_mdl = IoAllocateMdl(obuf, olen,
			    FALSE, FALSE, ip);
			/*
			 * Normally we would MmProbeAndLockPages()
			 * here, but we don't have to in our
			 * imlementation.
			 */
		}
		break;
	case METHOD_NEITHER:
		ip->irp_userbuf = obuf;
		sl->isl_parameters.isl_ioctl.isl_type3ibuf = ibuf;
		break;
	default:
		break;
	}

	/*
	 * Ideally, we should associate this IRP with the calling
	 * thread here.
	 */

	return (ip);
}

static irp *
IoAllocateIrp(stsize, chargequota)
	uint8_t			stsize;
	uint8_t			chargequota;
{
	irp			*i;

	i = ExAllocatePoolWithTag(NonPagedPool, IoSizeOfIrp(stsize), 0);
	if (i == NULL)
		return (NULL);

	IoInitializeIrp(i, IoSizeOfIrp(stsize), stsize);

	return (i);
}

static irp *
IoMakeAssociatedIrp(ip, stsize)
	irp			*ip;
	uint8_t			stsize;
{
	irp			*associrp;

	associrp = IoAllocateIrp(stsize, FALSE);
	if (associrp == NULL)
		return(NULL);

	mtx_lock(&ntoskrnl_dispatchlock);
	associrp->irp_flags |= IRP_ASSOCIATED_IRP;
	associrp->irp_tail.irp_overlay.irp_thread =
	    ip->irp_tail.irp_overlay.irp_thread;
	associrp->irp_assoc.irp_master = ip;
	mtx_unlock(&ntoskrnl_dispatchlock);

	return(associrp);
}

static void
IoFreeIrp(ip)
	irp			*ip;
{
	ExFreePool(ip);
	return;
}

static void
IoInitializeIrp(io, psize, ssize)
	irp			*io;
	uint16_t		psize;
	uint8_t			ssize;
{
	bzero((char *)io, IoSizeOfIrp(ssize));
	io->irp_size = psize;
	io->irp_stackcnt = ssize;
	io->irp_currentstackloc = ssize;
	INIT_LIST_HEAD(&io->irp_thlist);
	io->irp_tail.irp_overlay.irp_csl =
	    (io_stack_location *)(io + 1) + ssize;

	return;
}

static void
IoReuseIrp(ip, status)
	irp			*ip;
	uint32_t		status;
{
	uint8_t			allocflags;

	allocflags = ip->irp_allocflags;
	IoInitializeIrp(ip, ip->irp_size, ip->irp_stackcnt);
	ip->irp_iostat.isb_status = status;
	ip->irp_allocflags = allocflags;

	return;
}

void
IoAcquireCancelSpinLock(irql)
	uint8_t			*irql;
{
	KeAcquireSpinLock(&ntoskrnl_cancellock, irql);
	return;
}

void
IoReleaseCancelSpinLock(irql)
	uint8_t			irql;
{
	KeReleaseSpinLock(&ntoskrnl_cancellock, irql);
	return;
}

uint8_t
IoCancelIrp(irp *ip)
{
	cancel_func		cfunc;

	IoAcquireCancelSpinLock(&ip->irp_cancelirql);
	cfunc = IoSetCancelRoutine(ip, NULL);
	ip->irp_cancel = TRUE;
	if (ip->irp_cancelfunc == NULL) {
		IoReleaseCancelSpinLock(ip->irp_cancelirql);
		return(FALSE);
	}
	MSCALL2(cfunc, IoGetCurrentIrpStackLocation(ip)->isl_devobj, ip);
	return(TRUE);
}

uint32_t
IofCallDriver(dobj, ip)
	device_object		*dobj;
	irp			*ip;
{
	driver_object		*drvobj;
	io_stack_location	*sl;
	uint32_t		status;
	driver_dispatch		disp;

	drvobj = dobj->do_drvobj;

	if (ip->irp_currentstackloc <= 0)
		panic("IoCallDriver(): out of stack locations");

	IoSetNextIrpStackLocation(ip);
	sl = IoGetCurrentIrpStackLocation(ip);

	sl->isl_devobj = dobj;

	disp = drvobj->dro_dispatch[sl->isl_major];
	status = MSCALL2(disp, dobj, ip);

	return(status);
}

void
IofCompleteRequest(ip, prioboost)
	irp			*ip;
	uint8_t			prioboost;
{
	uint32_t		i;
	uint32_t		status;
	device_object		*dobj;
	io_stack_location	*sl;
	completion_func		cf;

	ip->irp_pendingreturned =
	    IoGetCurrentIrpStackLocation(ip)->isl_ctl & SL_PENDING_RETURNED;
	sl = (io_stack_location *)(ip + 1);

	for (i = ip->irp_currentstackloc; i < (uint32_t)ip->irp_stackcnt; i++) {
		if (ip->irp_currentstackloc < ip->irp_stackcnt - 1) {
			IoSkipCurrentIrpStackLocation(ip);
			dobj = IoGetCurrentIrpStackLocation(ip)->isl_devobj;
		} else
			dobj = NULL;

		if (sl[i].isl_completionfunc != NULL &&
		    ((ip->irp_iostat.isb_status == STATUS_SUCCESS &&
		    sl->isl_ctl & SL_INVOKE_ON_SUCCESS) ||
		    (ip->irp_iostat.isb_status != STATUS_SUCCESS &&
		    sl->isl_ctl & SL_INVOKE_ON_ERROR) ||
		    (ip->irp_cancel == TRUE &&
		    sl->isl_ctl & SL_INVOKE_ON_CANCEL))) {
			cf = sl->isl_completionfunc;
			status = MSCALL3(cf, dobj, ip, sl->isl_completionctx);
			if (status == STATUS_MORE_PROCESSING_REQUIRED)
				return;
		}

		if (IoGetCurrentIrpStackLocation(ip)->isl_ctl &
		    SL_PENDING_RETURNED)
			ip->irp_pendingreturned = TRUE;
	}

	/* Handle any associated IRPs. */

	if (ip->irp_flags & IRP_ASSOCIATED_IRP) {
		uint32_t		masterirpcnt;
		irp			*masterirp;
		mdl			*m;

		masterirp = ip->irp_assoc.irp_master;
		masterirpcnt =
		    InterlockedDecrement(&masterirp->irp_assoc.irp_irpcnt);

		while ((m = ip->irp_mdl) != NULL) {
			ip->irp_mdl = m->mdl_next;
			IoFreeMdl(m);
		}
		IoFreeIrp(ip);
		if (masterirpcnt == 0)
			IoCompleteRequest(masterirp, IO_NO_INCREMENT);
		return;
	}

	/* With any luck, these conditions will never arise. */

	if (ip->irp_flags & (IRP_PAGING_IO|IRP_CLOSE_OPERATION)) {
		if (ip->irp_usriostat != NULL)
			*ip->irp_usriostat = ip->irp_iostat;
		if (ip->irp_usrevent != NULL)
			KeSetEvent(ip->irp_usrevent, prioboost, FALSE);
		if (ip->irp_flags & IRP_PAGING_IO) {
			if (ip->irp_mdl != NULL)
				IoFreeMdl(ip->irp_mdl);
			IoFreeIrp(ip);
		}
	}

	return;
}

device_object *
IoAttachDeviceToDeviceStack(src, dst)
	device_object		*src;
	device_object		*dst;
{
	device_object		*attached;

	mtx_lock(&ntoskrnl_dispatchlock);
	attached = IoGetAttachedDevice(dst);
	attached->do_attacheddev = src;
	src->do_attacheddev = NULL;
	src->do_stacksize = attached->do_stacksize + 1;
	mtx_unlock(&ntoskrnl_dispatchlock);

	return(attached);
}

void
IoDetachDevice(topdev)
	device_object		*topdev;
{
	device_object		*tail;

	mtx_lock(&ntoskrnl_dispatchlock);

	/* First, break the chain. */
	tail = topdev->do_attacheddev;
	if (tail == NULL) {
		mtx_unlock(&ntoskrnl_dispatchlock);
		return;
	}
	topdev->do_attacheddev = tail->do_attacheddev;
	topdev->do_refcnt--;

	/* Now reduce the stacksize count for the tail objects. */

	tail = topdev->do_attacheddev;
	while (tail != NULL) {
		tail->do_stacksize--;
		tail = tail->do_attacheddev;
	}

	mtx_unlock(&ntoskrnl_dispatchlock);

	return;
}

/* Always called with dispatcher lock held. */
static void
ntoskrnl_wakeup(arg)
	void			*arg;
{
	nt_dispatch_header	*obj;
	wait_block		*w;
	list_entry		*e;
	struct thread		*td;

	obj = arg;

	e = obj->dh_waitlisthead.nle_flink;

	obj->dh_sigstate = TRUE;

	/*
	 * What happens if someone tells us to wake up
	 * threads waiting on an object, but nobody's
	 * waiting on it at the moment? For sync events,
	 * the signal state is supposed to be automatically
	 * reset, but this only happens in the KeWaitXXX()
	 * functions. If nobody is waiting, the state never
	 * gets cleared.
	 */

	if (e == &obj->dh_waitlisthead) {
		if (obj->dh_type == EVENT_TYPE_SYNC)
			obj->dh_sigstate = FALSE;
		return;
	}

	while (e != &obj->dh_waitlisthead) {
		w = (wait_block *)e;
		td = w->wb_kthread;
		ndis_thresume(td->td_proc);
		/*
		 * For synchronization objects, only wake up
		 * the first waiter.
		 */
		if (obj->dh_type == EVENT_TYPE_SYNC)
			break;
		e = e->nle_flink;
	}

	return;
}

static void 
ntoskrnl_time(tval)
	uint64_t                *tval;
{
	struct timespec		ts;

	nanotime(&ts);
	*tval = (uint64_t)ts.tv_nsec / 100 + (uint64_t)ts.tv_sec * 10000000 +
	    11644473600;

	return;
}

/*
 * KeWaitForSingleObject() is a tricky beast, because it can be used
 * with several different object types: semaphores, timers, events,
 * mutexes and threads. Semaphores don't appear very often, but the
 * other object types are quite common. KeWaitForSingleObject() is
 * what's normally used to acquire a mutex, and it can be used to
 * wait for a thread termination.
 *
 * The Windows NDIS API is implemented in terms of Windows kernel
 * primitives, and some of the object manipulation is duplicated in
 * NDIS. For example, NDIS has timers and events, which are actually
 * Windows kevents and ktimers. Now, you're supposed to only use the
 * NDIS variants of these objects within the confines of the NDIS API,
 * but there are some naughty developers out there who will use
 * KeWaitForSingleObject() on NDIS timer and event objects, so we
 * have to support that as well. Conseqently, our NDIS timer and event
 * code has to be closely tied into our ntoskrnl timer and event code,
 * just as it is in Windows.
 *
 * KeWaitForSingleObject() may do different things for different kinds
 * of objects:
 *
 * - For events, we check if the event has been signalled. If the
 *   event is already in the signalled state, we just return immediately,
 *   otherwise we wait for it to be set to the signalled state by someone
 *   else calling KeSetEvent(). Events can be either synchronization or
 *   notification events.
 *
 * - For timers, if the timer has already fired and the timer is in
 *   the signalled state, we just return, otherwise we wait on the
 *   timer. Unlike an event, timers get signalled automatically when
 *   they expire rather than someone having to trip them manually.
 *   Timers initialized with KeInitializeTimer() are always notification
 *   events: KeInitializeTimerEx() lets you initialize a timer as
 *   either a notification or synchronization event.
 *
 * - For mutexes, we try to acquire the mutex and if we can't, we wait
 *   on the mutex until it's available and then grab it. When a mutex is
 *   released, it enters the signaled state, which wakes up one of the
 *   threads waiting to acquire it. Mutexes are always synchronization
 *   events.
 *
 * - For threads, the only thing we do is wait until the thread object
 *   enters a signalled state, which occurs when the thread terminates.
 *   Threads are always notification events.
 *
 * A notification event wakes up all threads waiting on an object. A
 * synchronization event wakes up just one. Also, a synchronization event
 * is auto-clearing, which means we automatically set the event back to
 * the non-signalled state once the wakeup is done.
 */

uint32_t
KeWaitForSingleObject(obj, reason, mode, alertable, duetime)
	nt_dispatch_header	*obj;
	uint32_t		reason;
	uint32_t		mode;
	uint8_t			alertable;
	int64_t			*duetime;
{
	struct thread		*td = curthread;
	kmutant			*km;
	wait_block		w;
	struct timeval		tv;
	int			error = 0;
	uint64_t		curtime;

	if (obj == NULL)
		return(STATUS_INVALID_PARAMETER);

	mtx_lock(&ntoskrnl_dispatchlock);

	/*
	 * See if the object is a mutex. If so, and we already own
	 * it, then just increment the acquisition count and return.
         *
         * For any other kind of object, see if it's already in the
	 * signalled state, and if it is, just return. If the object
         * is marked as a synchronization event, reset the state to
         * unsignalled.
	 */

	if (obj->dh_size == OTYPE_MUTEX) {
		km = (kmutant *)obj;
		if (km->km_ownerthread == NULL ||
		    km->km_ownerthread == curthread->td_proc) {
			obj->dh_sigstate = FALSE;
			km->km_acquirecnt++;
			km->km_ownerthread = curthread->td_proc;
			mtx_unlock(&ntoskrnl_dispatchlock);
			return (STATUS_SUCCESS);
		}
	} else if (obj->dh_sigstate == TRUE) {
		if (obj->dh_type == EVENT_TYPE_SYNC)
			obj->dh_sigstate = FALSE;
		mtx_unlock(&ntoskrnl_dispatchlock);
		return (STATUS_SUCCESS);
	}

	w.wb_object = obj;
	w.wb_kthread = td;

	INSERT_LIST_TAIL((&obj->dh_waitlisthead), (&w.wb_waitlist));

	/*
	 * The timeout value is specified in 100 nanosecond units
	 * and can be a positive or negative number. If it's positive,
	 * then the duetime is absolute, and we need to convert it
	 * to an absolute offset relative to now in order to use it.
	 * If it's negative, then the duetime is relative and we
	 * just have to convert the units.
	 */

	if (duetime != NULL) {
		if (*duetime < 0) {
			tv.tv_sec = - (*duetime) / 10000000;
			tv.tv_usec = (- (*duetime) / 10) -
			    (tv.tv_sec * 1000000);
		} else {
			ntoskrnl_time(&curtime);
			if (*duetime < curtime)
				tv.tv_sec = tv.tv_usec = 0;
			else {
				tv.tv_sec = ((*duetime) - curtime) / 10000000;
				tv.tv_usec = ((*duetime) - curtime) / 10 -
				    (tv.tv_sec * 1000000);
			}
		}
	}

	error = ndis_thsuspend(td->td_proc, &ntoskrnl_dispatchlock,
	    duetime == NULL ? 0 : tvtohz(&tv));

	/* We timed out. Leave the object alone and return status. */

	if (error == EWOULDBLOCK) {
		REMOVE_LIST_ENTRY((&w.wb_waitlist));
		INIT_LIST_HEAD((&w.wb_waitlist));
		mtx_unlock(&ntoskrnl_dispatchlock);
		return(STATUS_TIMEOUT);
	}

	/*
	 * Mutexes are always synchronization objects, which means
         * if several threads are waiting to acquire it, only one will
         * be woken up. If that one is us, and the mutex is up for grabs,
         * grab it.
	 */

	if (obj->dh_size == OTYPE_MUTEX) {
		km = (kmutant *)obj;
		if (km->km_ownerthread == NULL) {
			km->km_ownerthread = curthread->td_proc;
			km->km_acquirecnt++;
		}
	}

	if (obj->dh_type == EVENT_TYPE_SYNC)
		obj->dh_sigstate = FALSE;
	REMOVE_LIST_ENTRY((&w.wb_waitlist));
	INIT_LIST_HEAD((&w.wb_waitlist));

	mtx_unlock(&ntoskrnl_dispatchlock);

	return(STATUS_SUCCESS);
}

static uint32_t
KeWaitForMultipleObjects(cnt, obj, wtype, reason, mode,
	alertable, duetime, wb_array)
	uint32_t		cnt;
	nt_dispatch_header	*obj[];
	uint32_t		wtype;
	uint32_t		reason;
	uint32_t		mode;
	uint8_t			alertable;
	int64_t			*duetime;
	wait_block		*wb_array;
{
	struct thread		*td = curthread;
	kmutant			*km;
	wait_block		_wb_array[THREAD_WAIT_OBJECTS];
	wait_block		*w;
	struct timeval		tv;
	int			i, wcnt = 0, widx = 0, error = 0;
	uint64_t		curtime;
	struct timespec		t1, t2;

	if (cnt > MAX_WAIT_OBJECTS)
		return(STATUS_INVALID_PARAMETER);
	if (cnt > THREAD_WAIT_OBJECTS && wb_array == NULL)
		return(STATUS_INVALID_PARAMETER);

	mtx_lock(&ntoskrnl_dispatchlock);

	if (wb_array == NULL)
		w = &_wb_array[0];
	else
		w = wb_array;

	/* First pass: see if we can satisfy any waits immediately. */

	for (i = 0; i < cnt; i++) {
		if (obj[i]->dh_size == OTYPE_MUTEX) {
			km = (kmutant *)obj[i];
			if (km->km_ownerthread == NULL ||
			    km->km_ownerthread == curthread->td_proc) {
				obj[i]->dh_sigstate = FALSE;
				km->km_acquirecnt++;
				km->km_ownerthread = curthread->td_proc;
				if (wtype == WAITTYPE_ANY) {
					mtx_unlock(&ntoskrnl_dispatchlock);
					return (STATUS_WAIT_0 + i);
				}
			}
		} else if (obj[i]->dh_sigstate == TRUE) {
			if (obj[i]->dh_type == EVENT_TYPE_SYNC)
				obj[i]->dh_sigstate = FALSE;
			if (wtype == WAITTYPE_ANY) {
				mtx_unlock(&ntoskrnl_dispatchlock);
				return (STATUS_WAIT_0 + i);
			}
		}
	}

	/*
	 * Second pass: set up wait for anything we can't
	 * satisfy immediately.
	 */

	for (i = 0; i < cnt; i++) {
		if (obj[i]->dh_sigstate == TRUE)
			continue;
		INSERT_LIST_TAIL((&obj[i]->dh_waitlisthead),
		    (&w[i].wb_waitlist));
		w[i].wb_kthread = td;
		w[i].wb_object = obj[i];
		wcnt++;
	}

	if (duetime != NULL) {
		if (*duetime < 0) {
			tv.tv_sec = - (*duetime) / 10000000;
			tv.tv_usec = (- (*duetime) / 10) -
			    (tv.tv_sec * 1000000);
		} else {
			ntoskrnl_time(&curtime);
			if (*duetime < curtime)
				tv.tv_sec = tv.tv_usec = 0;
			else {
				tv.tv_sec = ((*duetime) - curtime) / 10000000;
				tv.tv_usec = ((*duetime) - curtime) / 10 -
				    (tv.tv_sec * 1000000);
			}
		}
	}

	while (wcnt) {
		nanotime(&t1);

		error = ndis_thsuspend(td->td_proc, &ntoskrnl_dispatchlock,
		    duetime == NULL ? 0 : tvtohz(&tv));

		nanotime(&t2);

		for (i = 0; i < cnt; i++) {
			if (obj[i]->dh_size == OTYPE_MUTEX) {
				km = (kmutant *)obj;
				if (km->km_ownerthread == NULL) {
					km->km_ownerthread =
					    curthread->td_proc;
					km->km_acquirecnt++;
				}
			}
			if (obj[i]->dh_sigstate == TRUE) {
				widx = i;
				if (obj[i]->dh_type == EVENT_TYPE_SYNC)
					obj[i]->dh_sigstate = FALSE;
				REMOVE_LIST_ENTRY((&w[i].wb_waitlist));
				INIT_LIST_HEAD((&w[i].wb_waitlist));
				wcnt--;
			}
		}

		if (error || wtype == WAITTYPE_ANY)
			break;

		if (duetime != NULL) {
			tv.tv_sec -= (t2.tv_sec - t1.tv_sec);
			tv.tv_usec -= (t2.tv_nsec - t1.tv_nsec) / 1000;
		}
	}

	if (wcnt) {
		for (i = 0; i < cnt; i++) {
			REMOVE_LIST_ENTRY((&w[i].wb_waitlist));
			INIT_LIST_HEAD((&w[i].wb_waitlist));
		}
	}

	if (error == EWOULDBLOCK) {
		mtx_unlock(&ntoskrnl_dispatchlock);
		return(STATUS_TIMEOUT);
	}

	if (wtype == WAITTYPE_ANY && wcnt) {
		mtx_unlock(&ntoskrnl_dispatchlock);
		return(STATUS_WAIT_0 + widx);
	}

	mtx_unlock(&ntoskrnl_dispatchlock);

	return(STATUS_SUCCESS);
}

static void
WRITE_REGISTER_USHORT(reg, val)
	uint16_t		*reg;
	uint16_t		val;
{
	bus_space_write_2(NDIS_BUS_SPACE_MEM, 0x0, (bus_size_t)reg, val);
	return;
}

static uint16_t
READ_REGISTER_USHORT(reg)
	uint16_t		*reg;
{
	return(bus_space_read_2(NDIS_BUS_SPACE_MEM, 0x0, (bus_size_t)reg));
}

static void
WRITE_REGISTER_ULONG(reg, val)
	uint32_t		*reg;
	uint32_t		val;
{
	bus_space_write_4(NDIS_BUS_SPACE_MEM, 0x0, (bus_size_t)reg, val);
	return;
}

static uint32_t
READ_REGISTER_ULONG(reg)
	uint32_t		*reg;
{
	return(bus_space_read_4(NDIS_BUS_SPACE_MEM, 0x0, (bus_size_t)reg));
}

static uint8_t
READ_REGISTER_UCHAR(reg)
	uint8_t			*reg;
{
	return(bus_space_read_1(NDIS_BUS_SPACE_MEM, 0x0, (bus_size_t)reg));
}

static void
WRITE_REGISTER_UCHAR(reg, val)
	uint8_t			*reg;
	uint8_t			val;
{
	bus_space_write_1(NDIS_BUS_SPACE_MEM, 0x0, (bus_size_t)reg, val);
	return;
}

static int64_t
_allmul(a, b)
	int64_t			a;
	int64_t			b;
{
	return (a * b);
}

static int64_t
_alldiv(a, b)
	int64_t			a;
	int64_t			b;
{
	return (a / b);
}

static int64_t
_allrem(a, b)
	int64_t			a;
	int64_t			b;
{
	return (a % b);
}

static uint64_t
_aullmul(a, b)
	uint64_t		a;
	uint64_t		b;
{
	return (a * b);
}

static uint64_t
_aulldiv(a, b)
	uint64_t		a;
	uint64_t		b;
{
	return (a / b);
}

static uint64_t
_aullrem(a, b)
	uint64_t		a;
	uint64_t		b;
{
	return (a % b);
}

static int64_t
_allshl(a, b)
	int64_t			a;
	uint8_t			b;
{
	return (a << b);
}

static uint64_t
_aullshl(a, b)
	uint64_t		a;
	uint8_t			b;
{
	return (a << b);
}

static int64_t
_allshr(a, b)
	int64_t			a;
	uint8_t			b;
{
	return (a >> b);
}

static uint64_t
_aullshr(a, b)
	uint64_t		a;
	uint8_t			b;
{
	return (a >> b);
}

static slist_entry *
ntoskrnl_pushsl(head, entry)
	slist_header		*head;
	slist_entry		*entry;
{
	slist_entry		*oldhead;

	oldhead = head->slh_list.slh_next;
	entry->sl_next = head->slh_list.slh_next;
	head->slh_list.slh_next = entry;
	head->slh_list.slh_depth++;
	head->slh_list.slh_seq++;

	return(oldhead);
}

static slist_entry *
ntoskrnl_popsl(head)
	slist_header		*head;
{
	slist_entry		*first;

	first = head->slh_list.slh_next;
	if (first != NULL) {
		head->slh_list.slh_next = first->sl_next;
		head->slh_list.slh_depth--;
		head->slh_list.slh_seq++;
	}

	return(first);
}

/*
 * We need this to make lookaside lists work for amd64.
 * We pass a pointer to ExAllocatePoolWithTag() the lookaside
 * list structure. For amd64 to work right, this has to be a
 * pointer to the wrapped version of the routine, not the
 * original. Letting the Windows driver invoke the original
 * function directly will result in a convention calling
 * mismatch and a pretty crash. On x86, this effectively
 * becomes a no-op since ipt_func and ipt_wrap are the same.
 */

static funcptr
ntoskrnl_findwrap(func)
	funcptr			func;
{
	image_patch_table	*patch;

	patch = ntoskrnl_functbl;
	while (patch->ipt_func != NULL) {
		if ((funcptr)patch->ipt_func == func)
			return((funcptr)patch->ipt_wrap);
		patch++;
	}

	return(NULL);
}

static void
ExInitializePagedLookasideList(lookaside, allocfunc, freefunc,
    flags, size, tag, depth)
	paged_lookaside_list	*lookaside;
	lookaside_alloc_func	*allocfunc;
	lookaside_free_func	*freefunc;
	uint32_t		flags;
	size_t			size;
	uint32_t		tag;
	uint16_t		depth;
{
	bzero((char *)lookaside, sizeof(paged_lookaside_list));

	if (size < sizeof(slist_entry))
		lookaside->nll_l.gl_size = sizeof(slist_entry);
	else
		lookaside->nll_l.gl_size = size;
	lookaside->nll_l.gl_tag = tag;
	if (allocfunc == NULL)
		lookaside->nll_l.gl_allocfunc =
		    ntoskrnl_findwrap((funcptr)ExAllocatePoolWithTag);
	else
		lookaside->nll_l.gl_allocfunc = allocfunc;

	if (freefunc == NULL)
		lookaside->nll_l.gl_freefunc =
		    ntoskrnl_findwrap((funcptr)ExFreePool);
	else
		lookaside->nll_l.gl_freefunc = freefunc;

#ifdef __i386__
	KeInitializeSpinLock(&lookaside->nll_obsoletelock);
#endif

	lookaside->nll_l.gl_type = NonPagedPool;
	lookaside->nll_l.gl_depth = depth;
	lookaside->nll_l.gl_maxdepth = LOOKASIDE_DEPTH;

	return;
}

static void
ExDeletePagedLookasideList(lookaside)
	paged_lookaside_list   *lookaside;
{
	void			*buf;
	void		(*freefunc)(void *);

	freefunc = lookaside->nll_l.gl_freefunc;
	while((buf = ntoskrnl_popsl(&lookaside->nll_l.gl_listhead)) != NULL)
		MSCALL1(freefunc, buf);

	return;
}

static void
ExInitializeNPagedLookasideList(lookaside, allocfunc, freefunc,
    flags, size, tag, depth)
	npaged_lookaside_list	*lookaside;
	lookaside_alloc_func	*allocfunc;
	lookaside_free_func	*freefunc;
	uint32_t		flags;
	size_t			size;
	uint32_t		tag;
	uint16_t		depth;
{
	bzero((char *)lookaside, sizeof(npaged_lookaside_list));

	if (size < sizeof(slist_entry))
		lookaside->nll_l.gl_size = sizeof(slist_entry);
	else
		lookaside->nll_l.gl_size = size;
	lookaside->nll_l.gl_tag = tag;
	if (allocfunc == NULL)
		lookaside->nll_l.gl_allocfunc =
		    ntoskrnl_findwrap((funcptr)ExAllocatePoolWithTag);
	else
		lookaside->nll_l.gl_allocfunc = allocfunc;

	if (freefunc == NULL)
		lookaside->nll_l.gl_freefunc =
		    ntoskrnl_findwrap((funcptr)ExFreePool);
	else
		lookaside->nll_l.gl_freefunc = freefunc;

#ifdef __i386__
	KeInitializeSpinLock(&lookaside->nll_obsoletelock);
#endif

	lookaside->nll_l.gl_type = NonPagedPool;
	lookaside->nll_l.gl_depth = depth;
	lookaside->nll_l.gl_maxdepth = LOOKASIDE_DEPTH;

	return;
}

static void
ExDeleteNPagedLookasideList(lookaside)
	npaged_lookaside_list   *lookaside;
{
	void			*buf;
	void		(*freefunc)(void *);

	freefunc = lookaside->nll_l.gl_freefunc;
	while((buf = ntoskrnl_popsl(&lookaside->nll_l.gl_listhead)) != NULL)
		MSCALL1(freefunc, buf);

	return;
}

static slist_entry *
InterlockedPushEntrySList(head, entry)
	slist_header		*head;
	slist_entry		*entry;
{
	slist_entry		*oldhead;

	oldhead = ExInterlockedPushEntrySList(head, entry, &ntoskrnl_global);

	return(oldhead);
}

static slist_entry *
InterlockedPopEntrySList(head)
	slist_header		*head;
{
	slist_entry		*first;

	first = ExInterlockedPopEntrySList(head, &ntoskrnl_global);

	return(first);
}

static slist_entry *
ExInterlockedPushEntrySList(head, entry, lock)
	slist_header		*head;
	slist_entry		*entry;
	kspin_lock		*lock;
{
	slist_entry		*oldhead;
	uint8_t			irql;

	KeAcquireSpinLock(lock, &irql);
	oldhead = ntoskrnl_pushsl(head, entry);
	KeReleaseSpinLock(lock, irql);

	return(oldhead);
}

static slist_entry *
ExInterlockedPopEntrySList(head, lock)
	slist_header		*head;
	kspin_lock		*lock;
{
	slist_entry		*first;
	uint8_t			irql;

	KeAcquireSpinLock(lock, &irql);
	first = ntoskrnl_popsl(head);
	KeReleaseSpinLock(lock, irql);

	return(first);
}

static uint16_t
ExQueryDepthSList(head)
	slist_header		*head;
{
	uint16_t		depth;
	uint8_t			irql;

	KeAcquireSpinLock(&ntoskrnl_global, &irql);
	depth = head->slh_list.slh_depth;
	KeReleaseSpinLock(&ntoskrnl_global, irql);

	return(depth);
}

/*
 * The KeInitializeSpinLock(), KefAcquireSpinLockAtDpcLevel()
 * and KefReleaseSpinLockFromDpcLevel() appear to be analagous
 * to splnet()/splx() in their use. We can't create a new mutex
 * lock here because there is no complimentary KeFreeSpinLock()
 * function. Instead, we grab a mutex from the mutex pool.
 */
void
KeInitializeSpinLock(lock)
	kspin_lock		*lock;
{
	*lock = 0;

	return;
}

#ifdef __i386__
void
KefAcquireSpinLockAtDpcLevel(lock)
	kspin_lock		*lock;
{
	while (atomic_cmpset_acq_int((volatile u_int *)lock, 0, 1) == 0)
		/* sit and spin */;

	return;
}

void
KefReleaseSpinLockFromDpcLevel(lock)
	kspin_lock		*lock;
{
	atomic_store_rel_int((volatile u_int *)lock, 0);

	return;
}

uint8_t
KeAcquireSpinLockRaiseToDpc(kspin_lock *lock)
{
        uint8_t                 oldirql;

        if (KeGetCurrentIrql() > DISPATCH_LEVEL)
                panic("IRQL_NOT_LESS_THAN_OR_EQUAL");

        oldirql = KeRaiseIrql(DISPATCH_LEVEL);
        KeAcquireSpinLockAtDpcLevel(lock);

        return(oldirql);
}
#else
void
KeAcquireSpinLockAtDpcLevel(kspin_lock *lock)
{
	while (atomic_cmpset_acq_int((volatile u_int *)lock, 0, 1) == 0)
		/* sit and spin */;

	return;
}

void
KeReleaseSpinLockFromDpcLevel(kspin_lock *lock)
{
	atomic_store_rel_int((volatile u_int *)lock, 0);

	return;
}
#endif /* __i386__ */

uintptr_t
InterlockedExchange(dst, val)
	volatile uint32_t	*dst;
	uintptr_t		val;
{
	uint8_t			irql;
	uintptr_t		r;

	KeAcquireSpinLock(&ntoskrnl_global, &irql);
	r = *dst;
	*dst = val;
	KeReleaseSpinLock(&ntoskrnl_global, irql);

	return(r);
}

static uint32_t
InterlockedIncrement(addend)
	volatile uint32_t	*addend;
{
	atomic_add_long((volatile u_long *)addend, 1);
	return(*addend);
}

static uint32_t
InterlockedDecrement(addend)
	volatile uint32_t	*addend;
{
	atomic_subtract_long((volatile u_long *)addend, 1);
	return(*addend);
}

static void
ExInterlockedAddLargeStatistic(addend, inc)
	uint64_t		*addend;
	uint32_t		inc;
{
	uint8_t			irql;

	KeAcquireSpinLock(&ntoskrnl_global, &irql);
	*addend += inc;
	KeReleaseSpinLock(&ntoskrnl_global, irql);

	return;
};

mdl *
IoAllocateMdl(vaddr, len, secondarybuf, chargequota, iopkt)
	void			*vaddr;
	uint32_t		len;
	uint8_t			secondarybuf;
	uint8_t			chargequota;
	irp			*iopkt;
{
	mdl			*m;
	int			zone = 0;

	if (MmSizeOfMdl(vaddr, len) > MDL_ZONE_SIZE)
		m = ExAllocatePoolWithTag(NonPagedPool,
		    MmSizeOfMdl(vaddr, len), 0);
	else {
		m = uma_zalloc(mdl_zone, M_NOWAIT | M_ZERO);
		zone++;
	}

	if (m == NULL)
		return (NULL);

	MmInitializeMdl(m, vaddr, len);

	/*
	 * MmInitializMdl() clears the flags field, so we
	 * have to set this here. If the MDL came from the
	 * MDL UMA zone, tag it so we can release it to
	 * the right place later.
	 */
	if (zone)
		m->mdl_flags = MDL_ZONE_ALLOCED;

	if (iopkt != NULL) {
		if (secondarybuf == TRUE) {
			mdl			*last;
			last = iopkt->irp_mdl;
			while (last->mdl_next != NULL)
				last = last->mdl_next;
			last->mdl_next = m;
		} else {
			if (iopkt->irp_mdl != NULL)
				panic("leaking an MDL in IoAllocateMdl()");
			iopkt->irp_mdl = m;
		}
	}

	return (m);
}

void
IoFreeMdl(m)
	mdl			*m;
{
	if (m == NULL)
		return;

	if (m->mdl_flags & MDL_ZONE_ALLOCED)
		uma_zfree(mdl_zone, m);
	else
		ExFreePool(m);

        return;
}

static uint32_t
MmSizeOfMdl(vaddr, len)
	void			*vaddr;
	size_t			len;
{
	uint32_t		l;

        l = sizeof(struct mdl) +
	    (sizeof(vm_offset_t *) * SPAN_PAGES(vaddr, len));

	return(l);
}

/*
 * The Microsoft documentation says this routine fills in the
 * page array of an MDL with the _physical_ page addresses that
 * comprise the buffer, but we don't really want to do that here.
 * Instead, we just fill in the page array with the kernel virtual
 * addresses of the buffers.
 */
static void
MmBuildMdlForNonPagedPool(m)
	mdl			*m;
{
	vm_offset_t		*mdl_pages;
	int			pagecnt, i;

	pagecnt = SPAN_PAGES(m->mdl_byteoffset, m->mdl_bytecount);

	if (pagecnt > (m->mdl_size - sizeof(mdl)) / sizeof(vm_offset_t *))
		panic("not enough pages in MDL to describe buffer");

	mdl_pages = MmGetMdlPfnArray(m);

	for (i = 0; i < pagecnt; i++)
		*mdl_pages = (vm_offset_t)m->mdl_startva + (i * PAGE_SIZE);

	m->mdl_flags |= MDL_SOURCE_IS_NONPAGED_POOL;
	m->mdl_mappedsystemva = MmGetMdlVirtualAddress(m);

	return;
}

static void *
MmMapLockedPages(buf, accessmode)
	mdl			*buf;
	uint8_t			accessmode;
{
	buf->mdl_flags |= MDL_MAPPED_TO_SYSTEM_VA;
	return(MmGetMdlVirtualAddress(buf));
}

static void *
MmMapLockedPagesSpecifyCache(buf, accessmode, cachetype, vaddr,
    bugcheck, prio)
	mdl			*buf;
	uint8_t			accessmode;
	uint32_t		cachetype;
	void			*vaddr;
	uint32_t		bugcheck;
	uint32_t		prio;
{
	return(MmMapLockedPages(buf, accessmode));
}

static void
MmUnmapLockedPages(vaddr, buf)
	void			*vaddr;
	mdl			*buf;
{
	buf->mdl_flags &= ~MDL_MAPPED_TO_SYSTEM_VA;
	return;
}

/*
 * This function has a problem in that it will break if you
 * compile this module without PAE and try to use it on a PAE
 * kernel. Unfortunately, there's no way around this at the
 * moment. It's slightly less broken that using pmap_kextract().
 * You'd think the virtual memory subsystem would help us out
 * here, but it doesn't.
 */

static uint8_t
MmIsAddressValid(vaddr)
	void			*vaddr;
{
	if (pmap_extract(kernel_map->pmap, (vm_offset_t)vaddr))
		return(TRUE);

	return(FALSE);
}

/*
 * Workitems are unlike DPCs, in that they run in a user-mode thread
 * context rather than at DISPATCH_LEVEL in kernel context. In our
 * case we run them in kernel context anyway.
 */
static void
ntoskrnl_workitem_thread(arg)
	void			*arg;
{
	kdpc_queue		*kq;
	list_entry		*l;
	io_workitem		*iw;

	kq = arg;

	INIT_LIST_HEAD(&kq->kq_med);
	kq->kq_td = curthread;
	kq->kq_exit = 0;
	kq->kq_state = NDIS_PSTATE_SLEEPING;
	mtx_init(&kq->kq_lock, "NDIS thread lock", NULL, MTX_SPIN);
	KeInitializeEvent(&kq->kq_proc, EVENT_TYPE_SYNC, FALSE);
	KeInitializeEvent(&kq->kq_dead, EVENT_TYPE_SYNC, FALSE);

	while (1) {

		KeWaitForSingleObject((nt_dispatch_header *)&kq->kq_proc,
		    0, 0, TRUE, NULL);

		mtx_lock_spin(&kq->kq_lock);

		if (kq->kq_exit) {
			mtx_unlock_spin(&kq->kq_lock);
			KeSetEvent(&kq->kq_dead, 0, FALSE);
			break;
		}

		kq->kq_state = NDIS_PSTATE_RUNNING;

		l = kq->kq_med.nle_flink;
		while (l != & kq->kq_med) {
			iw = CONTAINING_RECORD(l,
			    io_workitem, iw_listentry);
			REMOVE_LIST_HEAD((&kq->kq_med));
			INIT_LIST_HEAD(l);
			if (iw->iw_func == NULL) {
				l = kq->kq_med.nle_flink;
				continue;
			}
			mtx_unlock_spin(&kq->kq_lock);
			MSCALL2(iw->iw_func, iw->iw_dobj, iw->iw_ctx);
			mtx_lock_spin(&kq->kq_lock);
			l = kq->kq_med.nle_flink;
		}

		kq->kq_state = NDIS_PSTATE_SLEEPING;

		mtx_unlock_spin(&kq->kq_lock);
	}

	mtx_destroy(&kq->kq_lock);
#if __FreeBSD_version < 502113
        mtx_lock(&Giant);
#endif
        kthread_exit(0);
        return; /* notreached */
}

static void
ntoskrnl_destroy_workitem_threads(void)
{
	kdpc_queue		*kq;
	int			i;

	for (i = 0; i < WORKITEM_THREADS; i++) {
		kq = wq_queues + i;

		kq->kq_exit = 1;
		KeSetEvent(&kq->kq_proc, 0, FALSE);	
		KeWaitForSingleObject((nt_dispatch_header *)&kq->kq_dead,
	    	    0, 0, TRUE, NULL);
	}

	return;
}

io_workitem *
IoAllocateWorkItem(dobj)
	device_object		*dobj;
{
	io_workitem		*iw;

	iw = uma_zalloc(iw_zone, M_NOWAIT);
	if (iw == NULL)
		return(NULL);

	INIT_LIST_HEAD(&iw->iw_listentry);
	iw->iw_dobj = dobj;

	mtx_lock(&ntoskrnl_dispatchlock);
	iw->iw_idx = wq_idx;
	WORKIDX_INC(wq_idx);
	mtx_unlock(&ntoskrnl_dispatchlock);

	return(iw);
}

void
IoFreeWorkItem(iw)
	io_workitem		*iw;
{
	uma_zfree(iw_zone, iw);
	return;
}

void
IoQueueWorkItem(iw, iw_func, qtype, ctx)
	io_workitem		*iw;
	io_workitem_func	iw_func;
	uint32_t		qtype;
	void			*ctx;
{
	int			state;
	kdpc_queue		*kq;

	iw->iw_func = iw_func;
	iw->iw_ctx = ctx;


	kq = wq_queues + iw->iw_idx;

	mtx_lock_spin(&kq->kq_lock);
	INSERT_LIST_TAIL((&kq->kq_med), (&iw->iw_listentry));
	state = kq->kq_state;
	mtx_unlock_spin(&kq->kq_lock);
	if (state == NDIS_PSTATE_SLEEPING)
		KeSetEvent(&kq->kq_proc, 0, FALSE);

	return;
}

static void
ntoskrnl_workitem(dobj, arg)
	device_object		*dobj;
	void			*arg;
{
	io_workitem		*iw;
	work_queue_item		*w;
	work_item_func		f;

	iw = arg;
	w = (work_queue_item *)dobj;
	f = (work_item_func)w->wqi_func;
	uma_zfree(iw_zone, iw);
	MSCALL2(f, w, w->wqi_ctx);

	return;
}

void
ExQueueWorkItem(w, qtype)
	work_queue_item		*w;
	uint32_t		qtype;
{
	io_workitem		*iw;
	io_workitem_func	iwf;

	iw = IoAllocateWorkItem((device_object *)w);
	if (iw == NULL)
		return;

	iw->iw_idx = WORKITEM_LEGACY_THREAD;
	iwf = (io_workitem_func)ntoskrnl_findwrap((funcptr)ntoskrnl_workitem);
	IoQueueWorkItem(iw, iwf, qtype, iw);

	return;
}

static size_t
RtlCompareMemory(s1, s2, len)
	const void		*s1;
	const void		*s2;
	size_t			len;
{
	size_t			i, total = 0;
	uint8_t			*m1, *m2;

	m1 = __DECONST(char *, s1);
	m2 = __DECONST(char *, s2);

	for (i = 0; i < len; i++) {
		if (m1[i] == m2[i])
			total++;
	}
	return(total);
}

static void
RtlInitAnsiString(dst, src)
	ndis_ansi_string	*dst;
	char			*src;
{
	ndis_ansi_string	*a;

	a = dst;
	if (a == NULL)
		return;
	if (src == NULL) {
		a->nas_len = a->nas_maxlen = 0;
		a->nas_buf = NULL;
	} else {
		a->nas_buf = src;
		a->nas_len = a->nas_maxlen = strlen(src);
	}

	return;
}

static void
RtlInitUnicodeString(dst, src)
	ndis_unicode_string	*dst;
	uint16_t		*src;
{
	ndis_unicode_string	*u;
	int			i;

	u = dst;
	if (u == NULL)
		return;
	if (src == NULL) {
		u->us_len = u->us_maxlen = 0;
		u->us_buf = NULL;
	} else {
		i = 0;
		while(src[i] != 0)
			i++;
		u->us_buf = src;
		u->us_len = u->us_maxlen = i * 2;
	}

	return;
}

ndis_status
RtlUnicodeStringToInteger(ustr, base, val)
	ndis_unicode_string	*ustr;
	uint32_t		base;
	uint32_t		*val;
{
	uint16_t		*uchr;
	int			len, neg = 0;
	char			abuf[64];
	char			*astr;

	uchr = ustr->us_buf;
	len = ustr->us_len;
	bzero(abuf, sizeof(abuf));

	if ((char)((*uchr) & 0xFF) == '-') {
		neg = 1;
		uchr++;
		len -= 2;
	} else if ((char)((*uchr) & 0xFF) == '+') {
		neg = 0;
		uchr++;
		len -= 2;
	}

	if (base == 0) {
		if ((char)((*uchr) & 0xFF) == 'b') {
			base = 2;
			uchr++;
			len -= 2;
		} else if ((char)((*uchr) & 0xFF) == 'o') {
			base = 8;
			uchr++;
			len -= 2;
		} else if ((char)((*uchr) & 0xFF) == 'x') {
			base = 16;
			uchr++;
			len -= 2;
		} else
			base = 10;
	}

	astr = abuf;
	if (neg) {
		strcpy(astr, "-");
		astr++;
	}

	ndis_unicode_to_ascii(uchr, len, &astr);
	*val = strtoul(abuf, NULL, base);

	return(NDIS_STATUS_SUCCESS);
}

static void
RtlFreeUnicodeString(ustr)
	ndis_unicode_string	*ustr;
{
	if (ustr->us_buf == NULL)
		return;
	free(ustr->us_buf, M_DEVBUF);
	ustr->us_buf = NULL;
	return;
}

static void
RtlFreeAnsiString(astr)
	ndis_ansi_string	*astr;
{
	if (astr->nas_buf == NULL)
		return;
	free(astr->nas_buf, M_DEVBUF);
	astr->nas_buf = NULL;
	return;
}

static int
atoi(str)
	const char		*str;
{
	return (int)strtol(str, (char **)NULL, 10);
}

static long
atol(str)
	const char		*str;
{
	return strtol(str, (char **)NULL, 10);
}

static int
rand(void)
{
	struct timeval		tv;

	microtime(&tv);
	srandom(tv.tv_usec);
	return((int)random());
}

static void
srand(seed)
	unsigned int		seed;
{
	srandom(seed);
	return;
}

static uint8_t
IoIsWdmVersionAvailable(major, minor)
	uint8_t			major;
	uint8_t			minor;
{
	if (major == WDM_MAJOR && minor == WDM_MINOR_WINXP)
		return(TRUE);
	return(FALSE);
}

static ndis_status
IoGetDeviceProperty(devobj, regprop, buflen, prop, reslen)
	device_object		*devobj;
	uint32_t		regprop;
	uint32_t		buflen;
	void			*prop;
	uint32_t		*reslen;
{
	driver_object		*drv;
	uint16_t		**name;

	drv = devobj->do_drvobj;

	switch (regprop) {
	case DEVPROP_DRIVER_KEYNAME:
		name = prop;
		*name = drv->dro_drivername.us_buf;
		*reslen = drv->dro_drivername.us_len;
		break;
	default:
		return(STATUS_INVALID_PARAMETER_2);
		break;
	}

	return(STATUS_SUCCESS);
}

static void
KeInitializeMutex(kmutex, level)
	kmutant			*kmutex;
	uint32_t		level;
{
	INIT_LIST_HEAD((&kmutex->km_header.dh_waitlisthead));
	kmutex->km_abandoned = FALSE;
	kmutex->km_apcdisable = 1;
	kmutex->km_header.dh_sigstate = TRUE;
	kmutex->km_header.dh_type = EVENT_TYPE_SYNC;
	kmutex->km_header.dh_size = OTYPE_MUTEX;
	kmutex->km_acquirecnt = 0;
	kmutex->km_ownerthread = NULL;
	return;
}

static uint32_t
KeReleaseMutex(kmutex, kwait)
	kmutant			*kmutex;
	uint8_t			kwait;
{
	mtx_lock(&ntoskrnl_dispatchlock);
	if (kmutex->km_ownerthread != curthread->td_proc) {
		mtx_unlock(&ntoskrnl_dispatchlock);
		return(STATUS_MUTANT_NOT_OWNED);
	}
	kmutex->km_acquirecnt--;
	if (kmutex->km_acquirecnt == 0) {
		kmutex->km_ownerthread = NULL;
		ntoskrnl_wakeup(&kmutex->km_header);
	}
	mtx_unlock(&ntoskrnl_dispatchlock);

	return(kmutex->km_acquirecnt);
}

static uint32_t
KeReadStateMutex(kmutex)
	kmutant			*kmutex;
{
	return(kmutex->km_header.dh_sigstate);
}

void
KeInitializeEvent(kevent, type, state)
	nt_kevent		*kevent;
	uint32_t		type;
	uint8_t			state;
{
	INIT_LIST_HEAD((&kevent->k_header.dh_waitlisthead));
	kevent->k_header.dh_sigstate = state;
	kevent->k_header.dh_type = type;
	kevent->k_header.dh_size = OTYPE_EVENT;
	return;
}

uint32_t
KeResetEvent(kevent)
	nt_kevent		*kevent;
{
	uint32_t		prevstate;

	mtx_lock(&ntoskrnl_dispatchlock);
	prevstate = kevent->k_header.dh_sigstate;
	kevent->k_header.dh_sigstate = FALSE;
	mtx_unlock(&ntoskrnl_dispatchlock);

	return(prevstate);
}

uint32_t
KeSetEvent(kevent, increment, kwait)
	nt_kevent		*kevent;
	uint32_t		increment;
	uint8_t			kwait;
{
	uint32_t		prevstate;

	mtx_lock(&ntoskrnl_dispatchlock);
	prevstate = kevent->k_header.dh_sigstate;
	ntoskrnl_wakeup(&kevent->k_header);
	mtx_unlock(&ntoskrnl_dispatchlock);

	return(prevstate);
}

void
KeClearEvent(kevent)
	nt_kevent		*kevent;
{
	kevent->k_header.dh_sigstate = FALSE;
	return;
}

uint32_t
KeReadStateEvent(kevent)
	nt_kevent		*kevent;
{
	return(kevent->k_header.dh_sigstate);
}

static ndis_status
ObReferenceObjectByHandle(handle, reqaccess, otype,
    accessmode, object, handleinfo)
	ndis_handle		handle;
	uint32_t		reqaccess;
	void			*otype;
	uint8_t			accessmode;
	void			**object;
	void			**handleinfo;
{
	nt_objref		*nr;

	nr = malloc(sizeof(nt_objref), M_DEVBUF, M_NOWAIT|M_ZERO);
	if (nr == NULL)
		return(NDIS_STATUS_FAILURE);

	INIT_LIST_HEAD((&nr->no_dh.dh_waitlisthead));
	nr->no_obj = handle;
	nr->no_dh.dh_size = OTYPE_THREAD;
	TAILQ_INSERT_TAIL(&ntoskrnl_reflist, nr, link);
	*object = nr;

	return(NDIS_STATUS_SUCCESS);
}

static void
ObfDereferenceObject(object)
	void			*object;
{
	nt_objref		*nr;

	nr = object;
	TAILQ_REMOVE(&ntoskrnl_reflist, nr, link);
	free(nr, M_DEVBUF);

	return;
}

static uint32_t
ZwClose(handle)
	ndis_handle		handle;
{
	return(STATUS_SUCCESS);
}

/*
 * This is here just in case the thread returns without calling
 * PsTerminateSystemThread().
 */
static void
ntoskrnl_thrfunc(arg)
	void			*arg;
{
	thread_context		*thrctx;
	uint32_t (*tfunc)(void *);
	void			*tctx;
	uint32_t		rval;

	thrctx = arg;
	tfunc = thrctx->tc_thrfunc;
	tctx = thrctx->tc_thrctx;
	free(thrctx, M_TEMP);

	rval = MSCALL1(tfunc, tctx);

	PsTerminateSystemThread(rval);
	return; /* notreached */
}

static ndis_status
PsCreateSystemThread(handle, reqaccess, objattrs, phandle,
	clientid, thrfunc, thrctx)
	ndis_handle		*handle;
	uint32_t		reqaccess;
	void			*objattrs;
	ndis_handle		phandle;
	void			*clientid;
	void			*thrfunc;
	void			*thrctx;
{
	int			error;
	char			tname[128];
	thread_context		*tc;
	struct proc		*p;

	tc = malloc(sizeof(thread_context), M_TEMP, M_NOWAIT);
	if (tc == NULL)
		return(NDIS_STATUS_FAILURE);

	tc->tc_thrctx = thrctx;
	tc->tc_thrfunc = thrfunc;

	sprintf(tname, "windows kthread %d", ntoskrnl_kth);
	error = kthread_create(ntoskrnl_thrfunc, tc, &p,
	    RFHIGHPID, NDIS_KSTACK_PAGES, tname);
	*handle = p;

	ntoskrnl_kth++;

	return(error);
}

/*
 * In Windows, the exit of a thread is an event that you're allowed
 * to wait on, assuming you've obtained a reference to the thread using
 * ObReferenceObjectByHandle(). Unfortunately, the only way we can
 * simulate this behavior is to register each thread we create in a
 * reference list, and if someone holds a reference to us, we poke
 * them.
 */
static ndis_status
PsTerminateSystemThread(status)
	ndis_status		status;
{
	struct nt_objref	*nr;

	mtx_lock(&ntoskrnl_dispatchlock);
	TAILQ_FOREACH(nr, &ntoskrnl_reflist, link) {
		if (nr->no_obj != curthread->td_proc)
			continue;
		ntoskrnl_wakeup(&nr->no_dh);
		break;
	}
	mtx_unlock(&ntoskrnl_dispatchlock);

	ntoskrnl_kth--;

#if __FreeBSD_version < 502113
	mtx_lock(&Giant);
#endif
	kthread_exit(0);
	return(0);	/* notreached */
}

static uint32_t
DbgPrint(char *fmt, ...)
{
	va_list			ap;

	if (bootverbose) {
		va_start(ap, fmt);
		vprintf(fmt, ap);
	}

	return(STATUS_SUCCESS);
}

static void
DbgBreakPoint(void)
{

#if __FreeBSD_version < 502113
	Debugger("DbgBreakPoint(): breakpoint");
#else
	kdb_enter("DbgBreakPoint(): breakpoint");
#endif
}

static void
ntoskrnl_timercall(arg)
	void			*arg;
{
	ktimer			*timer;
	struct timeval		tv;

	mtx_unlock(&Giant);

	mtx_lock(&ntoskrnl_dispatchlock);

	timer = arg;

	timer->k_header.dh_inserted = FALSE;

	/*
	 * If this is a periodic timer, re-arm it
	 * so it will fire again. We do this before
	 * calling any deferred procedure calls because
	 * it's possible the DPC might cancel the timer,
	 * in which case it would be wrong for us to
	 * re-arm it again afterwards.
	 */

	if (timer->k_period) {
		tv.tv_sec = 0;
		tv.tv_usec = timer->k_period * 1000;
		timer->k_header.dh_inserted = TRUE;
		timer->k_handle = timeout(ntoskrnl_timercall,
		    timer, tvtohz(&tv));
	}

	if (timer->k_dpc != NULL)
		KeInsertQueueDpc(timer->k_dpc, NULL, NULL);

	ntoskrnl_wakeup(&timer->k_header);
	mtx_unlock(&ntoskrnl_dispatchlock);

	mtx_lock(&Giant);

	return;
}

void
KeInitializeTimer(timer)
	ktimer			*timer;
{
	if (timer == NULL)
		return;

	KeInitializeTimerEx(timer,  EVENT_TYPE_NOTIFY);

	return;
}

void
KeInitializeTimerEx(timer, type)
	ktimer			*timer;
	uint32_t		type;
{
	if (timer == NULL)
		return;

	bzero((char *)timer, sizeof(ktimer));
	INIT_LIST_HEAD((&timer->k_header.dh_waitlisthead));
	timer->k_header.dh_sigstate = FALSE;
	timer->k_header.dh_inserted = FALSE;
	timer->k_header.dh_type = type;
	timer->k_header.dh_size = OTYPE_TIMER;
	callout_handle_init(&timer->k_handle);

	return;
}

/*
 * DPC subsystem. A Windows Defered Procedure Call has the following
 * properties:
 * - It runs at DISPATCH_LEVEL.
 * - It can have one of 3 importance values that control when it
 *   runs relative to other DPCs in the queue.
 * - On SMP systems, it can be set to run on a specific processor.
 * In order to satisfy the last property, we create a DPC thread for
 * each CPU in the system and bind it to that CPU. Each thread
 * maintains three queues with different importance levels, which
 * will be processed in order from lowest to highest.
 *
 * In Windows, interrupt handlers run as DPCs. (Not to be confused
 * with ISRs, which run in interrupt context and can preempt DPCs.)
 * ISRs are given the highest importance so that they'll take
 * precedence over timers and other things.
 */

static void
ntoskrnl_dpc_thread(arg)
	void			*arg;
{
	kdpc_queue		*kq;
	kdpc			*d;
	list_entry		*l;

	kq = arg;

	INIT_LIST_HEAD(&kq->kq_high);
	INIT_LIST_HEAD(&kq->kq_low);
	INIT_LIST_HEAD(&kq->kq_med);
	kq->kq_td = curthread;
	kq->kq_exit = 0;
	kq->kq_state = NDIS_PSTATE_SLEEPING;
	mtx_init(&kq->kq_lock, "NDIS thread lock", NULL, MTX_SPIN);
	KeInitializeEvent(&kq->kq_proc, EVENT_TYPE_SYNC, FALSE);
	KeInitializeEvent(&kq->kq_done, EVENT_TYPE_SYNC, FALSE);
	KeInitializeEvent(&kq->kq_dead, EVENT_TYPE_SYNC, FALSE);

	sched_pin();

	while (1) {
		KeWaitForSingleObject((nt_dispatch_header *)&kq->kq_proc,
		    0, 0, TRUE, NULL);

		mtx_lock_spin(&kq->kq_lock);

		if (kq->kq_exit) {
			mtx_unlock_spin(&kq->kq_lock);
			KeSetEvent(&kq->kq_dead, 0, FALSE);
			break;
		}

		kq->kq_state = NDIS_PSTATE_RUNNING;

		/* Process high importance list first. */

		l = kq->kq_high.nle_flink;
		while (l != &kq->kq_high) {
			d = CONTAINING_RECORD(l, kdpc, k_dpclistentry);
			REMOVE_LIST_ENTRY((&d->k_dpclistentry));
			INIT_LIST_HEAD((&d->k_dpclistentry));
			d->k_lock = NULL;
			mtx_unlock_spin(&kq->kq_lock);
			ntoskrnl_run_dpc(d);
			mtx_lock_spin(&kq->kq_lock);
			l = kq->kq_high.nle_flink;
		}

		/* Now the medium importance list. */

		l = kq->kq_med.nle_flink;
		while (l != &kq->kq_med) {
			d = CONTAINING_RECORD(l, kdpc, k_dpclistentry);
			REMOVE_LIST_ENTRY((&d->k_dpclistentry));
			INIT_LIST_HEAD((&d->k_dpclistentry));
			d->k_lock = NULL;
			mtx_unlock_spin(&kq->kq_lock);
			ntoskrnl_run_dpc(d);
			mtx_lock_spin(&kq->kq_lock);
			l = kq->kq_med.nle_flink;
		}

		/* And finally the low importance list. */

		l = kq->kq_low.nle_flink;
		while (l != &kq->kq_low) {
			d = CONTAINING_RECORD(l, kdpc, k_dpclistentry);
			REMOVE_LIST_ENTRY((&d->k_dpclistentry));
			INIT_LIST_HEAD((&d->k_dpclistentry));
			d->k_lock = NULL;
			mtx_unlock_spin(&kq->kq_lock);
			ntoskrnl_run_dpc(d);
			mtx_lock_spin(&kq->kq_lock);
			l = kq->kq_low.nle_flink;
		}

		kq->kq_state = NDIS_PSTATE_SLEEPING;

		mtx_unlock_spin(&kq->kq_lock);

		KeSetEvent(&kq->kq_done, 0, FALSE);

	}

	mtx_destroy(&kq->kq_lock);
#if __FreeBSD_version < 502113
        mtx_lock(&Giant);
#endif
        kthread_exit(0);
        return; /* notreached */
}


/*
 * This is a wrapper for Windows deferred procedure calls that
 * have been placed on an NDIS thread work queue. We need it
 * since the DPC could be a _stdcall function. Also, as far as
 * I can tell, defered procedure calls must run at DISPATCH_LEVEL.
 */
static void
ntoskrnl_run_dpc(arg)
	void			*arg;
{
	kdpc_func		dpcfunc;
	kdpc			*dpc;
	uint8_t			irql;

	dpc = arg;
	dpcfunc = dpc->k_deferedfunc;
	if (dpcfunc == NULL)
		return;
	irql = KeRaiseIrql(DISPATCH_LEVEL);
	MSCALL4(dpcfunc, dpc, dpc->k_deferredctx,
	    dpc->k_sysarg1, dpc->k_sysarg2);
	KeLowerIrql(irql);

	return;
}

static void
ntoskrnl_destroy_dpc_threads(void)
{
	kdpc_queue		*kq;
	kdpc			dpc;
	int			i;

	kq = kq_queues;
	for (i = 0; i < mp_ncpus; i++) {
		kq += i;

		kq->kq_exit = 1;
		KeInitializeDpc(&dpc, NULL, NULL);
		KeSetTargetProcessorDpc(&dpc, i);
		KeInsertQueueDpc(&dpc, NULL, NULL);

		KeWaitForSingleObject((nt_dispatch_header *)&kq->kq_dead,
		    0, 0, TRUE, NULL);
	}

	return;
}

static uint8_t
ntoskrnl_insert_dpc(head, dpc)
	list_entry		*head;
	kdpc			*dpc;
{
	list_entry		*l;
	kdpc			*d;

	l = head->nle_flink;
	while (l != head) {
		d = CONTAINING_RECORD(l, kdpc, k_dpclistentry);
		if (d == dpc)
			return(FALSE);
		l = l->nle_flink;
	}

	INSERT_LIST_TAIL((head), (&dpc->k_dpclistentry));
	return (TRUE);
}

void
KeInitializeDpc(dpc, dpcfunc, dpcctx)
	kdpc			*dpc;
	void			*dpcfunc;
	void			*dpcctx;
{

	if (dpc == NULL)
		return;

	dpc->k_deferedfunc = dpcfunc;
	dpc->k_deferredctx = dpcctx;
	dpc->k_num = KDPC_CPU_DEFAULT;
	dpc->k_importance = KDPC_IMPORTANCE_MEDIUM;
	dpc->k_num = KeGetCurrentProcessorNumber();
	/*
	 * In case someone tries to dequeue a DPC that
	 * hasn't been queued yet.
	 */
	dpc->k_lock = NULL /*&ntoskrnl_dispatchlock*/;
	INIT_LIST_HEAD((&dpc->k_dpclistentry));

	return;
}

uint8_t
KeInsertQueueDpc(dpc, sysarg1, sysarg2)
	kdpc			*dpc;
	void			*sysarg1;
	void			*sysarg2;
{
	kdpc_queue		*kq;
	uint8_t			r;
	int			state;

	if (dpc == NULL)
		return(FALSE);

	dpc->k_sysarg1 = sysarg1;
	dpc->k_sysarg2 = sysarg2;

	/*
	 * By default, the DPC is queued to run on the same CPU
	 * that scheduled it.
	 */

	kq = kq_queues;
	if (dpc->k_num == KDPC_CPU_DEFAULT)
		kq += curthread->td_oncpu;
	else
		kq += dpc->k_num;

	/*
	 * Also by default, we put the DPC on the medium
	 * priority queue.
	 */

	mtx_lock_spin(&kq->kq_lock);
	if (dpc->k_importance == KDPC_IMPORTANCE_HIGH)
		r = ntoskrnl_insert_dpc(&kq->kq_high, dpc);
	else if (dpc->k_importance == KDPC_IMPORTANCE_LOW)
		r = ntoskrnl_insert_dpc(&kq->kq_low, dpc);
	else
		r = ntoskrnl_insert_dpc(&kq->kq_med, dpc);
	if (r == TRUE)
		dpc->k_lock = &kq->kq_lock;
	state = kq->kq_state;
	mtx_unlock_spin(&kq->kq_lock);
	if (r == FALSE)
		return(r);

	if (state == NDIS_PSTATE_SLEEPING)
		KeSetEvent(&kq->kq_proc, 0, FALSE);

	return(r);
}

uint8_t
KeRemoveQueueDpc(dpc)
	kdpc			*dpc;
{
	if (dpc == NULL)
		return(FALSE);

	if (dpc->k_lock == NULL)
		return(FALSE);

	mtx_lock_spin((struct mtx*)(dpc->k_lock));
	if (dpc->k_dpclistentry.nle_flink == &dpc->k_dpclistentry) {
		mtx_unlock_spin((struct mtx*)(dpc->k_lock));
		return(FALSE);
	}

	REMOVE_LIST_ENTRY((&dpc->k_dpclistentry));
	INIT_LIST_HEAD((&dpc->k_dpclistentry));
	mtx_unlock_spin((struct mtx*)(dpc->k_lock));

	return(TRUE);
}

void
KeSetImportanceDpc(dpc, imp)
	kdpc			*dpc;
	uint32_t		imp;
{
	if (imp != KDPC_IMPORTANCE_LOW &&
	    imp != KDPC_IMPORTANCE_MEDIUM &&
	    imp != KDPC_IMPORTANCE_HIGH)
		return;

	dpc->k_importance = (uint8_t)imp;
	return;
}

void
KeSetTargetProcessorDpc(dpc, cpu)
	kdpc			*dpc;
	uint8_t			cpu;
{
	if (cpu > mp_ncpus)
		return;

	dpc->k_num = cpu;
	return;
}

void
KeFlushQueuedDpcs(void)
{
	kdpc_queue		*kq;
	int			i;

	/*
	 * Poke each DPC queue and wait
	 * for them to drain.
	 */

	for (i = 0; i < mp_ncpus; i++) {
		kq = kq_queues + i;
		KeSetEvent(&kq->kq_proc, 0, FALSE);
		KeWaitForSingleObject((nt_dispatch_header *)&kq->kq_done,
		    0, 0, TRUE, NULL);
	}

	return;
}

uint32_t
KeGetCurrentProcessorNumber(void)
{
	return((uint32_t)curthread->td_oncpu);
}

uint8_t
KeSetTimerEx(timer, duetime, period, dpc)
	ktimer			*timer;
	int64_t			duetime;
	uint32_t		period;
	kdpc			*dpc;
{
	struct timeval		tv;
	uint64_t		curtime;
	uint8_t			pending;

	if (timer == NULL)
		return(FALSE);

	mtx_lock(&ntoskrnl_dispatchlock);

	if (timer->k_header.dh_inserted == TRUE) {
		untimeout(ntoskrnl_timercall, timer, timer->k_handle);
		timer->k_header.dh_inserted = FALSE;
		pending = TRUE;
	} else
		pending = FALSE;

	timer->k_duetime = duetime;
	timer->k_period = period;
	timer->k_header.dh_sigstate = FALSE;
	timer->k_dpc = dpc;

	if (duetime < 0) {
		tv.tv_sec = - (duetime) / 10000000;
		tv.tv_usec = (- (duetime) / 10) -
		    (tv.tv_sec * 1000000);
	} else {
		ntoskrnl_time(&curtime);
		if (duetime < curtime)
			tv.tv_sec = tv.tv_usec = 0;
		else {
			tv.tv_sec = ((duetime) - curtime) / 10000000;
			tv.tv_usec = ((duetime) - curtime) / 10 -
			    (tv.tv_sec * 1000000);
		}
	}

	timer->k_header.dh_inserted = TRUE;
	timer->k_handle = timeout(ntoskrnl_timercall, timer, tvtohz(&tv));

	mtx_unlock(&ntoskrnl_dispatchlock);

	return(pending);
}

uint8_t
KeSetTimer(timer, duetime, dpc)
	ktimer			*timer;
	int64_t			duetime;
	kdpc			*dpc;
{
	return (KeSetTimerEx(timer, duetime, 0, dpc));
}

uint8_t
KeCancelTimer(timer)
	ktimer			*timer;
{
	uint8_t			pending;

	if (timer == NULL)
		return(FALSE);

	mtx_lock(&ntoskrnl_dispatchlock);

	if (timer->k_header.dh_inserted == TRUE) {
		untimeout(ntoskrnl_timercall, timer, timer->k_handle);
		if (timer->k_dpc != NULL)
			KeRemoveQueueDpc(timer->k_dpc);
		timer->k_header.dh_inserted = FALSE;
		pending = TRUE;
	} else
		pending = KeRemoveQueueDpc(timer->k_dpc);

	mtx_unlock(&ntoskrnl_dispatchlock);

	return(pending);
}

uint8_t
KeReadStateTimer(timer)
	ktimer			*timer;
{
	return(timer->k_header.dh_sigstate);
}

static void
dummy()
{
	printf ("ntoskrnl dummy called...\n");
	return;
}


image_patch_table ntoskrnl_functbl[] = {
	IMPORT_SFUNC(RtlCompareMemory, 3),
	IMPORT_SFUNC(RtlEqualUnicodeString, 3),
	IMPORT_SFUNC(RtlCopyUnicodeString, 2),
	IMPORT_SFUNC(RtlUnicodeStringToAnsiString, 3),
	IMPORT_SFUNC(RtlAnsiStringToUnicodeString, 3),
	IMPORT_SFUNC(RtlInitAnsiString, 2),
	IMPORT_SFUNC_MAP(RtlInitString, RtlInitAnsiString, 2),
	IMPORT_SFUNC(RtlInitUnicodeString, 2),
	IMPORT_SFUNC(RtlFreeAnsiString, 1),
	IMPORT_SFUNC(RtlFreeUnicodeString, 1),
	IMPORT_SFUNC(RtlUnicodeStringToInteger, 3),
	IMPORT_CFUNC(sprintf, 0),
	IMPORT_CFUNC(vsprintf, 0),
	IMPORT_CFUNC_MAP(_snprintf, snprintf, 0),
	IMPORT_CFUNC_MAP(_vsnprintf, vsnprintf, 0),
	IMPORT_CFUNC(DbgPrint, 0),
	IMPORT_SFUNC(DbgBreakPoint, 0),
	IMPORT_CFUNC(strncmp, 0),
	IMPORT_CFUNC(strcmp, 0),
	IMPORT_CFUNC(strncpy, 0),
	IMPORT_CFUNC(strcpy, 0),
	IMPORT_CFUNC(strlen, 0),
	IMPORT_CFUNC_MAP(strstr, ntoskrnl_strstr, 0),
	IMPORT_CFUNC_MAP(strchr, index, 0),
	IMPORT_CFUNC(memcpy, 0),
	IMPORT_CFUNC_MAP(memmove, ntoskrnl_memset, 0),
	IMPORT_CFUNC_MAP(memset, ntoskrnl_memset, 0),
	IMPORT_SFUNC(IoAllocateDriverObjectExtension, 4),
	IMPORT_SFUNC(IoGetDriverObjectExtension, 2),
	IMPORT_FFUNC(IofCallDriver, 2),
	IMPORT_FFUNC(IofCompleteRequest, 2),
	IMPORT_SFUNC(IoAcquireCancelSpinLock, 1),
	IMPORT_SFUNC(IoReleaseCancelSpinLock, 1),
	IMPORT_SFUNC(IoCancelIrp, 1),
	IMPORT_SFUNC(IoCreateDevice, 7),
	IMPORT_SFUNC(IoDeleteDevice, 1),
	IMPORT_SFUNC(IoGetAttachedDevice, 1),
	IMPORT_SFUNC(IoAttachDeviceToDeviceStack, 2),
	IMPORT_SFUNC(IoDetachDevice, 1),
	IMPORT_SFUNC(IoBuildSynchronousFsdRequest, 7),
	IMPORT_SFUNC(IoBuildAsynchronousFsdRequest, 6),
	IMPORT_SFUNC(IoBuildDeviceIoControlRequest, 9),
	IMPORT_SFUNC(IoAllocateIrp, 2),
	IMPORT_SFUNC(IoReuseIrp, 2),
	IMPORT_SFUNC(IoMakeAssociatedIrp, 2),
	IMPORT_SFUNC(IoFreeIrp, 1),
	IMPORT_SFUNC(IoInitializeIrp, 3),
	IMPORT_SFUNC(KeWaitForSingleObject, 5),
	IMPORT_SFUNC(KeWaitForMultipleObjects, 8),
	IMPORT_SFUNC(_allmul, 4),
	IMPORT_SFUNC(_alldiv, 4),
	IMPORT_SFUNC(_allrem, 4),
	IMPORT_RFUNC(_allshr, 0),
	IMPORT_RFUNC(_allshl, 0),
	IMPORT_SFUNC(_aullmul, 4),
	IMPORT_SFUNC(_aulldiv, 4),
	IMPORT_SFUNC(_aullrem, 4),
	IMPORT_RFUNC(_aullshr, 0),
	IMPORT_RFUNC(_aullshl, 0),
	IMPORT_CFUNC(atoi, 0),
	IMPORT_CFUNC(atol, 0),
	IMPORT_CFUNC(rand, 0),
	IMPORT_CFUNC(srand, 0),
	IMPORT_SFUNC(WRITE_REGISTER_USHORT, 2),
	IMPORT_SFUNC(READ_REGISTER_USHORT, 1),
	IMPORT_SFUNC(WRITE_REGISTER_ULONG, 2),
	IMPORT_SFUNC(READ_REGISTER_ULONG, 1),
	IMPORT_SFUNC(READ_REGISTER_UCHAR, 1),
	IMPORT_SFUNC(WRITE_REGISTER_UCHAR, 2),
	IMPORT_SFUNC(ExInitializePagedLookasideList, 7),
	IMPORT_SFUNC(ExDeletePagedLookasideList, 1),
	IMPORT_SFUNC(ExInitializeNPagedLookasideList, 7),
	IMPORT_SFUNC(ExDeleteNPagedLookasideList, 1),
	IMPORT_FFUNC(InterlockedPopEntrySList, 1),
	IMPORT_FFUNC(InterlockedPushEntrySList, 2),
	IMPORT_SFUNC(ExQueryDepthSList, 1),
	IMPORT_FFUNC_MAP(ExpInterlockedPopEntrySList,
	 	InterlockedPopEntrySList, 1),
	IMPORT_FFUNC_MAP(ExpInterlockedPushEntrySList,
		InterlockedPushEntrySList, 2),
	IMPORT_FFUNC(ExInterlockedPopEntrySList, 2),
	IMPORT_FFUNC(ExInterlockedPushEntrySList, 3),
	IMPORT_SFUNC(ExAllocatePoolWithTag, 3),
	IMPORT_SFUNC(ExFreePool, 1),
#ifdef __i386__
	IMPORT_FFUNC(KefAcquireSpinLockAtDpcLevel, 1),
	IMPORT_FFUNC(KefReleaseSpinLockFromDpcLevel,1),
	IMPORT_FFUNC(KeAcquireSpinLockRaiseToDpc, 1),
#else
	/*
	 * For AMD64, we can get away with just mapping
	 * KeAcquireSpinLockRaiseToDpc() directly to KfAcquireSpinLock()
	 * because the calling conventions end up being the same.
	 * On i386, we have to be careful because KfAcquireSpinLock()
	 * is _fastcall but KeAcquireSpinLockRaiseToDpc() isn't.
	 */
	IMPORT_SFUNC(KeAcquireSpinLockAtDpcLevel, 1),
	IMPORT_SFUNC(KeReleaseSpinLockFromDpcLevel, 1),
	IMPORT_SFUNC_MAP(KeAcquireSpinLockRaiseToDpc, KfAcquireSpinLock, 1),
#endif
	IMPORT_SFUNC_MAP(KeReleaseSpinLock, KfReleaseSpinLock, 1),
	IMPORT_FFUNC(InterlockedIncrement, 1),
	IMPORT_FFUNC(InterlockedDecrement, 1),
	IMPORT_FFUNC(InterlockedExchange, 2),
	IMPORT_FFUNC(ExInterlockedAddLargeStatistic, 2),
	IMPORT_SFUNC(IoAllocateMdl, 5),
	IMPORT_SFUNC(IoFreeMdl, 1),
	IMPORT_SFUNC(MmSizeOfMdl, 1),
	IMPORT_SFUNC(MmMapLockedPages, 2),
	IMPORT_SFUNC(MmMapLockedPagesSpecifyCache, 6),
	IMPORT_SFUNC(MmUnmapLockedPages, 2),
	IMPORT_SFUNC(MmBuildMdlForNonPagedPool, 1),
	IMPORT_SFUNC(MmIsAddressValid, 1),
	IMPORT_SFUNC(KeInitializeSpinLock, 1),
	IMPORT_SFUNC(IoIsWdmVersionAvailable, 2),
	IMPORT_SFUNC(IoGetDeviceProperty, 5),
	IMPORT_SFUNC(IoAllocateWorkItem, 1),
	IMPORT_SFUNC(IoFreeWorkItem, 1),
	IMPORT_SFUNC(IoQueueWorkItem, 4),
	IMPORT_SFUNC(ExQueueWorkItem, 2),
	IMPORT_SFUNC(ntoskrnl_workitem, 2),
	IMPORT_SFUNC(KeInitializeMutex, 2),
	IMPORT_SFUNC(KeReleaseMutex, 2),
	IMPORT_SFUNC(KeReadStateMutex, 1),
	IMPORT_SFUNC(KeInitializeEvent, 3),
	IMPORT_SFUNC(KeSetEvent, 3),
	IMPORT_SFUNC(KeResetEvent, 1),
	IMPORT_SFUNC(KeClearEvent, 1),
	IMPORT_SFUNC(KeReadStateEvent, 1),
	IMPORT_SFUNC(KeInitializeTimer, 1),
	IMPORT_SFUNC(KeInitializeTimerEx, 2),
	IMPORT_SFUNC(KeSetTimer, 3),
	IMPORT_SFUNC(KeSetTimerEx, 4),
	IMPORT_SFUNC(KeCancelTimer, 1),
	IMPORT_SFUNC(KeReadStateTimer, 1),
	IMPORT_SFUNC(KeInitializeDpc, 3),
	IMPORT_SFUNC(KeInsertQueueDpc, 3),
	IMPORT_SFUNC(KeRemoveQueueDpc, 1),
	IMPORT_SFUNC(KeSetImportanceDpc, 2),
	IMPORT_SFUNC(KeSetTargetProcessorDpc, 2),
	IMPORT_SFUNC(KeFlushQueuedDpcs, 0),
	IMPORT_SFUNC(KeGetCurrentProcessorNumber, 1),
	IMPORT_SFUNC(ObReferenceObjectByHandle, 6),
	IMPORT_FFUNC(ObfDereferenceObject, 1),
	IMPORT_SFUNC(ZwClose, 1),
	IMPORT_SFUNC(PsCreateSystemThread, 7),
	IMPORT_SFUNC(PsTerminateSystemThread, 1),

	/*
	 * This last entry is a catch-all for any function we haven't
	 * implemented yet. The PE import list patching routine will
	 * use it for any function that doesn't have an explicit match
	 * in this table.
	 */

	{ NULL, (FUNC)dummy, NULL, 0, WINDRV_WRAP_STDCALL },

	/* End of list. */

	{ NULL, NULL, NULL }
};
