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
#include <sys/condvar.h>
#include <sys/kthread.h>
#include <sys/module.h>
#include <sys/smp.h>
#include <sys/sched.h>

#include <machine/atomic.h>
#include <machine/clock.h>
#include <machine/bus.h>
#include <machine/stdarg.h>
#include <machine/resource.h>

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
	list_entry		kq_disp;
	struct thread		*kq_td;
	int			kq_cpu;
	int			kq_exit;
	struct mtx		kq_lock;
	nt_kevent		kq_proc;
	nt_kevent		kq_done;
	nt_kevent		kq_dead;
};

typedef struct kdpc_queue kdpc_queue;

struct wb_ext {
	struct cv		we_cv;
	struct thread		*we_td;
};

typedef struct wb_ext wb_ext;

#define NTOSKRNL_TIMEOUTS	256
struct callout ntoskrnl_callout[NTOSKRNL_TIMEOUTS];
int ntoskrnl_callidx;
#define CALLOUT_INC(i)	(i) = ((i) + 1) % NTOSKRNL_TIMEOUTS

static uint8_t RtlEqualUnicodeString(unicode_string *,
	unicode_string *, uint8_t);
static void RtlCopyUnicodeString(unicode_string *,
	unicode_string *);
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
static void ntoskrnl_waittest(nt_dispatch_header *, uint32_t);
static void ntoskrnl_satisfy_wait(nt_dispatch_header *, struct thread *);
static void ntoskrnl_satisfy_multiple_waits(wait_block *);
static int ntoskrnl_is_signalled(nt_dispatch_header *, struct thread *);
static void ntoskrnl_timercall(void *);
static void ntoskrnl_run_dpc(void *);
static void ntoskrnl_dpc_thread(void *);
static void ntoskrnl_destroy_dpc_threads(void);
static void ntoskrnl_destroy_workitem_threads(void);
static void ntoskrnl_workitem_thread(void *);
static void ntoskrnl_workitem(device_object *, void *);
static void ntoskrnl_unicode_to_ascii(uint16_t *, char *, int);
static void ntoskrnl_ascii_to_unicode(char *, uint16_t *, int);
static uint8_t ntoskrnl_insert_dpc(list_entry *, kdpc *);
static device_t ntoskrnl_finddev(device_t, uint32_t,
	uint8_t, uint8_t, struct resource **);
static void ntoskrnl_intr(void *);
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
static void *MmMapLockedPages(mdl *, uint8_t);
static void *MmMapLockedPagesSpecifyCache(mdl *,
	uint8_t, uint32_t, void *, uint32_t, uint32_t);
