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

#include <machine/atomic.h>
#include <machine/clock.h>
#include <machine/bus_memio.h>
#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <machine/stdarg.h>

#include <sys/bus.h>
#include <sys/rman.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/uma.h>

#include <compat/ndis/pe_var.h>
#include <compat/ndis/ntoskrnl_var.h>
#include <compat/ndis/hal_var.h>
#include <compat/ndis/resource_var.h>
#include <compat/ndis/ndis_var.h>

#define __regparm __attribute__((regparm(3)))

__stdcall static uint8_t RtlEqualUnicodeString(ndis_unicode_string *,
	ndis_unicode_string *, uint8_t);
__stdcall static void RtlCopyUnicodeString(ndis_unicode_string *,
	ndis_unicode_string *);
__stdcall static ndis_status RtlUnicodeStringToAnsiString(ndis_ansi_string *,
	ndis_unicode_string *, uint8_t);
__stdcall static ndis_status RtlAnsiStringToUnicodeString(ndis_unicode_string *,
	ndis_ansi_string *, uint8_t);
__stdcall static irp *IoBuildSynchronousFsdRequest(uint32_t, device_object *,
	 void *, uint32_t, uint64_t *, nt_kevent *, io_status_block *);
__stdcall static irp *IoBuildAsynchronousFsdRequest(uint32_t,
	device_object *, void *, uint32_t, uint64_t *, io_status_block *);
__stdcall static irp *IoBuildDeviceIoControlRequest(uint32_t,
	device_object *, void *, uint32_t, void *, uint32_t,
	uint8_t, nt_kevent *, io_status_block *);
__stdcall static irp *IoAllocateIrp(uint8_t, uint8_t);
__stdcall static void IoReuseIrp(irp *, uint32_t);
__stdcall static void IoFreeIrp(irp *);
__stdcall static void IoInitializeIrp(irp *, uint16_t, uint8_t);
__stdcall static irp *IoMakeAssociatedIrp(irp *, uint8_t);
__stdcall static uint32_t KeWaitForMultipleObjects(uint32_t,
	nt_dispatch_header **, uint32_t, uint32_t, uint32_t, uint8_t,
	int64_t *, wait_block *);
static void ntoskrnl_wakeup(void *);
static void ntoskrnl_timercall(void *);
static void ntoskrnl_run_dpc(void *);
__stdcall static void WRITE_REGISTER_USHORT(uint16_t *, uint16_t);
__stdcall static uint16_t READ_REGISTER_USHORT(uint16_t *);
__stdcall static void WRITE_REGISTER_ULONG(uint32_t *, uint32_t);
__stdcall static uint32_t READ_REGISTER_ULONG(uint32_t *);
__stdcall static void WRITE_REGISTER_UCHAR(uint8_t *, uint8_t);
__stdcall static uint8_t READ_REGISTER_UCHAR(uint8_t *);
__stdcall static int64_t _allmul(int64_t, int64_t);
__stdcall static int64_t _alldiv(int64_t, int64_t);
__stdcall static int64_t _allrem(int64_t, int64_t);
__regparm static int64_t _allshr(int64_t, uint8_t);
__regparm static int64_t _allshl(int64_t, uint8_t);
__stdcall static uint64_t _aullmul(uint64_t, uint64_t);
__stdcall static uint64_t _aulldiv(uint64_t, uint64_t);
__stdcall static uint64_t _aullrem(uint64_t, uint64_t);
__regparm static uint64_t _aullshr(uint64_t, uint8_t);
__regparm static uint64_t _aullshl(uint64_t, uint8_t);
static slist_entry *ntoskrnl_pushsl(slist_header *, slist_entry *);
static slist_entry *ntoskrnl_popsl(slist_header *);
__stdcall static void ExInitializePagedLookasideList(paged_lookaside_list *,
	lookaside_alloc_func *, lookaside_free_func *,
	uint32_t, size_t, uint32_t, uint16_t);
__stdcall static void ExDeletePagedLookasideList(paged_lookaside_list *);
__stdcall static void ExInitializeNPagedLookasideList(npaged_lookaside_list *,
	lookaside_alloc_func *, lookaside_free_func *,
	uint32_t, size_t, uint32_t, uint16_t);
__stdcall static void ExDeleteNPagedLookasideList(npaged_lookaside_list *);
__fastcall static slist_entry
	*InterlockedPushEntrySList(REGARGS2(slist_header *head,
	slist_entry *entry));
__fastcall static slist_entry *InterlockedPopEntrySList(REGARGS1(slist_header
	*head));
__fastcall static slist_entry
	*ExInterlockedPushEntrySList(REGARGS2(slist_header *head,
	slist_entry *entry), kspin_lock *lock);
__fastcall static slist_entry
	*ExInterlockedPopEntrySList(REGARGS2(slist_header *head,
	kspin_lock *lock));
__fastcall static uint32_t
	InterlockedIncrement(REGARGS1(volatile uint32_t *addend));
__fastcall static uint32_t
	InterlockedDecrement(REGARGS1(volatile uint32_t *addend));
__fastcall static void
	ExInterlockedAddLargeStatistic(REGARGS2(uint64_t *addend, uint32_t));
__stdcall static uint32_t MmSizeOfMdl(void *, size_t);
__stdcall static void MmBuildMdlForNonPagedPool(mdl *);
__stdcall static void *MmMapLockedPages(mdl *, uint8_t);
__stdcall static void *MmMapLockedPagesSpecifyCache(mdl *,
	uint8_t, uint32_t, void *, uint32_t, uint32_t);
__stdcall static void MmUnmapLockedPages(void *, mdl *);
__stdcall static size_t RtlCompareMemory(const void *, const void *, size_t);
__stdcall static void RtlInitAnsiString(ndis_ansi_string *, char *);
__stdcall static void RtlInitUnicodeString(ndis_unicode_string *,
	uint16_t *);
__stdcall static void RtlFreeUnicodeString(ndis_unicode_string *);
__stdcall static void RtlFreeAnsiString(ndis_ansi_string *);
__stdcall static ndis_status RtlUnicodeStringToInteger(ndis_unicode_string *,
	uint32_t, uint32_t *);
static int atoi (const char *);
static long atol (const char *);
static int rand(void);
static void srand(unsigned int);
static void ntoskrnl_time(uint64_t *);
__stdcall static uint8_t IoIsWdmVersionAvailable(uint8_t, uint8_t);
static void ntoskrnl_thrfunc(void *);
__stdcall static ndis_status PsCreateSystemThread(ndis_handle *,
	uint32_t, void *, ndis_handle, void *, void *, void *);
__stdcall static ndis_status PsTerminateSystemThread(ndis_status);
__stdcall static ndis_status IoGetDeviceProperty(device_object *, uint32_t,
	uint32_t, void *, uint32_t *);
__stdcall static void KeInitializeMutex(kmutant *, uint32_t);
__stdcall static uint32_t KeReleaseMutex(kmutant *, uint8_t);
__stdcall static uint32_t KeReadStateMutex(kmutant *);
__stdcall static ndis_status ObReferenceObjectByHandle(ndis_handle,
	uint32_t, void *, uint8_t, void **, void **);
__fastcall static void ObfDereferenceObject(REGARGS1(void *object));
__stdcall static uint32_t ZwClose(ndis_handle);
static void *ntoskrnl_memset(void *, int, size_t);
static funcptr ntoskrnl_findwrap(funcptr);
static uint32_t DbgPrint(char *, ...);
__stdcall static void DbgBreakPoint(void);
__stdcall static void dummy(void);

static struct mtx ntoskrnl_dispatchlock;
static kspin_lock ntoskrnl_global;
static kspin_lock ntoskrnl_cancellock;
static int ntoskrnl_kth = 0;
static struct nt_objref_head ntoskrnl_reflist;
static uma_zone_t mdl_zone;