static void MmUnmapLockedPages(void *, mdl *);
static uint8_t MmIsAddressValid(void *);
static size_t RtlCompareMemory(const void *, const void *, size_t);
static ndis_status RtlUnicodeStringToInteger(unicode_string *,
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
static int ntoskrnl_toupper(int);
static int ntoskrnl_tolower(int);
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

	for (i = 0; i < NTOSKRNL_TIMEOUTS; i++)
		callout_init(&ntoskrnl_callout[i], CALLOUT_MPSAFE);

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

	/* Stop the workitem queues. */
	ntoskrnl_destroy_workitem_threads();
	/* Stop the DPC queues. */
	ntoskrnl_destroy_dpc_threads();

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

static int
ntoskrnl_toupper(c)
	int			c;
{
	return(toupper(c));
}

static int
ntoskrnl_tolower(c)
	int			c;
{
	return(tolower(c));
}

static uint8_t 
RtlEqualUnicodeString(str1, str2, caseinsensitive)
	unicode_string		*str1;
	unicode_string		*str2;
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
	unicode_string		*dest;
	unicode_string		*src;
{

	if (dest->us_maxlen >= src->us_len)
		dest->us_len = src->us_len;
	else
		dest->us_len = dest->us_maxlen;
	memcpy(dest->us_buf, src->us_buf, dest->us_len);
	return;
}

static void
ntoskrnl_ascii_to_unicode(ascii, unicode, len)
	char			*ascii;
	uint16_t		*unicode;
	int			len;
{
	int			i;
	uint16_t		*ustr;

	ustr = unicode;
	for (i = 0; i < len; i++) {
		*ustr = (uint16_t)ascii[i];
		ustr++;
	}

	return;
}

static void
ntoskrnl_unicode_to_ascii(unicode, ascii, len)
	uint16_t		*unicode;
	char			*ascii;
	int			len;
{
	int			i;
	uint8_t			*astr;

	astr = ascii;
	for (i = 0; i < len / 2; i++) {
		*astr = (uint8_t)unicode[i];
		astr++;
	}

	return;
}

uint32_t
RtlUnicodeStringToAnsiString(dest, src, allocate)
	ansi_string		*dest;
	unicode_string		*src;
	uint8_t			allocate;
{
	if (dest == NULL || src == NULL)
		return(NDIS_STATUS_FAILURE);


	dest->as_len = src->us_len / 2;
	if (dest->as_maxlen < dest->as_len)
		dest->as_len = dest->as_maxlen;

	if (allocate == TRUE) {
		dest->as_buf = ExAllocatePoolWithTag(NonPagedPool,
		    (src->us_len / 2) + 1, 0);
		if (dest->as_buf == NULL)
			return(STATUS_INSUFFICIENT_RESOURCES);
		dest->as_len = dest->as_maxlen = src->us_len / 2;
	} else {
		dest->as_len = src->us_len / 2; /* XXX */
		if (dest->as_maxlen < dest->as_len)
			dest->as_len = dest->as_maxlen;
	}

	ntoskrnl_unicode_to_ascii(src->us_buf, dest->as_buf,
	    dest->as_len * 2);

	return (STATUS_SUCCESS);
}

uint32_t
RtlAnsiStringToUnicodeString(dest, src, allocate)
	unicode_string		*dest;
	ansi_string		*src;
	uint8_t			allocate;
{
	if (dest == NULL || src == NULL)
		return(NDIS_STATUS_FAILURE);

	if (allocate == TRUE) {
		dest->us_buf = ExAllocatePoolWithTag(NonPagedPool,
		    src->as_len * 2, 0);
		if (dest->us_buf == NULL)
			return(STATUS_INSUFFICIENT_RESOURCES);
		dest->us_len = dest->us_maxlen = strlen(src->as_buf) * 2;
	} else {
		dest->us_len = src->as_len * 2; /* XXX */
		if (dest->us_maxlen < dest->us_len)
			dest->us_len = dest->us_maxlen;
	}

	ntoskrnl_ascii_to_unicode(src->as_buf, dest->us_buf,
	    dest->us_len / 2);

	return (STATUS_SUCCESS);
}

void *
ExAllocatePoolWithTag(pooltype, len, tag)
	uint32_t		pooltype;
	size_t			len;
	uint32_t		tag;
{
	void			*buf;

	buf = malloc(len, M_DEVBUF, M_NOWAIT|M_ZERO);
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
	InsertTailList((&drv->dro_driverext->dre_usrext), (&ce->ce_list));

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
	InitializeListHead(&io->irp_thlist);
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

static device_t
ntoskrnl_finddev(dev, vector, irql, shared, res)
	device_t		dev;
	uint32_t		vector;
	uint8_t			irql;
	uint8_t			shared;
	struct resource		**res;
{
	device_t		*children;
	device_t		matching_dev;
	int			childcnt;
	struct resource		*r;
	struct resource_list	*rl;
	struct resource_list_entry	*rle;
	uint32_t		flags;
	int			i;

	/* We only want devices that have been successfully probed. */

	if (device_is_alive(dev) == FALSE)
		return(NULL);

	device_get_children(dev, &children, &childcnt);

	/*
	 * If this device has no children, it's a leaf: we can
	 * examine its resources. If the interrupt resource we
	 * want isn't here, we return NULL, otherwise we return
	 * the device to terminate the recursive search.
	 */

	if (childcnt == 0) {
		rl = BUS_GET_RESOURCE_LIST(device_get_parent(dev), dev);
		if (rl == NULL)
			return(NULL);
#if __FreeBSD_version < 600022
		SLIST_FOREACH(rle, rl, link) {
#else
		STAILQ_FOREACH(rle, rl, link) {
#endif
			r = rle->res;

			if (r == NULL)
				continue;

			flags = rman_get_flags(r);

			if (!(flags & RF_ACTIVE))
				continue;

			if (shared == TRUE && !(flags & RF_SHAREABLE))
				continue;

			if (rle->type == SYS_RES_IRQ &&
			    rman_get_start(r) == irql) {
				*res = r;
				return(dev);
			}
		}
		/* No match. */
		return (NULL);
	}

	/*
	 * If this device has children, do another
	 * level of recursion to inspect them.
	 */

	for (i = 0; i < childcnt; i++) {
		matching_dev = ntoskrnl_finddev(children[i],
		    vector, irql, shared, res);
		if (matching_dev != NULL) {
			free(children, M_TEMP);
			return(matching_dev);
		}
	}

	free(children, M_TEMP);
	return(NULL);
}

static void
ntoskrnl_intr(arg)
	void			*arg;
{
	kinterrupt		*iobj;
	uint8_t			irql;

	iobj = arg;

	KeAcquireSpinLock(iobj->ki_lock, &irql);
	MSCALL1(iobj->ki_svcfunc, iobj->ki_svcctx);
	KeReleaseSpinLock(iobj->ki_lock, irql);

	return;
}

uint8_t
KeSynchronizeExecution(iobj, syncfunc, syncctx)
	kinterrupt		*iobj;
	void			*syncfunc;
	void			*syncctx;
{
	uint8_t			irql;
	
	KeAcquireSpinLock(iobj->ki_lock, &irql);
	MSCALL1(syncfunc, syncctx);
	KeReleaseSpinLock(iobj->ki_lock, irql);

	return(TRUE);
}

/*
 * This routine is a pain because the only thing we get passed
 * here is the interrupt request level and vector, but bus_setup_intr()
 * needs the device too. We can hack around this for now, but it's
 * awkward.
 */

uint32_t
IoConnectInterrupt(iobj, svcfunc, svcctx, lock, vector, irql,
	syncirql, imode, shared, affinity, savefloat)
	kinterrupt		**iobj;
	void			*svcfunc;
	void			*svcctx;
	uint32_t		vector;
	kspin_lock		*lock;
	uint8_t			irql;
	uint8_t			syncirql;
	uint8_t			imode;
	uint8_t			shared;
	uint32_t		affinity;
	uint8_t			savefloat;
{
	devclass_t		nexus_class;
	device_t		*nexus_devs, devp;
	int			nexus_count = 0;
	device_t		matching_dev = NULL;
	struct resource		*res;
	int			i, error;

	nexus_class = devclass_find("nexus");
	devclass_get_devices(nexus_class, &nexus_devs, &nexus_count);

	for (i = 0; i < nexus_count; i++) {
		devp = nexus_devs[i];
		matching_dev = ntoskrnl_finddev(devp, vector,
		    irql, shared, &res);
		if (matching_dev)
			break;
	}

	free(nexus_devs, M_TEMP);

	if (matching_dev == NULL)
		return(STATUS_INVALID_PARAMETER);

	*iobj = ExAllocatePoolWithTag(NonPagedPool, sizeof(kinterrupt), 0);
	if (*iobj == NULL)
		return(STATUS_INSUFFICIENT_RESOURCES);

	(*iobj)->ki_dev = matching_dev;
	(*iobj)->ki_irq = res;
	(*iobj)->ki_svcfunc = svcfunc;
	(*iobj)->ki_svcctx = svcctx;

	if (lock == NULL) {
		KeInitializeSpinLock(&(*iobj)->ki_lock_priv);
		(*iobj)->ki_lock = &(*iobj)->ki_lock_priv;
	} else
		(*iobj)->ki_lock = lock;

	error = bus_setup_intr(matching_dev, res, INTR_TYPE_NET | INTR_MPSAFE,
	    ntoskrnl_intr, *iobj, &(*iobj)->ki_cookie);

	if (error) {
		ExFreePool(iobj);
		return (STATUS_INVALID_PARAMETER);
	}

	return(STATUS_SUCCESS);
}

void
IoDisconnectInterrupt(iobj)
	kinterrupt		*iobj;
{
	if (iobj == NULL)
		return;

	bus_teardown_intr(iobj->ki_dev, iobj->ki_irq, iobj->ki_cookie);
	ExFreePool(iobj);

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

	/* Now reduce the stacksize count for the takm_il objects. */

	tail = topdev->do_attacheddev;
	while (tail != NULL) {
		tail->do_stacksize--;
		tail = tail->do_attacheddev;
	}

	mtx_unlock(&ntoskrnl_dispatchlock);

	return;
}

/*
 * For the most part, an object is considered signalled if
 * dh_sigstate == TRUE. The exception is for mutant objects
 * (mutexes), where the logic works like this:
 *
 * - If the thread already owns the object and sigstate is
 *   less than or equal to 0, then the object is considered
 *   signalled (recursive acquisition).
 * - If dh_sigstate == 1, the object is also considered
 *   signalled.
 */

static int
ntoskrnl_is_signalled(obj, td)
	nt_dispatch_header	*obj;
	struct thread		*td;
{
	kmutant			*km;
	
	if (obj->dh_type == DISP_TYPE_MUTANT) {
		km = (kmutant *)obj;
		if ((obj->dh_sigstate <= 0 && km->km_ownerthread == td) ||
		    obj->dh_sigstate == 1)
			return(TRUE);
		return(FALSE);
	}

	if (obj->dh_sigstate > 0)
		return(TRUE);
	return(FALSE);
}

static void
ntoskrnl_satisfy_wait(obj, td)
	nt_dispatch_header	*obj;
	struct thread		*td;
{
	kmutant			*km;

	switch (obj->dh_type) {
	case DISP_TYPE_MUTANT:
		km = (struct kmutant *)obj;
		obj->dh_sigstate--;
		/*
		 * If sigstate reaches 0, the mutex is now
		 * non-signalled (the new thread owns it).
		 */
		if (obj->dh_sigstate == 0) {
			km->km_ownerthread = td;
			if (km->km_abandoned == TRUE)
				km->km_abandoned = FALSE;
		}
		break;
	/* Synchronization objects get reset to unsignalled. */
	case DISP_TYPE_SYNCHRONIZATION_EVENT:
	case DISP_TYPE_SYNCHRONIZATION_TIMER:
		obj->dh_sigstate = 0;
		break;
	case DISP_TYPE_SEMAPHORE:
		obj->dh_sigstate--;
		break;
	default:
		break;
	}

	return;
}

static void
ntoskrnl_satisfy_multiple_waits(wb)
	wait_block		*wb;
{
	wait_block		*cur;
	struct thread		*td;

	cur = wb;
	td = wb->wb_kthread;

	do {
		ntoskrnl_satisfy_wait(wb->wb_object, td);
		cur->wb_awakened = TRUE;
		cur = cur->wb_next;
	} while (cur != wb);

	return;
}

/* Always called with dispatcher lock held. */
static void
ntoskrnl_waittest(obj, increment)
	nt_dispatch_header	*obj;
	uint32_t		increment;
{
	wait_block		*w, *next;
	list_entry		*e;
	struct thread		*td;
	wb_ext			*we;
	int			satisfied;

	/*
	 * Once an object has been signalled, we walk its list of
	 * wait blocks. If a wait block can be awakened, then satisfy
	 * waits as necessary and wake the thread.
	 *
	 * The rules work like this:
	 *
	 * If a wait block is marked as WAITTYPE_ANY, then
	 * we can satisfy the wait conditions on the current
	 * object and wake the thread right away. Satisfying
	 * the wait also has the effect of breaking us out
	 * of the search loop.
	 *
	 * If the object is marked as WAITTYLE_ALL, then the
	 * wait block will be part of a circularly linked
	 * list of wait blocks belonging to a waiting thread
	 * that's sleeping in KeWaitForMultipleObjects(). In
	 * order to wake the thread, all the objects in the
	 * wait list must be in the signalled state. If they
	 * are, we then satisfy all of them and wake the
	 * thread.
	 *
	 */

	e = obj->dh_waitlisthead.nle_flink;

	while (e != &obj->dh_waitlisthead && obj->dh_sigstate > 0) {
		w = CONTAINING_RECORD(e, wait_block, wb_waitlist);
		we = w->wb_ext;
		td = we->we_td;
		satisfied = FALSE;
		if (w->wb_waittype == WAITTYPE_ANY) {
			/*
			 * Thread can be awakened if
			 * any wait is satisfied.
			 */
			ntoskrnl_satisfy_wait(obj, td);
			satisfied = TRUE;
			w->wb_awakened = TRUE;
		} else {
			/*
			 * Thread can only be woken up
			 * if all waits are satisfied.
			 * If the thread is waiting on multiple
			 * objects, they should all be linked
			 * through the wb_next pointers in the
			 * wait blocks.
			 */
			satisfied = TRUE;
			next = w->wb_next;
			while (next != w) {
				if (ntoskrnl_is_signalled(obj, td) == FALSE) {
					satisfied = FALSE;
					break;
				}
				next = next->wb_next;
			}
			ntoskrnl_satisfy_multiple_waits(w);
		}

		if (satisfied == TRUE)
			cv_broadcastpri(&we->we_cv, w->wb_oldpri -
			    (increment * 4));
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
 *   released, it enters the signalled state, which wakes up one of the
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
	wait_block		w;
	struct thread		*td = curthread;
	struct timeval		tv;
	int			error = 0;
	uint64_t		curtime;
	wb_ext			we;

	if (obj == NULL)
		return(STATUS_INVALID_PARAMETER);

	mtx_lock(&ntoskrnl_dispatchlock);

	cv_init(&we.we_cv, "KeWFS");
	we.we_td = td;

	/*
	 * Check to see if this object is already signalled,
	 * and just return without waiting if it is.
	 */
	if (ntoskrnl_is_signalled(obj, td) == TRUE) {
		/* Sanity check the signal state value. */
		if (obj->dh_sigstate != INT32_MIN) {
			ntoskrnl_satisfy_wait(obj, curthread);
			mtx_unlock(&ntoskrnl_dispatchlock);
			return (STATUS_SUCCESS);
		} else {
			/*
			 * There's a limit to how many times we can
			 * recursively acquire a mutant. If we hit
			 * the limit, something is very wrong.
			 */
			if (obj->dh_type == DISP_TYPE_MUTANT) {
				mtx_unlock(&ntoskrnl_dispatchlock);
				panic("mutant limit exceeded");
			}
		}
	}

	bzero((char *)&w, sizeof(wait_block));
	w.wb_object = obj;
	w.wb_ext = &we;
	w.wb_waittype = WAITTYPE_ANY;
	w.wb_next = &w;
	w.wb_waitkey = 0;
	w.wb_awakened = FALSE;
	w.wb_oldpri = td->td_priority;

	InsertTailList((&obj->dh_waitlisthead), (&w.wb_waitlist));

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

	if (duetime == NULL)
		cv_wait(&we.we_cv, &ntoskrnl_dispatchlock);
	else
		error = cv_timedwait(&we.we_cv,
		    &ntoskrnl_dispatchlock, tvtohz(&tv));

	RemoveEntryList(&w.wb_waitlist);

	cv_destroy(&we.we_cv);

	/* We timed out. Leave the object alone and return status. */

	if (error == EWOULDBLOCK) {
		mtx_unlock(&ntoskrnl_dispatchlock);
		return(STATUS_TIMEOUT);
	}

	mtx_unlock(&ntoskrnl_dispatchlock);

	return(STATUS_SUCCESS);
/*
	return(KeWaitForMultipleObjects(1, &obj, WAITTYPE_ALL, reason,
	    mode, alertable, duetime, &w));
*/
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
	wait_block		*whead, *w;
	wait_block		_wb_array[MAX_WAIT_OBJECTS];
	nt_dispatch_header	*cur;
	struct timeval		tv;
	int			i, wcnt = 0, error = 0;
	uint64_t		curtime;
	struct timespec		t1, t2;
	uint32_t		status = STATUS_SUCCESS;
	wb_ext			we;

	if (cnt > MAX_WAIT_OBJECTS)
		return(STATUS_INVALID_PARAMETER);
	if (cnt > THREAD_WAIT_OBJECTS && wb_array == NULL)
		return(STATUS_INVALID_PARAMETER);

	mtx_lock(&ntoskrnl_dispatchlock);

	cv_init(&we.we_cv, "KeWFM");
	we.we_td = td;

	if (wb_array == NULL)
		whead = _wb_array;
	else
		whead = wb_array;

	bzero((char *)whead, sizeof(wait_block) * cnt);

	/* First pass: see if we can satisfy any waits immediately. */

	wcnt = 0;
	w = whead;

	for (i = 0; i < cnt; i++) {
		InsertTailList((&obj[i]->dh_waitlisthead),
		    (&w->wb_waitlist));
		w->wb_ext = &we;
		w->wb_object = obj[i];
		w->wb_waittype = wtype;
		w->wb_waitkey = i;
		w->wb_awakened = FALSE;
		w->wb_oldpri = td->td_priority;
		w->wb_next = w + 1;
		w++;
		wcnt++;
		if (ntoskrnl_is_signalled(obj[i], td)) {
			/*
		 	 * There's a limit to how many times
		 	 * we can recursively acquire a mutant.
		 	 * If we hit the limit, something
			 * is very wrong.
		 	 */
			if (obj[i]->dh_sigstate == INT32_MIN &&
			    obj[i]->dh_type == DISP_TYPE_MUTANT) {
				mtx_unlock(&ntoskrnl_dispatchlock);
				panic("mutant limit exceeded");
			}

			/*
			 * If this is a WAITTYPE_ANY wait, then
			 * satisfy the waited object and exit
			 * right now.
			 */

			if (wtype == WAITTYPE_ANY) {
				ntoskrnl_satisfy_wait(obj[i], td);
				status = STATUS_WAIT_0 + i;
				goto wait_done;
			} else {
				w--;
				wcnt--;
				w->wb_object = NULL;
				RemoveEntryList(&w->wb_waitlist);
			}
		}
	}

	/*
	 * If this is a WAITTYPE_ALL wait and all objects are
	 * already signalled, satisfy the waits and exit now.
	 */

	if (wtype == WAITTYPE_ALL && wcnt == 0) {
		for (i = 0; i < cnt; i++)
			ntoskrnl_satisfy_wait(obj[i], td);
		status = STATUS_SUCCESS;
		goto wait_done;
	}

	/*
	 * Create a circular waitblock list. The waitcount
	 * must always be non-zero when we get here.
	 */

	(w - 1)->wb_next = whead;

        /* Wait on any objects that aren't yet signalled. */

	/* Calculate timeout, if any. */

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

		if (duetime == NULL)
			cv_wait(&we.we_cv, &ntoskrnl_dispatchlock);
		else
			error = cv_timedwait(&we.we_cv,
			    &ntoskrnl_dispatchlock, tvtohz(&tv));

		/* Wait with timeout expired. */

		if (error) {
			status = STATUS_TIMEOUT;
			goto wait_done;
		}

		nanotime(&t2);

		/* See what's been signalled. */

		w = whead;
		do {
			cur = w->wb_object;
			if (ntoskrnl_is_signalled(cur, td) == TRUE ||
			    w->wb_awakened == TRUE) {
				/* Sanity check the signal state value. */
				if (cur->dh_sigstate == INT32_MIN &&
				    cur->dh_type == DISP_TYPE_MUTANT) {
					mtx_unlock(&ntoskrnl_dispatchlock);
					panic("mutant limit exceeded");
				}
				wcnt--;
				if (wtype == WAITTYPE_ANY) {
					status = w->wb_waitkey &
					    STATUS_WAIT_0;
					goto wait_done;
				}
			}
			w = w->wb_next;
		} while (w != whead);

		/*
		 * If all objects have been signalled, or if this
		 * is a WAITTYPE_ANY wait and we were woke up by
		 * someone, we can bail.
		 */

		if (wcnt == 0) {
			status = STATUS_SUCCESS;
			goto wait_done;
		}

		/*
		 * If this is WAITTYPE_ALL wait, and there's still
		 * objects that haven't been signalled, deduct the
		 * time that's elapsed so far from the timeout and
		 * wait again (or continue waiting indefinitely if
		 * there's no timeout).
		 */

		if (duetime != NULL) {
			tv.tv_sec -= (t2.tv_sec - t1.tv_sec);
			tv.tv_usec -= (t2.tv_nsec - t1.tv_nsec) / 1000;
		}
	}


wait_done:

	cv_destroy(&we.we_cv);

	for (i = 0; i < cnt; i++) {
		if (whead[i].wb_object != NULL)
			RemoveEntryList(&whead[i].wb_waitlist);

	}
	mtx_unlock(&ntoskrnl_dispatchlock);

	return(status);
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
void
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

	InitializeListHead(&kq->kq_disp);
	kq->kq_td = curthread;
	kq->kq_exit = 0;
	mtx_init(&kq->kq_lock, "NDIS thread lock", NULL, MTX_SPIN);
	KeInitializeEvent(&kq->kq_proc, EVENT_TYPE_SYNC, FALSE);
	KeInitializeEvent(&kq->kq_dead, EVENT_TYPE_SYNC, FALSE);

	while (1) {
		KeWaitForSingleObject((nt_dispatch_header *)&kq->kq_proc,
		    0, 0, TRUE, NULL);

		mtx_lock_spin(&kq->kq_lock);

		if (kq->kq_exit) {
			mtx_unlock_spin(&kq->kq_lock);
			KeSetEvent(&kq->kq_dead, IO_NO_INCREMENT, FALSE);
			break;
		}

		while (!IsListEmpty(&kq->kq_disp)) {
			l = RemoveHeadList(&kq->kq_disp);
			iw = CONTAINING_RECORD(l,
			    io_workitem, iw_listentry);
			InitializeListHead((&iw->iw_listentry));
			if (iw->iw_func == NULL)
				continue;
			mtx_unlock_spin(&kq->kq_lock);
			MSCALL2(iw->iw_func, iw->iw_dobj, iw->iw_ctx);
			mtx_lock_spin(&kq->kq_lock);
		}

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
		KeSetEvent(&kq->kq_proc, IO_NO_INCREMENT, FALSE);	
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

	InitializeListHead(&iw->iw_listentry);
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
	kdpc_queue		*kq;
	list_entry		*l;
	io_workitem		*cur;

	iw->iw_func = iw_func;
	iw->iw_ctx = ctx;

	kq = wq_queues + iw->iw_idx;

	mtx_lock_spin(&kq->kq_lock);

	/*
	 * Traverse the list and make sure this workitem hasn't
	 * already been inserted. Queuing the same workitem
	 * twice will hose the list but good.
	 */

	l = kq->kq_disp.nle_flink;
	while (l != &kq->kq_disp) {
		cur = CONTAINING_RECORD(l, io_workitem, iw_listentry);
		if (cur == iw) {
			/* Already queued -- do nothing. */
			mtx_unlock_spin(&kq->kq_lock);
			return;
		}
		l = l->nle_flink;
	}

	InsertTailList((&kq->kq_disp), (&iw->iw_listentry));
	mtx_unlock_spin(&kq->kq_lock);

	KeSetEvent(&kq->kq_proc, IO_NO_INCREMENT, FALSE);

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

/*
 * The ExQueueWorkItem() API is deprecated in Windows XP. Microsoft
 * warns that it's unsafe and to use IoQueueWorkItem() instead. The
 * problem with ExQueueWorkItem() is that it can't guard against
 * the condition where a driver submits a job to the work queue and
 * is then unloaded before the job is able to run. IoQueueWorkItem()
 * acquires a reference to the device's device_object via the
 * object manager and retains it until after the job has completed,
 * which prevents the driver from being unloaded before the job
 * runs. (We don't currently support this behavior, though hopefully
 * that will change once the object manager API is fleshed out a bit.)
 *
 * Having said all that, the ExQueueWorkItem() API remains, because
 * there are still other parts of Windows that use it, including
 * NDIS itself: NdisScheduleWorkItem() calls ExQueueWorkItem().
 * We fake up the ExQueueWorkItem() API on top of our implementation
 * of IoQueueWorkItem(). Workitem thread #3 is reserved exclusively
 * for ExQueueWorkItem() jobs, and we pass a pointer to the work
 * queue item (provided by the caller) in to IoAllocateWorkItem()
 * instead of the device_object. We need to save this pointer so
 * we can apply a sanity check: as with the DPC queue and other
 * workitem queues, we can't allow the same work queue item to
 * be queued twice. If it's already pending, we silently return
 */

void
ExQueueWorkItem(w, qtype)
	work_queue_item		*w;
	uint32_t		qtype;
{
	io_workitem		*iw;
	io_workitem_func	iwf;
	kdpc_queue		*kq;
	list_entry		*l;
	io_workitem		*cur;


	/*
	 * We need to do a special sanity test to make sure
	 * the ExQueueWorkItem() API isn't used to queue
	 * the same workitem twice. Rather than checking the
	 * io_workitem pointer itself, we test the attached
	 * device object, which is really a pointer to the
	 * legacy work queue item structure.
	 */

	kq = wq_queues + WORKITEM_LEGACY_THREAD;
	mtx_lock_spin(&kq->kq_lock);
	l = kq->kq_disp.nle_flink;
	while (l != &kq->kq_disp) {
		cur = CONTAINING_RECORD(l, io_workitem, iw_listentry);
		if (cur->iw_dobj == (device_object *)w) {
			/* Already queued -- do nothing. */
			mtx_unlock_spin(&kq->kq_lock);
			return;
		}
		l = l->nle_flink;
	}
	mtx_unlock_spin(&kq->kq_lock);

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

void
RtlInitAnsiString(dst, src)
	ansi_string		*dst;
	char			*src;
{
	ansi_string		*a;

	a = dst;
	if (a == NULL)
		return;
	if (src == NULL) {
		a->as_len = a->as_maxlen = 0;
		a->as_buf = NULL;
	} else {
		a->as_buf = src;
		a->as_len = a->as_maxlen = strlen(src);
	}

	return;
}

void
RtlInitUnicodeString(dst, src)
	unicode_string		*dst;
	uint16_t		*src;
{
	unicode_string		*u;
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
	unicode_string		*ustr;
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

	ntoskrnl_unicode_to_ascii(uchr, astr, len);
	*val = strtoul(abuf, NULL, base);

	return(NDIS_STATUS_SUCCESS);
}

void
RtlFreeUnicodeString(ustr)
	unicode_string		*ustr;
{
	if (ustr->us_buf == NULL)
		return;
	ExFreePool(ustr->us_buf);
	ustr->us_buf = NULL;
	return;
}

void
RtlFreeAnsiString(astr)
	ansi_string		*astr;
{
	if (astr->as_buf == NULL)
		return;
	ExFreePool(astr->as_buf);
	astr->as_buf = NULL;
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
	InitializeListHead((&kmutex->km_header.dh_waitlisthead));
	kmutex->km_abandoned = FALSE;
	kmutex->km_apcdisable = 1;
	kmutex->km_header.dh_sigstate = 1;
	kmutex->km_header.dh_type = DISP_TYPE_MUTANT;
	kmutex->km_header.dh_size = sizeof(kmutant) / sizeof(uint32_t);
	kmutex->km_ownerthread = NULL;
	return;
}

static uint32_t
KeReleaseMutex(kmutex, kwait)
	kmutant			*kmutex;
	uint8_t			kwait;
{
	uint32_t		prevstate;

	mtx_lock(&ntoskrnl_dispatchlock);
	prevstate = kmutex->km_header.dh_sigstate;
	if (kmutex->km_ownerthread != curthread) {
		mtx_unlock(&ntoskrnl_dispatchlock);
		return(STATUS_MUTANT_NOT_OWNED);
	}

	kmutex->km_header.dh_sigstate++;
	kmutex->km_abandoned = FALSE;

	if (kmutex->km_header.dh_sigstate == 1) {
		kmutex->km_ownerthread = NULL;
		ntoskrnl_waittest(&kmutex->km_header, IO_NO_INCREMENT);
	}

	mtx_unlock(&ntoskrnl_dispatchlock);

	return(prevstate);
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
	InitializeListHead((&kevent->k_header.dh_waitlisthead));
	kevent->k_header.dh_sigstate = state;
	if (type == EVENT_TYPE_NOTIFY)
		kevent->k_header.dh_type = DISP_TYPE_NOTIFICATION_EVENT;
	else
		kevent->k_header.dh_type = DISP_TYPE_SYNCHRONIZATION_EVENT;
	kevent->k_header.dh_size = sizeof(nt_kevent) / sizeof(uint32_t);
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
	wait_block		*w;
	nt_dispatch_header	*dh;
	struct thread		*td;
	wb_ext			*we;

	mtx_lock(&ntoskrnl_dispatchlock);
	prevstate = kevent->k_header.dh_sigstate;
	dh = &kevent->k_header;

	if (IsListEmpty(&dh->dh_waitlisthead))
		/*
		 * If there's nobody in the waitlist, just set
		 * the state to signalled.
		 */
		dh->dh_sigstate = 1;
	else {
		/*
		 * Get the first waiter. If this is a synchronization
		 * event, just wake up that one thread (don't bother
		 * setting the state to signalled since we're supposed
		 * to automatically clear synchronization events anyway).
		 *
		 * If it's a notification event, or the the first
		 * waiter is doing a WAITTYPE_ALL wait, go through
		 * the full wait satisfaction process.
		 */
		w = CONTAINING_RECORD(dh->dh_waitlisthead.nle_flink,
		    wait_block, wb_waitlist);
		we = w->wb_ext;
		td = we->we_td;
		if (kevent->k_header.dh_type == DISP_TYPE_NOTIFICATION_EVENT ||
		    w->wb_waittype == WAITTYPE_ALL) {
			if (prevstate == 0) {
				dh->dh_sigstate = 1;
				ntoskrnl_waittest(dh, increment);
			}
		} else {
			w->wb_awakened |= TRUE;
			cv_broadcastpri(&we->we_cv, w->wb_oldpri -
			    (increment * 4));
		}
	}

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

/*
 * The object manager in Windows is responsible for managing
 * references and access to various types of objects, including
 * device_objects, events, threads, timers and so on. However,
 * there's a difference in the way objects are handled in user
 * mode versus kernel mode.
 *
 * In user mode (i.e. Win32 applications), all objects are
 * managed by the object manager. For example, when you create
 * a timer or event object, you actually end up with an 
 * object_header (for the object manager's bookkeeping
 * purposes) and an object body (which contains the actual object
 * structure, e.g. ktimer, kevent, etc...). This allows Windows
 * to manage resource quotas and to enforce access restrictions
 * on basically every kind of system object handled by the kernel.
 *
 * However, in kernel mode, you only end up using the object
 * manager some of the time. For example, in a driver, you create
 * a timer object by simply allocating the memory for a ktimer
 * structure and initializing it with KeInitializeTimer(). Hence,
 * the timer has no object_header and no reference counting or
 * security/resource checks are done on it. The assumption in
 * this case is that if you're running in kernel mode, you know
 * what you're doing, and you're already at an elevated privilege
 * anyway.
 *
 * There are some exceptions to this. The two most important ones
 * for our purposes are device_objects and threads. We need to use
 * the object manager to do reference counting on device_objects,
 * and for threads, you can only get a pointer to a thread's
 * dispatch header by using ObReferenceObjectByHandle() on the
 * handle returned by PsCreateSystemThread().
 */

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

	InitializeListHead((&nr->no_dh.dh_waitlisthead));
	nr->no_obj = handle;
	nr->no_dh.dh_type = DISP_TYPE_THREAD;
	nr->no_dh.dh_sigstate = 0;
	nr->no_dh.dh_size = (uint8_t)(sizeof(struct thread) /
	    sizeof(uint32_t));
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
		nr->no_dh.dh_sigstate = 1;
		ntoskrnl_waittest(&nr->no_dh, IO_NO_INCREMENT);
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

	mtx_lock(&ntoskrnl_dispatchlock);

	timer = arg;

	callout_init(timer->k_callout, CALLOUT_MPSAFE);

	/* Mark the timer as no longer being on the timer queue. */

	timer->k_header.dh_inserted = FALSE;

	/* Now signal the object and satisfy any waits on it. */

	timer->k_header.dh_sigstate = 1;
	ntoskrnl_waittest(&timer->k_header, IO_NO_INCREMENT);

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
		timer->k_callout = &ntoskrnl_callout[ntoskrnl_callidx];
		CALLOUT_INC(ntoskrnl_callidx);
		callout_reset(timer->k_callout, tvtohz(&tv),
		    ntoskrnl_timercall, timer);
	}

	/* If there's a DPC associated with the timer, queue it up. */

	if (timer->k_dpc != NULL)
		KeInsertQueueDpc(timer->k_dpc, NULL, NULL);

	mtx_unlock(&ntoskrnl_dispatchlock);

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
	InitializeListHead((&timer->k_header.dh_waitlisthead));
	timer->k_header.dh_sigstate = FALSE;
	timer->k_header.dh_inserted = FALSE;
	if (type == EVENT_TYPE_NOTIFY)
		timer->k_header.dh_type = DISP_TYPE_NOTIFICATION_TIMER;
	else
		timer->k_header.dh_type = DISP_TYPE_SYNCHRONIZATION_TIMER;
	timer->k_header.dh_size = sizeof(ktimer) / sizeof(uint32_t);

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

	InitializeListHead(&kq->kq_disp);
	kq->kq_td = curthread;
	kq->kq_exit = 0;
	mtx_init(&kq->kq_lock, "NDIS thread lock", NULL, MTX_SPIN);
	KeInitializeEvent(&kq->kq_proc, EVENT_TYPE_SYNC, FALSE);
	KeInitializeEvent(&kq->kq_done, EVENT_TYPE_SYNC, FALSE);
	KeInitializeEvent(&kq->kq_dead, EVENT_TYPE_SYNC, FALSE);

	/*
	 * Elevate our priority. DPCs are used to run interrupt
	 * handlers, and they should trigger as soon as possible
	 * once scheduled by an ISR.
	 */

	mtx_lock_spin(&sched_lock);
	sched_pin();
        sched_prio(curthread, PRI_MIN_KERN);
#if __FreeBSD_version < 600000
        curthread->td_base_pri = PRI_MIN_KERN;
#endif
	mtx_unlock_spin(&sched_lock);

	while (1) {
		KeWaitForSingleObject((nt_dispatch_header *)&kq->kq_proc,
		    0, 0, TRUE, NULL);

		mtx_lock_spin(&kq->kq_lock);

		if (kq->kq_exit) {
			mtx_unlock_spin(&kq->kq_lock);
			KeSetEvent(&kq->kq_dead, IO_NO_INCREMENT, FALSE);
			break;
		}

		while (!IsListEmpty(&kq->kq_disp)) {
			l = RemoveHeadList((&kq->kq_disp));
			d = CONTAINING_RECORD(l, kdpc, k_dpclistentry);
			InitializeListHead((&d->k_dpclistentry));
			d->k_lock = NULL;
			mtx_unlock_spin(&kq->kq_lock);
			ntoskrnl_run_dpc(d);
			mtx_lock_spin(&kq->kq_lock);
		}

		mtx_unlock_spin(&kq->kq_lock);

		KeSetEvent(&kq->kq_done, IO_NO_INCREMENT, FALSE);

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

	if (dpc->k_importance == KDPC_IMPORTANCE_HIGH)
		InsertHeadList((head), (&dpc->k_dpclistentry));
	else
		InsertTailList((head), (&dpc->k_dpclistentry));

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
	/*
	 * In case someone tries to dequeue a DPC that
	 * hasn't been queued yet.
	 */
	dpc->k_lock = NULL;
	InitializeListHead((&dpc->k_dpclistentry));

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
	r = ntoskrnl_insert_dpc(&kq->kq_disp, dpc);
	if (r == TRUE)
		dpc->k_lock = &kq->kq_lock;
	mtx_unlock_spin(&kq->kq_lock);

	if (r == FALSE)
		return(r);

	KeSetEvent(&kq->kq_proc, IO_NO_INCREMENT, FALSE);

	return(r);
}

uint8_t
KeRemoveQueueDpc(dpc)
	kdpc			*dpc;
{
	struct mtx		*lock;

	if (dpc == NULL)
		return(FALSE);

	lock = dpc->k_lock;

	if (lock == NULL)
		return(FALSE);

	mtx_lock_spin(lock);
	dpc->k_lock = NULL;
	if (dpc->k_dpclistentry.nle_flink == &dpc->k_dpclistentry) {
		mtx_unlock_spin(lock);
		return(FALSE);
	}

	RemoveEntryList((&dpc->k_dpclistentry));
	InitializeListHead((&dpc->k_dpclistentry));
	mtx_unlock_spin(lock);

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
		KeSetEvent(&kq->kq_proc, IO_NO_INCREMENT, FALSE);
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
		callout_stop(timer->k_callout);
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
	timer->k_callout = &ntoskrnl_callout[ntoskrnl_callidx];
	CALLOUT_INC(ntoskrnl_callidx);
	callout_reset(timer->k_callout, tvtohz(&tv),
	    ntoskrnl_timercall, timer);

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

/*
 * The Windows DDK documentation seems to say that cancelling
 * a timer that has a DPC will result in the DPC also being
 * cancelled, but this isn't really the case.
 */

uint8_t
KeCancelTimer(timer)
	ktimer			*timer;
{
	uint8_t			pending;

	if (timer == NULL)
		return(FALSE);

	mtx_lock(&ntoskrnl_dispatchlock);

	pending = timer->k_header.dh_inserted;

	if (timer->k_header.dh_inserted == TRUE) {
		timer->k_header.dh_inserted = FALSE;
		callout_stop(timer->k_callout);
	}

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
	IMPORT_CFUNC_MAP(toupper, ntoskrnl_toupper, 0),
	IMPORT_CFUNC_MAP(tolower, ntoskrnl_tolower, 0),
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
	IMPORT_SFUNC(IoConnectInterrupt, 11),
	IMPORT_SFUNC(IoDisconnectInterrupt, 1),
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
	IMPORT_SFUNC(KeSynchronizeExecution, 3),
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