int
ntoskrnl_libinit()
{
	image_patch_table	*patch;

	mtx_init(&ntoskrnl_dispatchlock,
	    "ntoskrnl dispatch lock", MTX_NDIS_LOCK, MTX_DEF);
	KeInitializeSpinLock(&ntoskrnl_global);
	KeInitializeSpinLock(&ntoskrnl_cancellock);
	TAILQ_INIT(&ntoskrnl_reflist);

	patch = ntoskrnl_functbl;
	while (patch->ipt_func != NULL) {
		windrv_wrap((funcptr)patch->ipt_func,
		    (funcptr *)&patch->ipt_wrap);
		patch++;
	}

	/*
	 * MDLs are supposed to be variable size (they describe
	 * buffers containing some number of pages, but we don't
	 * know ahead of time how many pages that will be). But
	 * always allocating them off the heap is very slow. As
	 * a compromize, we create an MDL UMA zone big enough to
	 * handle any buffer requiring up to 16 pages, and we
	 * use those for any MDLs for buffers of 16 pages or less
	 * in size. For buffers larger than that (which we assume
	 * will be few and far between, we allocate the MDLs off
	 * the heap.
	 */

	mdl_zone = uma_zcreate("Windows MDL", MDL_ZONE_SIZE,
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

	uma_zdestroy(mdl_zone);

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

__stdcall static uint8_t 
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

__stdcall static void
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

__stdcall static ndis_status
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

__stdcall static ndis_status
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

__stdcall void *
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

__stdcall void
ExFreePool(buf)
	void			*buf;
{
	free(buf, M_DEVBUF);
	return;
}

__stdcall uint32_t
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

__stdcall void *
IoGetDriverObjectExtension(drv, clid)
	driver_object		*drv;
	void			*clid;
{
	list_entry		*e;
	custom_extension	*ce;

	e = drv->dro_driverext->dre_usrext.nle_flink;
	while (e != &drv->dro_driverext->dre_usrext) {
		ce = (custom_extension *)e;
		if (ce->ce_clid == clid)
			return((void *)(ce + 1));
		e = e->nle_flink;
	}

	return(NULL);
}


__stdcall uint32_t
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

__stdcall void
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

__stdcall device_object *
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

__stdcall static irp *
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

__stdcall static irp *
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

__stdcall static irp *
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

__stdcall static irp *
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

__stdcall static irp *
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

__stdcall static void
IoFreeIrp(ip)
	irp			*ip;
{
	ExFreePool(ip);
	return;
}

__stdcall static void
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

__stdcall static void
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

__stdcall void
IoAcquireCancelSpinLock(irql)
	uint8_t			*irql;
{
	KeAcquireSpinLock(&ntoskrnl_cancellock, irql);
	return;
}

__stdcall void
IoReleaseCancelSpinLock(irql)
	uint8_t			irql;
{
	KeReleaseSpinLock(&ntoskrnl_cancellock, irql);
	return;
}

__stdcall uint8_t
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

__fastcall uint32_t
IofCallDriver(REGARGS2(device_object *dobj, irp *ip))
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

__fastcall void
IofCompleteRequest(REGARGS2(irp *ip, uint8_t prioboost))
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
		masterirpcnt = FASTCALL1(InterlockedDecrement,
		    &masterirp->irp_assoc.irp_irpcnt);

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

__stdcall device_object *
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

__stdcall void
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

	obj->dh_sigstate = TRUE;
	e = obj->dh_waitlisthead.nle_flink;
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

__stdcall uint32_t
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

	mtx_unlock(&ntoskrnl_dispatchlock);

	return(STATUS_SUCCESS);
}

__stdcall static uint32_t
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
		for (i = 0; i < cnt; i++)
			REMOVE_LIST_ENTRY((&w[i].wb_waitlist));
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

__stdcall static void
WRITE_REGISTER_USHORT(reg, val)
	uint16_t		*reg;
	uint16_t		val;
{
	bus_space_write_2(NDIS_BUS_SPACE_MEM, 0x0, (bus_size_t)reg, val);
	return;
}

__stdcall static uint16_t
READ_REGISTER_USHORT(reg)
	uint16_t		*reg;
{
	return(bus_space_read_2(NDIS_BUS_SPACE_MEM, 0x0, (bus_size_t)reg));
}

__stdcall static void
WRITE_REGISTER_ULONG(reg, val)
	uint32_t		*reg;
	uint32_t		val;
{
	bus_space_write_4(NDIS_BUS_SPACE_MEM, 0x0, (bus_size_t)reg, val);
	return;
}

__stdcall static uint32_t
READ_REGISTER_ULONG(reg)
	uint32_t		*reg;
{
	return(bus_space_read_4(NDIS_BUS_SPACE_MEM, 0x0, (bus_size_t)reg));
}

__stdcall static uint8_t
READ_REGISTER_UCHAR(reg)
	uint8_t			*reg;
{
	return(bus_space_read_1(NDIS_BUS_SPACE_MEM, 0x0, (bus_size_t)reg));
}

__stdcall static void
WRITE_REGISTER_UCHAR(reg, val)
	uint8_t			*reg;
	uint8_t			val;
{
	bus_space_write_1(NDIS_BUS_SPACE_MEM, 0x0, (bus_size_t)reg, val);
	return;
}

__stdcall static int64_t
_allmul(a, b)
	int64_t			a;
	int64_t			b;
{
	return (a * b);
}

__stdcall static int64_t
_alldiv(a, b)
	int64_t			a;
	int64_t			b;
{
	return (a / b);
}

__stdcall static int64_t
_allrem(a, b)
	int64_t			a;
	int64_t			b;
{
	return (a % b);
}

__stdcall static uint64_t
_aullmul(a, b)
	uint64_t		a;
	uint64_t		b;
{
	return (a * b);
}

__stdcall static uint64_t
_aulldiv(a, b)
	uint64_t		a;
	uint64_t		b;
{
	return (a / b);
}

__stdcall static uint64_t
_aullrem(a, b)
	uint64_t		a;
	uint64_t		b;
{
	return (a % b);
}

__regparm static int64_t
_allshl(a, b)
	int64_t			a;
	uint8_t			b;
{
	return (a << b);
}

__regparm static uint64_t
_aullshl(a, b)
	uint64_t		a;
	uint8_t			b;
{
	return (a << b);
}

__regparm static int64_t
_allshr(a, b)
	int64_t			a;
	uint8_t			b;
{
	return (a >> b);
}

__regparm static uint64_t
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

__stdcall static void
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

__stdcall static void
ExDeletePagedLookasideList(lookaside)
	paged_lookaside_list   *lookaside;
{
	void			*buf;
	__stdcall void		(*freefunc)(void *);

	freefunc = lookaside->nll_l.gl_freefunc;
	while((buf = ntoskrnl_popsl(&lookaside->nll_l.gl_listhead)) != NULL)
		MSCALL1(freefunc, buf);

	return;
}

__stdcall static void
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

__stdcall static void
ExDeleteNPagedLookasideList(lookaside)
	npaged_lookaside_list   *lookaside;
{
	void			*buf;
	__stdcall void		(*freefunc)(void *);

	freefunc = lookaside->nll_l.gl_freefunc;
	while((buf = ntoskrnl_popsl(&lookaside->nll_l.gl_listhead)) != NULL)
		MSCALL1(freefunc, buf);

	return;
}

/*
 * Note: the interlocked slist push and pop routines are
 * declared to be _fastcall in Windows. gcc 3.4 is supposed
 * to have support for this calling convention, however we
 * don't have that version available yet, so we kludge things
 * up using __regparm__(3) and some argument shuffling.
 */

__fastcall static slist_entry *
InterlockedPushEntrySList(REGARGS2(slist_header *head, slist_entry *entry))
{
	slist_entry		*oldhead;

	oldhead = (slist_entry *)FASTCALL3(ExInterlockedPushEntrySList,
	    head, entry, &ntoskrnl_global);

	return(oldhead);
}

__fastcall static slist_entry *
InterlockedPopEntrySList(REGARGS1(slist_header *head))
{
	slist_entry		*first;

	first = (slist_entry *)FASTCALL2(ExInterlockedPopEntrySList,
	    head, &ntoskrnl_global);

	return(first);
}

__fastcall static slist_entry *
ExInterlockedPushEntrySList(REGARGS2(slist_header *head,
	slist_entry *entry), kspin_lock *lock)
{
	slist_entry		*oldhead;
	uint8_t			irql;

	KeAcquireSpinLock(lock, &irql);
	oldhead = ntoskrnl_pushsl(head, entry);
	KeReleaseSpinLock(lock, irql);

	return(oldhead);
}

__fastcall static slist_entry *
ExInterlockedPopEntrySList(REGARGS2(slist_header *head, kspin_lock *lock))
{
	slist_entry		*first;
	uint8_t			irql;

	KeAcquireSpinLock(lock, &irql);
	first = ntoskrnl_popsl(head);
	KeReleaseSpinLock(lock, irql);

	return(first);
}

/*
 * The KeInitializeSpinLock(), KefAcquireSpinLockAtDpcLevel()
 * and KefReleaseSpinLockFromDpcLevel() appear to be analagous
 * to splnet()/splx() in their use. We can't create a new mutex
 * lock here because there is no complimentary KeFreeSpinLock()
 * function. Instead, we grab a mutex from the mutex pool.
 */
__stdcall void
KeInitializeSpinLock(lock)
	kspin_lock		*lock;
{
	*lock = 0;

	return;
}

#ifdef __i386__
__fastcall void
KefAcquireSpinLockAtDpcLevel(REGARGS1(kspin_lock *lock))
{
	while (atomic_cmpset_acq_int((volatile u_int *)lock, 0, 1) == 0)
		/* sit and spin */;

	return;
}

__fastcall void
KefReleaseSpinLockFromDpcLevel(REGARGS1(kspin_lock *lock))
{
	atomic_store_rel_int((volatile u_int *)lock, 0);

	return;
}

__stdcall uint8_t
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
__stdcall void
KeAcquireSpinLockAtDpcLevel(kspin_lock *lock)
{
	while (atomic_cmpset_acq_int((volatile u_int *)lock, 0, 1) == 0)
		/* sit and spin */;

	return;
}

__stdcall void
KefReleaseSpinLockFromDpcLevel(kspin_lock *lock)
{
	atomic_store_rel_int((volatile u_int *)lock, 0);

	return;
}
#endif /* __i386__ */

__fastcall uintptr_t
InterlockedExchange(REGARGS2(volatile uint32_t *dst, uintptr_t val))
{
	uint8_t			irql;
	uintptr_t		r;

	KeAcquireSpinLock(&ntoskrnl_global, &irql);
	r = *dst;
	*dst = val;
	KeReleaseSpinLock(&ntoskrnl_global, irql);

	return(r);
}

__fastcall static uint32_t
InterlockedIncrement(REGARGS1(volatile uint32_t *addend))
{
	atomic_add_long((volatile u_long *)addend, 1);
	return(*addend);
}

__fastcall static uint32_t
InterlockedDecrement(REGARGS1(volatile uint32_t *addend))
{
	atomic_subtract_long((volatile u_long *)addend, 1);
	return(*addend);
}

__fastcall static void
ExInterlockedAddLargeStatistic(REGARGS2(uint64_t *addend, uint32_t inc))
{
	uint8_t			irql;

	KeAcquireSpinLock(&ntoskrnl_global, &irql);
	*addend += inc;
	KeReleaseSpinLock(&ntoskrnl_global, irql);

	return;
};

__stdcall mdl *
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

__stdcall void
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

__stdcall static uint32_t
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
__stdcall static void
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

__stdcall static void *
MmMapLockedPages(buf, accessmode)
	mdl			*buf;
	uint8_t			accessmode;
{
	buf->mdl_flags |= MDL_MAPPED_TO_SYSTEM_VA;
	return(MmGetMdlVirtualAddress(buf));
}

__stdcall static void *
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

__stdcall static void
MmUnmapLockedPages(vaddr, buf)
	void			*vaddr;
	mdl			*buf;
{
	buf->mdl_flags &= ~MDL_MAPPED_TO_SYSTEM_VA;
	return;
}

__stdcall static size_t
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

__stdcall static void
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

__stdcall static void
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

__stdcall ndis_status
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

__stdcall static void
RtlFreeUnicodeString(ustr)
	ndis_unicode_string	*ustr;
{
	if (ustr->us_buf == NULL)
		return;
	free(ustr->us_buf, M_DEVBUF);
	ustr->us_buf = NULL;
	return;
}

__stdcall static void
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

__stdcall static uint8_t
IoIsWdmVersionAvailable(major, minor)
	uint8_t			major;
	uint8_t			minor;
{
	if (major == WDM_MAJOR && minor == WDM_MINOR_WINXP)
		return(TRUE);
	return(FALSE);
}

__stdcall static ndis_status
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

__stdcall static void
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

__stdcall static uint32_t
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

__stdcall static uint32_t
KeReadStateMutex(kmutex)
	kmutant			*kmutex;
{
	return(kmutex->km_header.dh_sigstate);
}

__stdcall void
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

__stdcall uint32_t
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

__stdcall uint32_t
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

__stdcall void
KeClearEvent(kevent)
	nt_kevent		*kevent;
{
	kevent->k_header.dh_sigstate = FALSE;
	return;
}

__stdcall uint32_t
KeReadStateEvent(kevent)
	nt_kevent		*kevent;
{
	return(kevent->k_header.dh_sigstate);
}

__stdcall static ndis_status
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

__fastcall static void
ObfDereferenceObject(REGARGS1(void *object))
{
	nt_objref		*nr;

	nr = object;
	TAILQ_REMOVE(&ntoskrnl_reflist, nr, link);
	free(nr, M_DEVBUF);

	return;
}

__stdcall static uint32_t
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
	__stdcall uint32_t (*tfunc)(void *);
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

__stdcall static ndis_status
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
__stdcall static ndis_status
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

__stdcall static void
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

__stdcall void
KeInitializeTimer(timer)
	ktimer			*timer;
{
	if (timer == NULL)
		return;

	KeInitializeTimerEx(timer,  EVENT_TYPE_NOTIFY);

	return;
}

__stdcall void
KeInitializeTimerEx(timer, type)
	ktimer			*timer;
	uint32_t		type;
{
	if (timer == NULL)
		return;

	INIT_LIST_HEAD((&timer->k_header.dh_waitlisthead));
	timer->k_header.dh_sigstate = FALSE;
	timer->k_header.dh_inserted = FALSE;
	timer->k_header.dh_type = type;
	timer->k_header.dh_size = OTYPE_TIMER;
	callout_handle_init(&timer->k_handle);

	return;
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
	__stdcall kdpc_func	dpcfunc;
	kdpc			*dpc;
	uint8_t			irql;

	dpc = arg;
	dpcfunc = dpc->k_deferedfunc;
	irql = KeRaiseIrql(DISPATCH_LEVEL);
	MSCALL4(dpcfunc, dpc, dpc->k_deferredctx,
	    dpc->k_sysarg1, dpc->k_sysarg2);
	KeLowerIrql(irql);

	return;
}

__stdcall void
KeInitializeDpc(dpc, dpcfunc, dpcctx)
	kdpc			*dpc;
	void			*dpcfunc;
	void			*dpcctx;
{

	if (dpc == NULL)
		return;

	dpc->k_deferedfunc = dpcfunc;
	dpc->k_deferredctx = dpcctx;

	return;
}

__stdcall uint8_t
KeInsertQueueDpc(dpc, sysarg1, sysarg2)
	kdpc			*dpc;
	void			*sysarg1;
	void			*sysarg2;
{
	dpc->k_sysarg1 = sysarg1;
	dpc->k_sysarg2 = sysarg2;

	if (ndis_sched(ntoskrnl_run_dpc, dpc, NDIS_SWI))
		return(FALSE);

	return(TRUE);
}

__stdcall uint8_t
KeRemoveQueueDpc(dpc)
	kdpc			*dpc;
{
	if (ndis_unsched(ntoskrnl_run_dpc, dpc, NDIS_SWI))
		return(FALSE);

	return(TRUE);
}

__stdcall uint8_t
KeSetTimerEx(timer, duetime, period, dpc)
	ktimer			*timer;
	int64_t			duetime;
	uint32_t		period;
	kdpc			*dpc;
{
	struct timeval		tv;
	uint64_t		curtime;
	uint8_t			pending;

	mtx_lock(&ntoskrnl_dispatchlock);

	if (timer == NULL)
		return(FALSE);

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

__stdcall uint8_t
KeSetTimer(timer, duetime, dpc)
	ktimer			*timer;
	int64_t			duetime;
	kdpc			*dpc;
{
	return (KeSetTimerEx(timer, duetime, 0, dpc));
}

__stdcall uint8_t
KeCancelTimer(timer)
	ktimer			*timer;
{
	uint8_t			pending;

	if (timer == NULL)
		return(FALSE);

	mtx_lock(&ntoskrnl_dispatchlock);

	if (timer->k_header.dh_inserted == TRUE) {
		untimeout(ntoskrnl_timercall, timer, timer->k_handle);
		pending = TRUE;
	} else
		pending = KeRemoveQueueDpc(timer->k_dpc);

	mtx_unlock(&ntoskrnl_dispatchlock);

	return(pending);
}

__stdcall uint8_t
KeReadStateTimer(timer)
	ktimer			*timer;
{
	return(timer->k_header.dh_sigstate);
}

__stdcall static void
dummy()
{
	printf ("ntoskrnl dummy called...\n");
	return;
}


image_patch_table ntoskrnl_functbl[] = {
	IMPORT_FUNC(RtlCompareMemory),
	IMPORT_FUNC(RtlEqualUnicodeString),
	IMPORT_FUNC(RtlCopyUnicodeString),
	IMPORT_FUNC(RtlUnicodeStringToAnsiString),
	IMPORT_FUNC(RtlAnsiStringToUnicodeString),
	IMPORT_FUNC(RtlInitAnsiString),
	IMPORT_FUNC_MAP(RtlInitString, RtlInitAnsiString),
	IMPORT_FUNC(RtlInitUnicodeString),
	IMPORT_FUNC(RtlFreeAnsiString),
	IMPORT_FUNC(RtlFreeUnicodeString),
	IMPORT_FUNC(RtlUnicodeStringToInteger),
	IMPORT_FUNC(sprintf),
	IMPORT_FUNC(vsprintf),
	IMPORT_FUNC_MAP(_snprintf, snprintf),
	IMPORT_FUNC_MAP(_vsnprintf, vsnprintf),
	IMPORT_FUNC(DbgPrint),
	IMPORT_FUNC(DbgBreakPoint),
	IMPORT_FUNC(strncmp),
	IMPORT_FUNC(strcmp),
	IMPORT_FUNC(strncpy),
	IMPORT_FUNC(strcpy),
	IMPORT_FUNC(strlen),
	IMPORT_FUNC(memcpy),
	IMPORT_FUNC_MAP(memmove, ntoskrnl_memset),
	IMPORT_FUNC_MAP(memset, ntoskrnl_memset),
	IMPORT_FUNC(IoAllocateDriverObjectExtension),
	IMPORT_FUNC(IoGetDriverObjectExtension),
	IMPORT_FUNC(IofCallDriver),
	IMPORT_FUNC(IofCompleteRequest),
	IMPORT_FUNC(IoAcquireCancelSpinLock),
	IMPORT_FUNC(IoReleaseCancelSpinLock),
	IMPORT_FUNC(IoCancelIrp),
	IMPORT_FUNC(IoCreateDevice),
	IMPORT_FUNC(IoDeleteDevice),
	IMPORT_FUNC(IoGetAttachedDevice),
	IMPORT_FUNC(IoAttachDeviceToDeviceStack),
	IMPORT_FUNC(IoDetachDevice),
	IMPORT_FUNC(IoBuildSynchronousFsdRequest),
	IMPORT_FUNC(IoBuildAsynchronousFsdRequest),
	IMPORT_FUNC(IoBuildDeviceIoControlRequest),
	IMPORT_FUNC(IoAllocateIrp),
	IMPORT_FUNC(IoReuseIrp),
	IMPORT_FUNC(IoMakeAssociatedIrp),
	IMPORT_FUNC(IoFreeIrp),
	IMPORT_FUNC(IoInitializeIrp),
	IMPORT_FUNC(KeWaitForSingleObject),
	IMPORT_FUNC(KeWaitForMultipleObjects),
	IMPORT_FUNC(_allmul),
	IMPORT_FUNC(_alldiv),
	IMPORT_FUNC(_allrem),
	IMPORT_FUNC(_allshr),
	IMPORT_FUNC(_allshl),
	IMPORT_FUNC(_aullmul),
	IMPORT_FUNC(_aulldiv),
	IMPORT_FUNC(_aullrem),
	IMPORT_FUNC(_aullshr),
	IMPORT_FUNC(_aullshl),
	IMPORT_FUNC(atoi),
	IMPORT_FUNC(atol),
	IMPORT_FUNC(rand),
	IMPORT_FUNC(srand),
	IMPORT_FUNC(WRITE_REGISTER_USHORT),
	IMPORT_FUNC(READ_REGISTER_USHORT),
	IMPORT_FUNC(WRITE_REGISTER_ULONG),
	IMPORT_FUNC(READ_REGISTER_ULONG),
	IMPORT_FUNC(READ_REGISTER_UCHAR),
	IMPORT_FUNC(WRITE_REGISTER_UCHAR),
	IMPORT_FUNC(ExInitializePagedLookasideList),
	IMPORT_FUNC(ExDeletePagedLookasideList),
	IMPORT_FUNC(ExInitializeNPagedLookasideList),
	IMPORT_FUNC(ExDeleteNPagedLookasideList),
	IMPORT_FUNC(InterlockedPopEntrySList),
	IMPORT_FUNC(InterlockedPushEntrySList),
	IMPORT_FUNC(ExInterlockedPopEntrySList),
	IMPORT_FUNC(ExInterlockedPushEntrySList),
	IMPORT_FUNC(ExAllocatePoolWithTag),
	IMPORT_FUNC(ExFreePool),
#ifdef __i386__
	IMPORT_FUNC(KefAcquireSpinLockAtDpcLevel),
	IMPORT_FUNC(KefReleaseSpinLockFromDpcLevel),
	IMPORT_FUNC(KeAcquireSpinLockRaiseToDpc),
#else
	/*
	 * For AMD64, we can get away with just mapping
	 * KeAcquireSpinLockRaiseToDpc() directly to KfAcquireSpinLock()
	 * because the calling conventions end up being the same.
	 * On i386, we have to be careful because KfAcquireSpinLock()
	 * is _fastcall but KeAcquireSpinLockRaiseToDpc() isn't.
	 */
	IMPORT_FUNC(KeAcquireSpinLockAtDpcLevel),
	IMPORT_FUNC(KeReleaseSpinLockFromDpcLevel),
	IMPORT_FUNC_MAP(KeAcquireSpinLockRaiseToDpc, KfAcquireSpinLock),
#endif
	IMPORT_FUNC_MAP(KeReleaseSpinLock, KfReleaseSpinLock),
	IMPORT_FUNC(InterlockedIncrement),
	IMPORT_FUNC(InterlockedDecrement),
	IMPORT_FUNC(ExInterlockedAddLargeStatistic),
	IMPORT_FUNC(IoAllocateMdl),
	IMPORT_FUNC(IoFreeMdl),
	IMPORT_FUNC(MmSizeOfMdl),
	IMPORT_FUNC(MmMapLockedPages),
	IMPORT_FUNC(MmMapLockedPagesSpecifyCache),
	IMPORT_FUNC(MmUnmapLockedPages),
	IMPORT_FUNC(MmBuildMdlForNonPagedPool),
	IMPORT_FUNC(KeInitializeSpinLock),
	IMPORT_FUNC(IoIsWdmVersionAvailable),
	IMPORT_FUNC(IoGetDeviceProperty),
	IMPORT_FUNC(KeInitializeMutex),
	IMPORT_FUNC(KeReleaseMutex),
	IMPORT_FUNC(KeReadStateMutex),
	IMPORT_FUNC(KeInitializeEvent),
	IMPORT_FUNC(KeSetEvent),
	IMPORT_FUNC(KeResetEvent),
	IMPORT_FUNC(KeClearEvent),
	IMPORT_FUNC(KeReadStateEvent),
	IMPORT_FUNC(KeInitializeTimer),
	IMPORT_FUNC(KeInitializeTimerEx),
	IMPORT_FUNC(KeSetTimer),
	IMPORT_FUNC(KeSetTimerEx),
	IMPORT_FUNC(KeCancelTimer),
	IMPORT_FUNC(KeReadStateTimer),
	IMPORT_FUNC(KeInitializeDpc),
	IMPORT_FUNC(KeInsertQueueDpc),
	IMPORT_FUNC(KeRemoveQueueDpc),
	IMPORT_FUNC(ObReferenceObjectByHandle),
	IMPORT_FUNC(ObfDereferenceObject),
	IMPORT_FUNC(ZwClose),
	IMPORT_FUNC(PsCreateSystemThread),
	IMPORT_FUNC(PsTerminateSystemThread),

	/*
	 * This last entry is a catch-all for any function we haven't
	 * implemented yet. The PE import list patching routine will
	 * use it for any function that doesn't have an explicit match
	 * in this table.
	 */

	{ NULL, (FUNC)dummy, NULL },

	/* End of list. */

	{ NULL, NULL, NULL }
};
