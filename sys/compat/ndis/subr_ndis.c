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

/*
 * This file implements a translation layer between the BSD networking
 * infrasturcture and Windows(R) NDIS network driver modules. A Windows
 * NDIS driver calls into several functions in the NDIS.SYS Windows
 * kernel module and exports a table of functions designed to be called
 * by the NDIS subsystem. Using the PE loader, we can patch our own
 * versions of the NDIS routines into a given Windows driver module and
 * convince the driver that it is in fact running on Windows.
 *
 * We provide a table of all our implemented NDIS routines which is patched
 * into the driver object code. All our exported routines must use the
 * _stdcall calling convention, since that's what the Windows object code
 * expects.
 */


#include <sys/ctype.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/errno.h>

#include <sys/callout.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/timespec.h>
#include <sys/smp.h>
#include <sys/queue.h>
#include <sys/proc.h>
#include <sys/filedesc.h>
#include <sys/namei.h>
#include <sys/fcntl.h>
#include <sys/vnode.h>
#include <sys/kthread.h>
#include <sys/linker.h>
#include <sys/mount.h>
#include <sys/sysproto.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <machine/atomic.h>
#include <machine/bus_memio.h>
#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <machine/resource.h>

#include <sys/bus.h>
#include <sys/rman.h>

#include <machine/stdarg.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_ioctl.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <compat/ndis/pe_var.h>
#include <compat/ndis/resource_var.h>
#include <compat/ndis/ntoskrnl_var.h>
#include <compat/ndis/hal_var.h>
#include <compat/ndis/ndis_var.h>
#include <compat/ndis/cfg_var.h>
#include <dev/if_ndis/if_ndisvar.h>

static char ndis_filepath[MAXPATHLEN];
extern struct nd_head ndis_devhead;

SYSCTL_STRING(_hw, OID_AUTO, ndis_filepath, CTLFLAG_RW, ndis_filepath,
        MAXPATHLEN, "Path used by NdisOpenFile() to search for files");

__stdcall static void NdisInitializeWrapper(ndis_handle *,
	driver_object *, void *, void *);
__stdcall static ndis_status NdisMRegisterMiniport(ndis_handle,
	ndis_miniport_characteristics *, int);
__stdcall static ndis_status NdisAllocateMemoryWithTag(void **,
	uint32_t, uint32_t);
__stdcall static ndis_status NdisAllocateMemory(void **,
	uint32_t, uint32_t, ndis_physaddr);
__stdcall static void NdisFreeMemory(void *, uint32_t, uint32_t);
__stdcall static ndis_status NdisMSetAttributesEx(ndis_handle, ndis_handle,
	uint32_t, uint32_t, ndis_interface_type);
__stdcall static void NdisOpenConfiguration(ndis_status *,
	ndis_handle *, ndis_handle);
__stdcall static void NdisOpenConfigurationKeyByIndex(ndis_status *,
	ndis_handle, uint32_t, ndis_unicode_string *, ndis_handle *);
__stdcall static void NdisOpenConfigurationKeyByName(ndis_status *,
	ndis_handle, ndis_unicode_string *, ndis_handle *);
static ndis_status ndis_encode_parm(ndis_miniport_block *,
	struct sysctl_oid *, ndis_parm_type, ndis_config_parm **);
static ndis_status ndis_decode_parm(ndis_miniport_block *,
	ndis_config_parm *, char *);
__stdcall static void NdisReadConfiguration(ndis_status *, ndis_config_parm **,
	ndis_handle, ndis_unicode_string *, ndis_parm_type);
__stdcall static void NdisWriteConfiguration(ndis_status *, ndis_handle,
	ndis_unicode_string *, ndis_config_parm *);
__stdcall static void NdisCloseConfiguration(ndis_handle);
__stdcall static void NdisAllocateSpinLock(ndis_spin_lock *);
__stdcall static void NdisFreeSpinLock(ndis_spin_lock *);
__stdcall static void NdisAcquireSpinLock(ndis_spin_lock *);
__stdcall static void NdisReleaseSpinLock(ndis_spin_lock *);
__stdcall static void NdisDprAcquireSpinLock(ndis_spin_lock *);
__stdcall static void NdisDprReleaseSpinLock(ndis_spin_lock *);
__stdcall static uint32_t NdisReadPciSlotInformation(ndis_handle, uint32_t,
	uint32_t, void *, uint32_t);
__stdcall static uint32_t NdisWritePciSlotInformation(ndis_handle, uint32_t,
	uint32_t, void *, uint32_t);
static void NdisWriteErrorLogEntry(ndis_handle, ndis_error_code, uint32_t, ...);
static void ndis_map_cb(void *, bus_dma_segment_t *, int, int);
__stdcall static void NdisMStartBufferPhysicalMapping(ndis_handle,
	ndis_buffer *, uint32_t, uint8_t, ndis_paddr_unit *, uint32_t *);
__stdcall static void NdisMCompleteBufferPhysicalMapping(ndis_handle,
	ndis_buffer *, uint32_t);
__stdcall static void NdisMInitializeTimer(ndis_miniport_timer *, ndis_handle,
	ndis_timer_function, void *);
__stdcall static void NdisInitializeTimer(ndis_timer *,
	ndis_timer_function, void *);
__stdcall static void NdisSetTimer(ndis_timer *, uint32_t);
__stdcall static void NdisMSetPeriodicTimer(ndis_miniport_timer *, uint32_t);
__stdcall static void NdisMCancelTimer(ndis_timer *, uint8_t *);
__stdcall static void ndis_timercall(kdpc *, ndis_miniport_timer *,
	void *, void *);
__stdcall static void NdisMQueryAdapterResources(ndis_status *, ndis_handle,
	ndis_resource_list *, uint32_t *);
__stdcall static ndis_status NdisMRegisterIoPortRange(void **,
	ndis_handle, uint32_t, uint32_t);
__stdcall static void NdisMDeregisterIoPortRange(ndis_handle,
	uint32_t, uint32_t, void *);
__stdcall static void NdisReadNetworkAddress(ndis_status *, void **,
	uint32_t *, ndis_handle);
__stdcall static ndis_status NdisQueryMapRegisterCount(uint32_t, uint32_t *);
__stdcall static ndis_status NdisMAllocateMapRegisters(ndis_handle,
	uint32_t, uint8_t, uint32_t, uint32_t);
__stdcall static void NdisMFreeMapRegisters(ndis_handle);
static void ndis_mapshared_cb(void *, bus_dma_segment_t *, int, int);
__stdcall static void NdisMAllocateSharedMemory(ndis_handle, uint32_t,
	uint8_t, void **, ndis_physaddr *);
static void ndis_asyncmem_complete(void *);
__stdcall static ndis_status NdisMAllocateSharedMemoryAsync(ndis_handle,
	uint32_t, uint8_t, void *);
__stdcall static void NdisMFreeSharedMemory(ndis_handle, uint32_t,
	uint8_t, void *, ndis_physaddr);
__stdcall static ndis_status NdisMMapIoSpace(void **, ndis_handle,
	ndis_physaddr, uint32_t);
__stdcall static void NdisMUnmapIoSpace(ndis_handle, void *, uint32_t);
__stdcall static uint32_t NdisGetCacheFillSize(void);
__stdcall static uint32_t NdisMGetDmaAlignment(ndis_handle);
__stdcall static ndis_status NdisMInitializeScatterGatherDma(ndis_handle,
	uint8_t, uint32_t);
__stdcall static void NdisUnchainBufferAtFront(ndis_packet *, ndis_buffer **);
__stdcall static void NdisUnchainBufferAtBack(ndis_packet *, ndis_buffer **);
__stdcall static void NdisAllocateBufferPool(ndis_status *,
	ndis_handle *, uint32_t);
__stdcall static void NdisFreeBufferPool(ndis_handle);
__stdcall static void NdisAllocateBuffer(ndis_status *, ndis_buffer **,
	ndis_handle, void *, uint32_t);
__stdcall static void NdisFreeBuffer(ndis_buffer *);
__stdcall static uint32_t NdisBufferLength(ndis_buffer *);
__stdcall static void NdisQueryBuffer(ndis_buffer *, void **, uint32_t *);
__stdcall static void NdisQueryBufferSafe(ndis_buffer *, void **,
	uint32_t *, uint32_t);
__stdcall static void *NdisBufferVirtualAddress(ndis_buffer *);
__stdcall static void *NdisBufferVirtualAddressSafe(ndis_buffer *, uint32_t);
__stdcall static void NdisAdjustBufferLength(ndis_buffer *, int);
__stdcall static uint32_t NdisInterlockedIncrement(uint32_t *);
__stdcall static uint32_t NdisInterlockedDecrement(uint32_t *);
__stdcall static void NdisInitializeEvent(ndis_event *);
__stdcall static void NdisSetEvent(ndis_event *);
__stdcall static void NdisResetEvent(ndis_event *);
__stdcall static uint8_t NdisWaitEvent(ndis_event *, uint32_t);
__stdcall static ndis_status NdisUnicodeStringToAnsiString(ndis_ansi_string *,
	ndis_unicode_string *);
__stdcall static ndis_status
	NdisAnsiStringToUnicodeString(ndis_unicode_string *,
	ndis_ansi_string *);
__stdcall static ndis_status NdisMPciAssignResources(ndis_handle,
	uint32_t, ndis_resource_list **);
__stdcall static ndis_status NdisMRegisterInterrupt(ndis_miniport_interrupt *,
	ndis_handle, uint32_t, uint32_t, uint8_t,
	uint8_t, ndis_interrupt_mode);
__stdcall static void NdisMDeregisterInterrupt(ndis_miniport_interrupt *);
__stdcall static void NdisMRegisterAdapterShutdownHandler(ndis_handle, void *,
	ndis_shutdown_handler);
__stdcall static void NdisMDeregisterAdapterShutdownHandler(ndis_handle);
__stdcall static uint32_t NDIS_BUFFER_TO_SPAN_PAGES(ndis_buffer *);
__stdcall static void NdisGetBufferPhysicalArraySize(ndis_buffer *,
	uint32_t *);
__stdcall static void NdisQueryBufferOffset(ndis_buffer *,
	uint32_t *, uint32_t *);
__stdcall static void NdisMSleep(uint32_t);
__stdcall static uint32_t NdisReadPcmciaAttributeMemory(ndis_handle,
	uint32_t, void *, uint32_t);
__stdcall static uint32_t NdisWritePcmciaAttributeMemory(ndis_handle,
	uint32_t, void *, uint32_t);
__stdcall static list_entry *NdisInterlockedInsertHeadList(list_entry *,
	list_entry *, ndis_spin_lock *);
__stdcall static list_entry *NdisInterlockedRemoveHeadList(list_entry *,
	ndis_spin_lock *);
__stdcall static list_entry *NdisInterlockedInsertTailList(list_entry *,
	list_entry *, ndis_spin_lock *);
__stdcall static uint8_t
	NdisMSynchronizeWithInterrupt(ndis_miniport_interrupt *,
	void *, void *);
__stdcall static void NdisGetCurrentSystemTime(uint64_t *);
__stdcall static void NdisGetSystemUpTime(uint32_t *);
__stdcall static void NdisInitializeString(ndis_unicode_string *, char *);
__stdcall static void NdisInitAnsiString(ndis_ansi_string *, char *);
__stdcall static void NdisInitUnicodeString(ndis_unicode_string *,
	uint16_t *);
__stdcall static void NdisFreeString(ndis_unicode_string *);
__stdcall static ndis_status NdisMRemoveMiniport(ndis_handle *);
__stdcall static void NdisTerminateWrapper(ndis_handle, void *);
__stdcall static void NdisMGetDeviceProperty(ndis_handle, device_object **,
	device_object **, device_object **, cm_resource_list *,
	cm_resource_list *);
__stdcall static void NdisGetFirstBufferFromPacket(ndis_packet *,
	ndis_buffer **, void **, uint32_t *, uint32_t *);
__stdcall static void NdisGetFirstBufferFromPacketSafe(ndis_packet *,
	ndis_buffer **, void **, uint32_t *, uint32_t *, uint32_t);
static int ndis_find_sym(linker_file_t, char *, char *, caddr_t *);
__stdcall static void NdisOpenFile(ndis_status *, ndis_handle *, uint32_t *,
	ndis_unicode_string *, ndis_physaddr);
__stdcall static void NdisMapFile(ndis_status *, void **, ndis_handle);
__stdcall static void NdisUnmapFile(ndis_handle);
__stdcall static void NdisCloseFile(ndis_handle);
__stdcall static uint8_t NdisSystemProcessorCount(void);
__stdcall static void NdisMIndicateStatusComplete(ndis_handle);
__stdcall static void NdisMIndicateStatus(ndis_handle, ndis_status,
        void *, uint32_t);
static void ndis_workfunc(void *);
static funcptr ndis_findwrap(funcptr);
__stdcall static ndis_status NdisScheduleWorkItem(ndis_work_item *);
__stdcall static void NdisCopyFromPacketToPacket(ndis_packet *,
	uint32_t, uint32_t, ndis_packet *, uint32_t, uint32_t *);
__stdcall static void NdisCopyFromPacketToPacketSafe(ndis_packet *,
	uint32_t, uint32_t, ndis_packet *, uint32_t, uint32_t *, uint32_t);
__stdcall static ndis_status NdisMRegisterDevice(ndis_handle,
	ndis_unicode_string *, ndis_unicode_string *, driver_dispatch **,
	void **, ndis_handle *);
__stdcall static ndis_status NdisMDeregisterDevice(ndis_handle);
__stdcall static ndis_status
	NdisMQueryAdapterInstanceName(ndis_unicode_string *,
	ndis_handle);
__stdcall static void NdisMRegisterUnloadHandler(ndis_handle, void *);
__stdcall static void dummy(void);

/*
 * Some really old drivers do not properly check the return value
 * from NdisAllocatePacket() and NdisAllocateBuffer() and will
 * sometimes allocate few more buffers/packets that they originally
 * requested when they created the pool. To prevent this from being
 * a problem, we allocate a few extra buffers/packets beyond what
 * the driver asks for. This #define controls how many.
 */
#define NDIS_POOL_EXTRA		16

int
ndis_libinit()
{
	image_patch_table	*patch;

	strcpy(ndis_filepath, "/compat/ndis");

	patch = ndis_functbl;
	while (patch->ipt_func != NULL) {
		windrv_wrap((funcptr)patch->ipt_func,
		    (funcptr *)&patch->ipt_wrap);
		patch++;
	}

	return(0);
}

int
ndis_libfini()
{
	image_patch_table	*patch;

	patch = ndis_functbl;
	while (patch->ipt_func != NULL) {
		windrv_unwrap(patch->ipt_wrap);
		patch++;
	}

	return(0);
}

static funcptr
ndis_findwrap(func)
	funcptr			func;
{
	image_patch_table	*patch;

	patch = ndis_functbl;
	while (patch->ipt_func != NULL) {
		if ((funcptr)patch->ipt_func == func)
			return((funcptr)patch->ipt_wrap);
		patch++;
	}

	return(NULL);
}

/*
 * NDIS deals with strings in unicode format, so we have
 * do deal with them that way too. For now, we only handle
 * conversion between unicode and ASCII since that's all
 * that device drivers care about.
 */

int
ndis_ascii_to_unicode(ascii, unicode)
	char			*ascii;
	uint16_t		**unicode;
{
	uint16_t		*ustr;
	int			i;

	if (*unicode == NULL)
		*unicode = malloc(strlen(ascii) * 2, M_DEVBUF, M_NOWAIT);

	if (*unicode == NULL)
		return(ENOMEM);
	ustr = *unicode;
	for (i = 0; i < strlen(ascii); i++) {
		*ustr = (uint16_t)ascii[i];
		ustr++;
	}

	return(0);
}

int
ndis_unicode_to_ascii(unicode, ulen, ascii)
	uint16_t		*unicode;
	int			ulen;
	char			**ascii;
{
	uint8_t			*astr;
	int			i;

	if (*ascii == NULL)
		*ascii = malloc((ulen / 2) + 1, M_DEVBUF, M_NOWAIT|M_ZERO);
	if (*ascii == NULL)
		return(ENOMEM);
	astr = *ascii;
	for (i = 0; i < ulen / 2; i++) {
		*astr = (uint8_t)unicode[i];
		astr++;
	}

	return(0);
}

/*
 * This routine does the messy Windows Driver Model device attachment
 * stuff on behalf of NDIS drivers. We register our own AddDevice
 * routine here
 */
__stdcall static void
NdisInitializeWrapper(wrapper, drv, path, unused)
	ndis_handle		*wrapper;
	driver_object		*drv;
	void			*path;
	void			*unused;
{
	/*
	 * As of yet, I haven't come up with a compelling
	 * reason to define a private NDIS wrapper structure,
	 * so we use a pointer to the driver object as the
	 * wrapper handle. The driver object has the miniport
	 * characteristics struct for this driver hung off it
	 * via IoAllocateDriverObjectExtension(), and that's
	 * really all the private data we need.
	 */

	*wrapper = drv;

	/*
	 * If this was really Windows, we'd be registering dispatch
	 * routines for the NDIS miniport module here, but we're
	 * not Windows so all we really need to do is set up an
	 * AddDevice function that'll be invoked when a new device
	 * instance appears.
	 */

	drv->dro_driverext->dre_adddevicefunc = NdisAddDevice;

	return;
}

__stdcall static void
NdisTerminateWrapper(handle, syspec)
	ndis_handle		handle;
	void			*syspec;
{
	/* Nothing to see here, move along. */
	return;
}

__stdcall static ndis_status
NdisMRegisterMiniport(handle, characteristics, len)
	ndis_handle		handle;
	ndis_miniport_characteristics *characteristics;
	int			len;
{
	ndis_miniport_characteristics	*ch = NULL;
	driver_object		*drv;

	drv = (driver_object *)handle;

	/*
	 * We need to save the NDIS miniport characteristics
	 * somewhere. This data is per-driver, not per-device
	 * (all devices handled by the same driver have the
	 * same characteristics) so we hook it onto the driver
	 * object using IoAllocateDriverObjectExtension().
	 * The extra extension info is automagically deleted when
	 * the driver is unloaded (see windrv_unload()).
	 */

	if (IoAllocateDriverObjectExtension(drv, (void *)1,
	    sizeof(ndis_miniport_characteristics), (void **)&ch) !=
	    STATUS_SUCCESS)
		return(NDIS_STATUS_RESOURCES);

	bzero((char *)ch, sizeof(ndis_miniport_characteristics));

	bcopy((char *)characteristics, (char *)ch, len);

	if (ch->nmc_version_major < 5 || ch->nmc_version_minor < 1) {
		ch->nmc_shutdown_handler = NULL;
		ch->nmc_canceltxpkts_handler = NULL;
		ch->nmc_pnpevent_handler = NULL;
	}

	return(NDIS_STATUS_SUCCESS);
}

__stdcall static ndis_status
NdisAllocateMemoryWithTag(vaddr, len, tag)
	void			**vaddr;
	uint32_t		len;
	uint32_t		tag;
{
	void			*mem;


	mem = ExAllocatePoolWithTag(NonPagedPool, len, tag);
	if (mem == NULL)
		return(NDIS_STATUS_RESOURCES);
	*vaddr = mem;

	return(NDIS_STATUS_SUCCESS);
}

__stdcall static ndis_status
NdisAllocateMemory(vaddr, len, flags, highaddr)
	void			**vaddr;
	uint32_t		len;
	uint32_t		flags;
	ndis_physaddr		highaddr;
{
	void			*mem;

	mem = ExAllocatePoolWithTag(NonPagedPool, len, 0);
	if (mem == NULL)
		return(NDIS_STATUS_RESOURCES);
	*vaddr = mem;

	return(NDIS_STATUS_SUCCESS);
}

__stdcall static void
NdisFreeMemory(vaddr, len, flags)
	void			*vaddr;
	uint32_t		len;
	uint32_t		flags;
{
	if (len == 0)
		return;

	ExFreePool(vaddr);

	return;
}

__stdcall static ndis_status
NdisMSetAttributesEx(adapter_handle, adapter_ctx, hangsecs,
			flags, iftype)
	ndis_handle			adapter_handle;
	ndis_handle			adapter_ctx;
	uint32_t			hangsecs;
	uint32_t			flags;
	ndis_interface_type		iftype;
{
	ndis_miniport_block		*block;

	/*
	 * Save the adapter context, we need it for calling
	 * the driver's internal functions.
	 */
	block = (ndis_miniport_block *)adapter_handle;
	block->nmb_miniportadapterctx = adapter_ctx;
	block->nmb_checkforhangsecs = hangsecs;
	block->nmb_flags = flags;

	return(NDIS_STATUS_SUCCESS);
}

__stdcall static void
NdisOpenConfiguration(status, cfg, wrapctx)
	ndis_status		*status;
	ndis_handle		*cfg;
	ndis_handle		wrapctx;
{

	*cfg = wrapctx;
	*status = NDIS_STATUS_SUCCESS;

	return;
}

__stdcall static void
NdisOpenConfigurationKeyByName(status, cfg, subkey, subhandle)
	ndis_status		*status;
	ndis_handle		cfg;
	ndis_unicode_string	*subkey;
	ndis_handle		*subhandle;
{
	*subhandle = cfg;
	*status = NDIS_STATUS_SUCCESS;
	return;
}

__stdcall static void
NdisOpenConfigurationKeyByIndex(status, cfg, idx, subkey, subhandle)
	ndis_status		*status;
	ndis_handle		cfg;
	uint32_t		idx;
	ndis_unicode_string	*subkey;
	ndis_handle		*subhandle;
{
	*status = NDIS_STATUS_FAILURE;
	return;
}

static ndis_status
ndis_encode_parm(block, oid, type, parm)
	ndis_miniport_block	*block;
        struct sysctl_oid	*oid;
	ndis_parm_type		type;
	ndis_config_parm	**parm;
{
	uint16_t		*unicode;
	ndis_unicode_string	*ustr;
	int			base = 0;

	unicode = (uint16_t *)&block->nmb_dummybuf;

	switch(type) {
	case ndis_parm_string:
		ndis_ascii_to_unicode((char *)oid->oid_arg1, &unicode);
		(*parm)->ncp_type = ndis_parm_string;
		ustr = &(*parm)->ncp_parmdata.ncp_stringdata;
		ustr->us_len = strlen((char *)oid->oid_arg1) * 2;
		ustr->us_buf = unicode;
		break;
	case ndis_parm_int:
		if (strncmp((char *)oid->oid_arg1, "0x", 2) == 0)
			base = 16;
		else
			base = 10;
		(*parm)->ncp_type = ndis_parm_int;
		(*parm)->ncp_parmdata.ncp_intdata =
		    strtol((char *)oid->oid_arg1, NULL, base);
		break;
	case ndis_parm_hexint:
		if (strncmp((char *)oid->oid_arg1, "0x", 2) == 0)
			base = 16;
		else
			base = 10;
		(*parm)->ncp_type = ndis_parm_hexint;
		(*parm)->ncp_parmdata.ncp_intdata =
		    strtoul((char *)oid->oid_arg1, NULL, base);
		break;
	default:
		return(NDIS_STATUS_FAILURE);
		break;
	}

	return(NDIS_STATUS_SUCCESS);
}

int
ndis_strcasecmp(s1, s2)
        const char              *s1;
        const char              *s2;
{
	char			a, b;

	/*
	 * In the kernel, toupper() is a macro. Have to be careful
	 * not to use pointer arithmetic when passing it arguments.
	 */

	while(1) {
		a = *s1;
		b = *s2++;
		if (toupper(a) != toupper(b))
			break;
		if (*s1++ == '\0')
			return(0);
	}

	return (*(const unsigned char *)s1 - *(const unsigned char *)(s2 - 1));
}

int
ndis_strncasecmp(s1, s2, n)
        const char              *s1;
        const char              *s2;
	size_t			n;
{
	char			a, b;

	if (n != 0) {
		do {
			a = *s1;
			b = *s2++;
			if (toupper(a) != toupper(b))
				return (*(const unsigned char *)s1 -
				    *(const unsigned char *)(s2 - 1));
			if (*s1++ == '\0')
				break;
		} while (--n != 0);
	}

	return(0);
}

__stdcall static void
NdisReadConfiguration(status, parm, cfg, key, type)
	ndis_status		*status;
	ndis_config_parm	**parm;
	ndis_handle		cfg;
	ndis_unicode_string	*key;
	ndis_parm_type		type;
{
	char			*keystr = NULL;
	uint16_t		*unicode;
	ndis_miniport_block	*block;
	struct ndis_softc	*sc;
        struct sysctl_oid	*oidp;
	struct sysctl_ctx_entry	*e;

	block = (ndis_miniport_block *)cfg;
	sc = device_get_softc(block->nmb_physdeviceobj->do_devext);

	if (key->us_len == 0 || key->us_buf == NULL) {
		*status = NDIS_STATUS_FAILURE;
		return;
	}

	ndis_unicode_to_ascii(key->us_buf, key->us_len, &keystr);
	*parm = &block->nmb_replyparm;
	bzero((char *)&block->nmb_replyparm, sizeof(ndis_config_parm));
	unicode = (uint16_t *)&block->nmb_dummybuf;

	/*
	 * See if registry key is already in a list of known keys
	 * included with the driver.
	 */
#if __FreeBSD_version < 502113
	TAILQ_FOREACH(e, &sc->ndis_ctx, link) {
#else
	TAILQ_FOREACH(e, device_get_sysctl_ctx(sc->ndis_dev), link) {
#endif
		oidp = e->entry;
		if (ndis_strcasecmp(oidp->oid_name, keystr) == 0) {
			if (strcmp((char *)oidp->oid_arg1, "UNSET") == 0) {
				free(keystr, M_DEVBUF);
				*status = NDIS_STATUS_FAILURE;
				return;
			}
			*status = ndis_encode_parm(block, oidp, type, parm);
			free(keystr, M_DEVBUF);
			return;
		}
	}

	/*
	 * If the key didn't match, add it to the list of dynamically
	 * created ones. Sometimes, drivers refer to registry keys
	 * that aren't documented in their .INF files. These keys
	 * are supposed to be created by some sort of utility or
	 * control panel snap-in that comes with the driver software.
	 * Sometimes it's useful to be able to manipulate these.
	 * If the driver requests the key in the form of a string,
	 * make its default value an empty string, otherwise default
	 * it to "0".
	 */

	if (type == ndis_parm_int || type == ndis_parm_hexint)
		ndis_add_sysctl(sc, keystr, "(dynamic integer key)",
		    "UNSET", CTLFLAG_RW);
	else
		ndis_add_sysctl(sc, keystr, "(dynamic string key)",
		    "UNSET", CTLFLAG_RW);

	free(keystr, M_DEVBUF);
	*status = NDIS_STATUS_FAILURE;
	return;
}

static ndis_status
ndis_decode_parm(block, parm, val)
	ndis_miniport_block	*block;
	ndis_config_parm	*parm;
	char			*val;
{
	ndis_unicode_string	*ustr;
	char			*astr = NULL;

	switch(parm->ncp_type) {
	case ndis_parm_string:
		ustr = &parm->ncp_parmdata.ncp_stringdata;
		ndis_unicode_to_ascii(ustr->us_buf, ustr->us_len, &astr);
		bcopy(astr, val, 254);
		free(astr, M_DEVBUF);
		break;
	case ndis_parm_int:
		sprintf(val, "%d", parm->ncp_parmdata.ncp_intdata);
		break;
	case ndis_parm_hexint:
		sprintf(val, "%xu", parm->ncp_parmdata.ncp_intdata);
		break;
	default:
		return(NDIS_STATUS_FAILURE);
		break;
	}
	return(NDIS_STATUS_SUCCESS);
}

__stdcall static void
NdisWriteConfiguration(status, cfg, key, parm)
	ndis_status		*status;
	ndis_handle		cfg;
	ndis_unicode_string	*key;
	ndis_config_parm	*parm;
{
	char			*keystr = NULL;
	ndis_miniport_block	*block;
	struct ndis_softc	*sc;
        struct sysctl_oid	*oidp;
	struct sysctl_ctx_entry	*e;
	char			val[256];

	block = (ndis_miniport_block *)cfg;
	sc = device_get_softc(block->nmb_physdeviceobj->do_devext);

	ndis_unicode_to_ascii(key->us_buf, key->us_len, &keystr);

	/* Decode the parameter into a string. */
	bzero(val, sizeof(val));
	*status = ndis_decode_parm(block, parm, val);
	if (*status != NDIS_STATUS_SUCCESS) {
		free(keystr, M_DEVBUF);
		return;
	}

	/* See if the key already exists. */

#if __FreeBSD_version < 502113
	TAILQ_FOREACH(e, &sc->ndis_ctx, link) {
#else
	TAILQ_FOREACH(e, device_get_sysctl_ctx(sc->ndis_dev), link) {
#endif
		oidp = e->entry;
		if (ndis_strcasecmp(oidp->oid_name, keystr) == 0) {
			/* Found it, set the value. */
			strcpy((char *)oidp->oid_arg1, val);
			free(keystr, M_DEVBUF);
			return;
		}
	}

	/* Not found, add a new key with the specified value. */
	ndis_add_sysctl(sc, keystr, "(dynamically set key)",
		    val, CTLFLAG_RW);

	free(keystr, M_DEVBUF);
	*status = NDIS_STATUS_SUCCESS;
	return;
}

__stdcall static void
NdisCloseConfiguration(cfg)
	ndis_handle		cfg;
{
	return;
}

/*
 * Initialize a Windows spinlock.
 */
__stdcall static void
NdisAllocateSpinLock(lock)
	ndis_spin_lock		*lock;
{
	KeInitializeSpinLock(&lock->nsl_spinlock);
	lock->nsl_kirql = 0;

	return;
}

/*
 * Destroy a Windows spinlock. This is a no-op for now. There are two reasons
 * for this. One is that it's sort of superfluous: we don't have to do anything
 * special to deallocate the spinlock. The other is that there are some buggy
 * drivers which call NdisFreeSpinLock() _after_ calling NdisFreeMemory() on
 * the block of memory in which the spinlock resides. (Yes, ADMtek, I'm
 * talking to you.)
 */
__stdcall static void
NdisFreeSpinLock(lock)
	ndis_spin_lock		*lock;
{
#ifdef notdef
	KeInitializeSpinLock(&lock->nsl_spinlock);
	lock->nsl_kirql = 0;
#endif
	return;
}

/*
 * Acquire a spinlock from IRQL <= DISPATCH_LEVEL.
 */

__stdcall static void
NdisAcquireSpinLock(lock)
	ndis_spin_lock		*lock;
{
	KeAcquireSpinLock(&lock->nsl_spinlock, &lock->nsl_kirql);
	return;
}

/*
 * Release a spinlock from IRQL == DISPATCH_LEVEL.
 */

__stdcall static void
NdisReleaseSpinLock(lock)
	ndis_spin_lock		*lock;
{
	KeReleaseSpinLock(&lock->nsl_spinlock, lock->nsl_kirql);
	return;
}

/*
 * Acquire a spinlock when already running at IRQL == DISPATCH_LEVEL.
 */
__stdcall static void
NdisDprAcquireSpinLock(lock)
	ndis_spin_lock		*lock;
{
	KeAcquireSpinLockAtDpcLevel(&lock->nsl_spinlock);
	return;
}

/*
 * Release a spinlock without leaving IRQL == DISPATCH_LEVEL.
 */
__stdcall static void
NdisDprReleaseSpinLock(lock)
	ndis_spin_lock		*lock;
{
	KeReleaseSpinLockFromDpcLevel(&lock->nsl_spinlock);
	return;
}

__stdcall static uint32_t
NdisReadPciSlotInformation(adapter, slot, offset, buf, len)
	ndis_handle		adapter;
	uint32_t		slot;
	uint32_t		offset;
	void			*buf;
	uint32_t		len;
{
	ndis_miniport_block	*block;
	int			i;
	char			*dest;
	device_t		dev;

	block = (ndis_miniport_block *)adapter;
	dest = buf;
	if (block == NULL)
		return(0);

	dev = block->nmb_physdeviceobj->do_devext;

	/*
	 * I have a test system consisting of a Sun w2100z
	 * dual 2.4Ghz Opteron machine and an Atheros 802.11a/b/g
	 * "Aries" miniPCI NIC. (The NIC is installed in the
	 * machine using a miniPCI to PCI bus adapter card.)
	 * When running in SMP mode, I found that
	 * performing a large number of consecutive calls to
	 * NdisReadPciSlotInformation() would result in a
	 * sudden system reset (or in some cases a freeze).
	 * My suspicion is that the multiple reads are somehow
	 * triggering a fatal PCI bus error that leads to a
	 * machine check. The 1us delay in the loop below
	 * seems to prevent this problem.
	 */

	for (i = 0; i < len; i++) {
		DELAY(1);
		dest[i] = pci_read_config(dev, i + offset, 1);
	}

	return(len);
}

__stdcall static uint32_t
NdisWritePciSlotInformation(adapter, slot, offset, buf, len)
	ndis_handle		adapter;
	uint32_t		slot;
	uint32_t		offset;
	void			*buf;
	uint32_t		len;
{
	ndis_miniport_block	*block;
	int			i;
	char			*dest;
	device_t		dev;

	block = (ndis_miniport_block *)adapter;
	dest = buf;

	if (block == NULL)
		return(0);

	dev = block->nmb_physdeviceobj->do_devext;
	for (i = 0; i < len; i++) {
		DELAY(1);
		pci_write_config(dev, i + offset, dest[i], 1);
	}

	return(len);
}

/*
 * The errorlog routine uses a variable argument list, so we
 * have to declare it this way.
 */
#define ERRMSGLEN 512
static void
NdisWriteErrorLogEntry(ndis_handle adapter, ndis_error_code code,
	uint32_t numerrors, ...)
{
	ndis_miniport_block	*block;
	va_list			ap;
	int			i, error;
	char			*str = NULL, *ustr = NULL;
	uint16_t		flags;
	char			msgbuf[ERRMSGLEN];
	device_t		dev;
	driver_object		*drv;

	block = (ndis_miniport_block *)adapter;
	dev = block->nmb_physdeviceobj->do_devext;
	drv = block->nmb_physdeviceobj->do_drvobj;

	error = pe_get_message((vm_offset_t)drv->dro_driverstart,
	    code, &str, &i, &flags);
	if (error == 0 && flags & MESSAGE_RESOURCE_UNICODE) {
		ustr = msgbuf;
		ndis_unicode_to_ascii((uint16_t *)str,
		    ((i / 2)) > (ERRMSGLEN - 1) ? ERRMSGLEN : i, &ustr);
		str = ustr;
	}
	device_printf (dev, "NDIS ERROR: %x (%s)\n", code,
	    str == NULL ? "unknown error" : str);
	device_printf (dev, "NDIS NUMERRORS: %x\n", numerrors);

	va_start(ap, numerrors);
	for (i = 0; i < numerrors; i++)
		device_printf (dev, "argptr: %p\n",
		    va_arg(ap, void *));
	va_end(ap);

	return;
}

static void
ndis_map_cb(arg, segs, nseg, error)
	void			*arg;
	bus_dma_segment_t	*segs;
	int			nseg;
	int			error;
{
	struct ndis_map_arg	*ctx;
	int			i;

	if (error)
		return;

	ctx = arg;

	for (i = 0; i < nseg; i++) {
		ctx->nma_fraglist[i].npu_physaddr.np_quad = segs[i].ds_addr;
		ctx->nma_fraglist[i].npu_len = segs[i].ds_len;
	}

	ctx->nma_cnt = nseg;

	return;
}

__stdcall static void
NdisMStartBufferPhysicalMapping(adapter, buf, mapreg, writedev, addrarray, arraysize)
	ndis_handle		adapter;
	ndis_buffer		*buf;
	uint32_t		mapreg;
	uint8_t			writedev;
	ndis_paddr_unit		*addrarray;
	uint32_t		*arraysize;
{
	ndis_miniport_block	*block;
	struct ndis_softc	*sc;
	struct ndis_map_arg	nma;
	bus_dmamap_t		map;
	int			error;

	if (adapter == NULL)
		return;

	block = (ndis_miniport_block *)adapter;
	sc = device_get_softc(block->nmb_physdeviceobj->do_devext);

	if (mapreg > sc->ndis_mmapcnt)
		return;

	map = sc->ndis_mmaps[mapreg];
	nma.nma_fraglist = addrarray;

	error = bus_dmamap_load(sc->ndis_mtag, map,
	    MmGetMdlVirtualAddress(buf), MmGetMdlByteCount(buf), ndis_map_cb,
	    (void *)&nma, BUS_DMA_NOWAIT);

	if (error)
		return;

	bus_dmamap_sync(sc->ndis_mtag, map,
	    writedev ? BUS_DMASYNC_PREWRITE : BUS_DMASYNC_PREREAD);

	*arraysize = nma.nma_cnt;

	return;
}

__stdcall static void
NdisMCompleteBufferPhysicalMapping(adapter, buf, mapreg)
	ndis_handle		adapter;
	ndis_buffer		*buf;
	uint32_t		mapreg;
{
	ndis_miniport_block	*block;
	struct ndis_softc	*sc;
	bus_dmamap_t		map;

	if (adapter == NULL)
		return;

	block = (ndis_miniport_block *)adapter;
	sc = device_get_softc(block->nmb_physdeviceobj->do_devext);

	if (mapreg > sc->ndis_mmapcnt)
		return;

	map = sc->ndis_mmaps[mapreg];

	bus_dmamap_sync(sc->ndis_mtag, map,
	    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

	bus_dmamap_unload(sc->ndis_mtag, map);

	return;
}

/*
 * This is an older (?) timer init routine which doesn't
 * accept a miniport context handle. Serialized miniports should
 * never call this function.
 */

__stdcall static void
NdisInitializeTimer(timer, func, ctx)
	ndis_timer		*timer;
	ndis_timer_function	func;
	void			*ctx;
{
	KeInitializeTimer(&timer->nt_ktimer);
	KeInitializeDpc(&timer->nt_kdpc, func, ctx);

	return;
}

__stdcall static void
ndis_timercall(dpc, timer, sysarg1, sysarg2)
	kdpc			*dpc;
	ndis_miniport_timer	*timer;
	void			*sysarg1;
	void			*sysarg2;
{
	/*
	 * Since we're called as a DPC, we should be running
	 * at DISPATCH_LEVEL here. This means to acquire the
	 * spinlock, we can use KeAcquireSpinLockAtDpcLevel()
	 * rather than KeAcquireSpinLock().
	 */
	if (NDIS_SERIALIZED(timer->nmt_block))
		KeAcquireSpinLockAtDpcLevel(&timer->nmt_block->nmb_lock);

	MSCALL4(timer->nmt_timerfunc, dpc, timer->nmt_timerctx,
	    sysarg1, sysarg2);

	if (NDIS_SERIALIZED(timer->nmt_block))
		KeReleaseSpinLockFromDpcLevel(&timer->nmt_block->nmb_lock);

	return;
}

/*
 * For a long time I wondered why there were two NDIS timer initialization
 * routines, and why this one needed an NDIS_MINIPORT_TIMER and the
 * MiniportAdapterHandle. The NDIS_MINIPORT_TIMER has its own callout
 * function and context pointers separate from those in the DPC, which
 * allows for another level of indirection: when the timer fires, we
 * can have our own timer function invoked, and from there we can call
 * the driver's function. But why go to all that trouble? Then it hit
 * me: for serialized miniports, the timer callouts are not re-entrant.
 * By trapping the callouts and having access to the MiniportAdapterHandle,
 * we can protect the driver callouts by acquiring the NDIS serialization
 * lock. This is essential for allowing serialized miniports to work
 * correctly on SMP systems. On UP hosts, setting IRQL to DISPATCH_LEVEL
 * is enough to prevent other threads from pre-empting you, but with
 * SMP, you must acquire a lock as well, otherwise the other CPU is
 * free to clobber you.
 */
__stdcall static void
NdisMInitializeTimer(timer, handle, func, ctx)
	ndis_miniport_timer	*timer;
	ndis_handle		handle;
	ndis_timer_function	func;
	void			*ctx;
{
	/* Save the driver's funcptr and context */

	timer->nmt_timerfunc = func;
	timer->nmt_timerctx = ctx;
	timer->nmt_block = handle;

	/*
	 * Set up the timer so it will call our intermediate DPC.
	 * Be sure to use the wrapped entry point, since
	 * ntoskrnl_run_dpc() expects to invoke a function with
	 * Microsoft calling conventions.
	 */
	KeInitializeTimer(&timer->nmt_ktimer);
	KeInitializeDpc(&timer->nmt_kdpc,
	    ndis_findwrap((funcptr)ndis_timercall), timer);

	return;
}

/*
 * In Windows, there's both an NdisMSetTimer() and an NdisSetTimer(),
 * but the former is just a macro wrapper around the latter.
 */
__stdcall static void
NdisSetTimer(timer, msecs)
	ndis_timer		*timer;
	uint32_t		msecs;
{
	/*
	 * KeSetTimer() wants the period in
	 * hundred nanosecond intervals.
	 */
	KeSetTimer(&timer->nt_ktimer,
	    ((int64_t)msecs * -10000), &timer->nt_kdpc);

	return;
}

__stdcall static void
NdisMSetPeriodicTimer(timer, msecs)
	ndis_miniport_timer	*timer;
	uint32_t		msecs;
{
	KeSetTimerEx(&timer->nmt_ktimer,
	    ((int64_t)msecs * -10000), msecs, &timer->nmt_kdpc);

	return;
}

/*
 * Technically, this is really NdisCancelTimer(), but we also
 * (ab)use it for NdisMCancelTimer(), since in our implementation
 * we don't need the extra info in the ndis_miniport_timer
 * structure just to cancel a timer.
 */

__stdcall static void
NdisMCancelTimer(timer, cancelled)
	ndis_timer		*timer;
	uint8_t			*cancelled;
{
	*cancelled = KeCancelTimer(&timer->nt_ktimer);

	return;
}

__stdcall static void
NdisMQueryAdapterResources(status, adapter, list, buflen)
	ndis_status		*status;
	ndis_handle		adapter;
	ndis_resource_list	*list;
	uint32_t		*buflen;
{
	ndis_miniport_block	*block;
	struct ndis_softc	*sc;
	int			rsclen;

	block = (ndis_miniport_block *)adapter;
	sc = device_get_softc(block->nmb_physdeviceobj->do_devext);

	rsclen = sizeof(ndis_resource_list) +
	    (sizeof(cm_partial_resource_desc) * (sc->ndis_rescnt - 1));
	if (*buflen < rsclen) {
		*buflen = rsclen;
		*status = NDIS_STATUS_INVALID_LENGTH;
		return;
	}

	bcopy((char *)block->nmb_rlist, (char *)list, rsclen);
	*status = NDIS_STATUS_SUCCESS;

	return;
}

__stdcall static ndis_status
NdisMRegisterIoPortRange(offset, adapter, port, numports)
	void			**offset;
	ndis_handle		adapter;
	uint32_t		port;
	uint32_t		numports;
{
	struct ndis_miniport_block	*block;
	struct ndis_softc	*sc;

	if (adapter == NULL)
		return(NDIS_STATUS_FAILURE);

	block = (ndis_miniport_block *)adapter;
	sc = device_get_softc(block->nmb_physdeviceobj->do_devext);

	if (sc->ndis_res_io == NULL)
		return(NDIS_STATUS_FAILURE);

	/* Don't let the device map more ports than we have. */
	if (rman_get_size(sc->ndis_res_io) < numports)
		return(NDIS_STATUS_INVALID_LENGTH);

	*offset = (void *)rman_get_start(sc->ndis_res_io);

	return(NDIS_STATUS_SUCCESS);
}

__stdcall static void
NdisMDeregisterIoPortRange(adapter, port, numports, offset)
	ndis_handle		adapter;
	uint32_t		port;
	uint32_t		numports;
	void			*offset;
{
	return;
}

__stdcall static void
NdisReadNetworkAddress(status, addr, addrlen, adapter)
	ndis_status		*status;
	void			**addr;
	uint32_t		*addrlen;
	ndis_handle		adapter;
{
	struct ndis_softc	*sc;
	ndis_miniport_block	*block;
	uint8_t			empty[] = { 0, 0, 0, 0, 0, 0 };

	block = (ndis_miniport_block *)adapter;
	sc = device_get_softc(block->nmb_physdeviceobj->do_devext);

	if (bcmp(sc->arpcom.ac_enaddr, empty, ETHER_ADDR_LEN) == 0)
		*status = NDIS_STATUS_FAILURE;
	else {
		*addr = sc->arpcom.ac_enaddr;
		*addrlen = ETHER_ADDR_LEN;
		*status = NDIS_STATUS_SUCCESS;
	}

	return;
}

__stdcall static ndis_status
NdisQueryMapRegisterCount(bustype, cnt)
	uint32_t		bustype;
	uint32_t		*cnt;
{
	*cnt = 8192;
	return(NDIS_STATUS_SUCCESS);
}

__stdcall static ndis_status
NdisMAllocateMapRegisters(adapter, dmachannel, dmasize, physmapneeded, maxmap)
	ndis_handle		adapter;
	uint32_t		dmachannel;
	uint8_t			dmasize;
	uint32_t		physmapneeded;
	uint32_t		maxmap;
{
	struct ndis_softc	*sc;
	ndis_miniport_block	*block;
	int			error, i, nseg = NDIS_MAXSEG;

	block = (ndis_miniport_block *)adapter;
	sc = device_get_softc(block->nmb_physdeviceobj->do_devext);

	sc->ndis_mmaps = malloc(sizeof(bus_dmamap_t) * physmapneeded,
	    M_DEVBUF, M_NOWAIT|M_ZERO);

	if (sc->ndis_mmaps == NULL)
		return(NDIS_STATUS_RESOURCES);

	error = bus_dma_tag_create(sc->ndis_parent_tag, ETHER_ALIGN, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL,
	    NULL, maxmap * nseg, nseg, maxmap, BUS_DMA_ALLOCNOW,
	    NULL, NULL, &sc->ndis_mtag);

	if (error) {
		free(sc->ndis_mmaps, M_DEVBUF);
		return(NDIS_STATUS_RESOURCES);
	}

	for (i = 0; i < physmapneeded; i++)
		bus_dmamap_create(sc->ndis_mtag, 0, &sc->ndis_mmaps[i]);

	sc->ndis_mmapcnt = physmapneeded;

	return(NDIS_STATUS_SUCCESS);
}

__stdcall static void
NdisMFreeMapRegisters(adapter)
	ndis_handle		adapter;
{
	struct ndis_softc	*sc;
	ndis_miniport_block	*block;
	int			i;

	block = (ndis_miniport_block *)adapter;
	sc = device_get_softc(block->nmb_physdeviceobj->do_devext);

	for (i = 0; i < sc->ndis_mmapcnt; i++)
		bus_dmamap_destroy(sc->ndis_mtag, sc->ndis_mmaps[i]);

	free(sc->ndis_mmaps, M_DEVBUF);

	bus_dma_tag_destroy(sc->ndis_mtag);

	return;
}

static void
ndis_mapshared_cb(arg, segs, nseg, error)
	void			*arg;
	bus_dma_segment_t	*segs;
	int			nseg;
	int			error;
{
	ndis_physaddr		*p;

	if (error || nseg > 1)
		return;

	p = arg;

	p->np_quad = segs[0].ds_addr;

	return;
}

/*
 * This maps to bus_dmamem_alloc().
 */
__stdcall static void
NdisMAllocateSharedMemory(adapter, len, cached, vaddr, paddr)
	ndis_handle		adapter;
	uint32_t		len;
	uint8_t			cached;
	void			**vaddr;
	ndis_physaddr		*paddr;
{
	ndis_miniport_block	*block;
	struct ndis_softc	*sc;
	struct ndis_shmem	*sh;
	int			error;

	if (adapter == NULL)
		return;

	block = (ndis_miniport_block *)adapter;
	sc = device_get_softc(block->nmb_physdeviceobj->do_devext);

	sh = malloc(sizeof(struct ndis_shmem), M_DEVBUF, M_NOWAIT|M_ZERO);
	if (sh == NULL)
		return;

	/*
	 * When performing shared memory allocations, create a tag
	 * with a lowaddr limit that restricts physical memory mappings
	 * so that they all fall within the first 1GB of memory.
	 * At least one device/driver combination (Linksys Instant
	 * Wireless PCI Card V2.7, Broadcom 802.11b) seems to have
	 * problems with performing DMA operations with physical
	 * addresses that lie above the 1GB mark. I don't know if this
	 * is a hardware limitation or if the addresses are being
	 * truncated within the driver, but this seems to be the only
	 * way to make these cards work reliably in systems with more
	 * than 1GB of physical memory.
	 */

	error = bus_dma_tag_create(sc->ndis_parent_tag, 64,
	    0, NDIS_BUS_SPACE_SHARED_MAXADDR, BUS_SPACE_MAXADDR, NULL,
	    NULL, len, 1, len, BUS_DMA_ALLOCNOW, NULL, NULL,
	    &sh->ndis_stag);

	if (error) {
		free(sh, M_DEVBUF);
		return;
	}

	error = bus_dmamem_alloc(sh->ndis_stag, vaddr,
	    BUS_DMA_NOWAIT | BUS_DMA_ZERO, &sh->ndis_smap);

	if (error) {
		bus_dma_tag_destroy(sh->ndis_stag);
		free(sh, M_DEVBUF);
		return;
	}

	error = bus_dmamap_load(sh->ndis_stag, sh->ndis_smap, *vaddr,
	    len, ndis_mapshared_cb, (void *)paddr, BUS_DMA_NOWAIT);

	if (error) {
		bus_dmamem_free(sh->ndis_stag, *vaddr, sh->ndis_smap);
		bus_dma_tag_destroy(sh->ndis_stag);
		free(sh, M_DEVBUF);
		return;
	}

	sh->ndis_saddr = *vaddr;
	sh->ndis_next = sc->ndis_shlist;
	sc->ndis_shlist = sh;

	return;
}

struct ndis_allocwork {
	ndis_handle		na_adapter;
	uint32_t		na_len;
	uint8_t			na_cached;
	void			*na_ctx;
};

static void
ndis_asyncmem_complete(arg)
	void			*arg;
{
	ndis_miniport_block	*block;
	struct ndis_softc	*sc;
	struct ndis_allocwork	*w;
	void			*vaddr;
	ndis_physaddr		paddr;
	__stdcall ndis_allocdone_handler	donefunc;

	w = arg;
	block = (ndis_miniport_block *)w->na_adapter;
	sc = device_get_softc(block->nmb_physdeviceobj->do_devext);

	vaddr = NULL;
	paddr.np_quad = 0;

	donefunc = sc->ndis_chars->nmc_allocate_complete_func;
	NdisMAllocateSharedMemory(w->na_adapter, w->na_len,
	    w->na_cached, &vaddr, &paddr);
	MSCALL5(donefunc, w->na_adapter, vaddr, &paddr, w->na_len, w->na_ctx);

	free(arg, M_DEVBUF);

	return;
}

__stdcall static ndis_status
NdisMAllocateSharedMemoryAsync(adapter, len, cached, ctx)
	ndis_handle		adapter;
	uint32_t		len;
	uint8_t			cached;
	void			*ctx;
{
	struct ndis_allocwork	*w;

	if (adapter == NULL)
		return(NDIS_STATUS_FAILURE);

	w = malloc(sizeof(struct ndis_allocwork), M_TEMP, M_NOWAIT);

	if (w == NULL)
		return(NDIS_STATUS_FAILURE);

	w->na_adapter = adapter;
	w->na_cached = cached;
	w->na_len = len;
	w->na_ctx = ctx;

	/*
	 * Pawn this work off on the SWI thread instead of the
	 * taskqueue thread, because sometimes drivers will queue
	 * up work items on the taskqueue thread that will block,
	 * which would prevent the memory allocation from completing
	 * when we need it.
	 */
	ndis_sched(ndis_asyncmem_complete, w, NDIS_SWI);

	return(NDIS_STATUS_PENDING);
}

__stdcall static void
NdisMFreeSharedMemory(adapter, len, cached, vaddr, paddr)
	ndis_handle		adapter;
	uint32_t		len;
	uint8_t			cached;
	void			*vaddr;
	ndis_physaddr		paddr;
{
	ndis_miniport_block	*block;
	struct ndis_softc	*sc;
	struct ndis_shmem	*sh, *prev;

	if (vaddr == NULL || adapter == NULL)
		return;

	block = (ndis_miniport_block *)adapter;
	sc = device_get_softc(block->nmb_physdeviceobj->do_devext);
	sh = prev = sc->ndis_shlist;

	/* Sanity check: is list empty? */

	if (sh == NULL)
		return;

	while (sh) {
		if (sh->ndis_saddr == vaddr)
			break;
		prev = sh;
		sh = sh->ndis_next;
	}

	bus_dmamap_unload(sh->ndis_stag, sh->ndis_smap);
	bus_dmamem_free(sh->ndis_stag, vaddr, sh->ndis_smap);
	bus_dma_tag_destroy(sh->ndis_stag);

	if (sh == sc->ndis_shlist)
		sc->ndis_shlist = sh->ndis_next;
	else
		prev->ndis_next = sh->ndis_next;

	free(sh, M_DEVBUF);

	return;
}

__stdcall static ndis_status
NdisMMapIoSpace(vaddr, adapter, paddr, len)
	void			**vaddr;
	ndis_handle		adapter;
	ndis_physaddr		paddr;
	uint32_t		len;
{
	ndis_miniport_block	*block;
	struct ndis_softc	*sc;

	if (adapter == NULL)
		return(NDIS_STATUS_FAILURE);

	block = (ndis_miniport_block *)adapter;
	sc = device_get_softc(block->nmb_physdeviceobj->do_devext);

	if (sc->ndis_res_mem != NULL &&
	    paddr.np_quad == rman_get_start(sc->ndis_res_mem))
		*vaddr = (void *)rman_get_virtual(sc->ndis_res_mem);
	else if (sc->ndis_res_altmem != NULL &&
	     paddr.np_quad == rman_get_start(sc->ndis_res_altmem))
		*vaddr = (void *)rman_get_virtual(sc->ndis_res_altmem);
	else if (sc->ndis_res_am != NULL &&
	     paddr.np_quad == rman_get_start(sc->ndis_res_am))
		*vaddr = (void *)rman_get_virtual(sc->ndis_res_am);
	else
		return(NDIS_STATUS_FAILURE);

	return(NDIS_STATUS_SUCCESS);
}

__stdcall static void
NdisMUnmapIoSpace(adapter, vaddr, len)
	ndis_handle		adapter;
	void			*vaddr;
	uint32_t		len;
{
	return;
}

__stdcall static uint32_t
NdisGetCacheFillSize(void)
{
	return(128);
}

__stdcall static uint32_t
NdisMGetDmaAlignment(handle)
	ndis_handle		handle;
{
	return(128);
}

/*
 * NDIS has two methods for dealing with NICs that support DMA.
 * One is to just pass packets to the driver and let it call
 * NdisMStartBufferPhysicalMapping() to map each buffer in the packet
 * all by itself, and the other is to let the NDIS library handle the
 * buffer mapping internally, and hand the driver an already populated
 * scatter/gather fragment list. If the driver calls
 * NdisMInitializeScatterGatherDma(), it wants to use the latter
 * method.
 */

__stdcall static ndis_status
NdisMInitializeScatterGatherDma(adapter, is64, maxphysmap)
	ndis_handle		adapter;
	uint8_t			is64;
	uint32_t		maxphysmap;
{
	struct ndis_softc	*sc;
	ndis_miniport_block	*block;
	int			error;

	if (adapter == NULL)
		return(NDIS_STATUS_FAILURE);
	block = (ndis_miniport_block *)adapter;
	sc = device_get_softc(block->nmb_physdeviceobj->do_devext);

	/* Don't do this twice. */
	if (sc->ndis_sc == 1)
		return(NDIS_STATUS_SUCCESS);

	error = bus_dma_tag_create(sc->ndis_parent_tag, ETHER_ALIGN, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    MCLBYTES * NDIS_MAXSEG, NDIS_MAXSEG, MCLBYTES, BUS_DMA_ALLOCNOW,
	    NULL, NULL, &sc->ndis_ttag);

	sc->ndis_sc = 1;

	return(NDIS_STATUS_SUCCESS);
}

__stdcall void
NdisAllocatePacketPool(status, pool, descnum, protrsvdlen)
	ndis_status		*status;
	ndis_handle		*pool;
	uint32_t		descnum;
	uint32_t		protrsvdlen;
{
	ndis_packet		*cur;
	int			i;

	*pool = malloc((sizeof(ndis_packet) + protrsvdlen) *
	    ((descnum + NDIS_POOL_EXTRA) + 1),
	    M_DEVBUF, M_NOWAIT|M_ZERO);

	if (*pool == NULL) {
		*status = NDIS_STATUS_RESOURCES;
		return;
	}

	cur = (ndis_packet *)*pool;
	KeInitializeSpinLock(&cur->np_lock);
	cur->np_private.npp_flags = 0x1; /* mark the head of the list */
	cur->np_private.npp_totlen = 0; /* init deletetion flag */
	for (i = 0; i < (descnum + NDIS_POOL_EXTRA); i++) {
		cur->np_private.npp_head = (ndis_handle)(cur + 1);
		cur++;
	}

	*status = NDIS_STATUS_SUCCESS;
	return;
}

__stdcall void
NdisAllocatePacketPoolEx(status, pool, descnum, oflowdescnum, protrsvdlen)
	ndis_status		*status;
	ndis_handle		*pool;
	uint32_t		descnum;
	uint32_t		oflowdescnum;
	uint32_t		protrsvdlen;
{
	return(NdisAllocatePacketPool(status, pool,
	    descnum + oflowdescnum, protrsvdlen));
}

__stdcall uint32_t
NdisPacketPoolUsage(pool)
	ndis_handle		pool;
{
	ndis_packet		*head;
	uint8_t			irql;
	uint32_t		cnt;

	head = (ndis_packet *)pool;
	KeAcquireSpinLock(&head->np_lock, &irql);
	cnt = head->np_private.npp_count;
	KeReleaseSpinLock(&head->np_lock, irql);

	return(cnt);
}

__stdcall void
NdisFreePacketPool(pool)
	ndis_handle		pool;
{
	ndis_packet		*head;
	uint8_t			irql;

	head = pool;

	/* Mark this pool as 'going away.' */

	KeAcquireSpinLock(&head->np_lock, &irql);
	head->np_private.npp_totlen = 1;

	/* If there are no buffers loaned out, destroy the pool. */

	if (head->np_private.npp_count == 0) {
		KeReleaseSpinLock(&head->np_lock, irql);
		free(pool, M_DEVBUF);
	} else {
		printf("NDIS: buggy driver deleting active packet pool!\n");
		KeReleaseSpinLock(&head->np_lock, irql);
	}

	return;
}

__stdcall void
NdisAllocatePacket(status, packet, pool)
	ndis_status		*status;
	ndis_packet		**packet;
	ndis_handle		pool;
{
	ndis_packet		*head, *pkt;
	uint8_t			irql;

	head = (ndis_packet *)pool;
	KeAcquireSpinLock(&head->np_lock, &irql);

	if (head->np_private.npp_flags != 0x1) {
		*status = NDIS_STATUS_FAILURE;
		KeReleaseSpinLock(&head->np_lock, irql);
		return;
	}

	/*
	 * If this pool is marked as 'going away' don't allocate any
	 * more packets out of it.
	 */

	if (head->np_private.npp_totlen) {
		*status = NDIS_STATUS_FAILURE;
		KeReleaseSpinLock(&head->np_lock, irql);
		return;
	}

	pkt = (ndis_packet *)head->np_private.npp_head;

	if (pkt == NULL) {
		*status = NDIS_STATUS_RESOURCES;
		KeReleaseSpinLock(&head->np_lock, irql);
		return;
	}

	head->np_private.npp_head = pkt->np_private.npp_head;

	pkt->np_private.npp_head = pkt->np_private.npp_tail = NULL;
	/* Save pointer to the pool. */
	pkt->np_private.npp_pool = head;

	/* Set the oob offset pointer. Lots of things expect this. */
	pkt->np_private.npp_packetooboffset =
	    offsetof(ndis_packet, np_oob);

	/*
	 * We must initialize the packet flags correctly in order
	 * for the NDIS_SET_PACKET_MEDIA_SPECIFIC_INFO() and
	 * NDIS_GET_PACKET_MEDIA_SPECIFIC_INFO() macros to work
         * correctly.
	 */
	pkt->np_private.npp_ndispktflags = NDIS_PACKET_ALLOCATED_BY_NDIS;
	pkt->np_private.npp_validcounts = FALSE;

	*packet = pkt;

	head->np_private.npp_count++;
	*status = NDIS_STATUS_SUCCESS;

	KeReleaseSpinLock(&head->np_lock, irql);

	return;
}

__stdcall void
NdisFreePacket(packet)
	ndis_packet		*packet;
{
	ndis_packet		*head;
	uint8_t			irql;

	if (packet == NULL || packet->np_private.npp_pool == NULL)
		return;

	head = packet->np_private.npp_pool;
	KeAcquireSpinLock(&head->np_lock, &irql);

	if (head->np_private.npp_flags != 0x1) {
		KeReleaseSpinLock(&head->np_lock, irql);
		return;
	}

	packet->np_private.npp_head = head->np_private.npp_head;
	head->np_private.npp_head = (ndis_buffer *)packet;
	head->np_private.npp_count--;

	/*
	 * If the pool has been marked for deletion and there are
	 * no more packets outstanding, nuke the pool.
	 */

	if (head->np_private.npp_totlen && head->np_private.npp_count == 0) {
		KeReleaseSpinLock(&head->np_lock, irql);
		free(head, M_DEVBUF);
	} else
		KeReleaseSpinLock(&head->np_lock, irql);

	return;
}

__stdcall static void
NdisUnchainBufferAtFront(packet, buf)
	ndis_packet		*packet;
	ndis_buffer		**buf;
{
	ndis_packet_private	*priv;

	if (packet == NULL || buf == NULL)
		return;

	priv = &packet->np_private;

	priv->npp_validcounts = FALSE;

	if (priv->npp_head == priv->npp_tail) {
		*buf = priv->npp_head;
		priv->npp_head = priv->npp_tail = NULL;
	} else {
		*buf = priv->npp_head;
		priv->npp_head = (*buf)->mdl_next;
	}

	return;
}

__stdcall static void
NdisUnchainBufferAtBack(packet, buf)
	ndis_packet		*packet;
	ndis_buffer		**buf;
{
	ndis_packet_private	*priv;
	ndis_buffer		*tmp;

	if (packet == NULL || buf == NULL)
		return;

	priv = &packet->np_private;

	priv->npp_validcounts = FALSE;

	if (priv->npp_head == priv->npp_tail) {
		*buf = priv->npp_head;
		priv->npp_head = priv->npp_tail = NULL;
	} else {
		*buf = priv->npp_tail;
		tmp = priv->npp_head;
		while (tmp->mdl_next != priv->npp_tail)
			tmp = tmp->mdl_next;
		priv->npp_tail = tmp;
		tmp->mdl_next = NULL;
	}

	return;
}

/*
 * The NDIS "buffer" is really an MDL (memory descriptor list)
 * which is used to describe a buffer in a way that allows it
 * to mapped into different contexts. We have to be careful how
 * we handle them: in some versions of Windows, the NdisFreeBuffer()
 * routine is an actual function in the NDIS API, but in others
 * it's just a macro wrapper around IoFreeMdl(). There's really
 * no way to use the 'descnum' parameter to count how many
 * "buffers" are allocated since in order to use IoFreeMdl() to
 * dispose of a buffer, we have to use IoAllocateMdl() to allocate
 * them, and IoAllocateMdl() just grabs them out of the heap.
 */

__stdcall static void
NdisAllocateBufferPool(status, pool, descnum)
	ndis_status		*status;
	ndis_handle		*pool;
	uint32_t		descnum;
{
	/*
	 * The only thing we can really do here is verify that descnum
	 * is a reasonable value, but I really don't know what to check
	 * it against.
	 */

	*pool = NonPagedPool;
	*status = NDIS_STATUS_SUCCESS;
	return;
}

__stdcall static void
NdisFreeBufferPool(pool)
	ndis_handle		pool;
{
	return;
}

__stdcall static void
NdisAllocateBuffer(status, buffer, pool, vaddr, len)
	ndis_status		*status;
	ndis_buffer		**buffer;
	ndis_handle		pool;
	void			*vaddr;
	uint32_t		len;
{
	ndis_buffer		*buf;

	buf = IoAllocateMdl(vaddr, len, FALSE, FALSE, NULL);
	if (buf == NULL) {
		*status = NDIS_STATUS_RESOURCES;
		return;
	}

	*buffer = buf;
	*status = NDIS_STATUS_SUCCESS;

	return;
}

__stdcall static void
NdisFreeBuffer(buf)
	ndis_buffer		*buf;
{
	IoFreeMdl(buf);
	return;
}

/* Aw c'mon. */

__stdcall static uint32_t
NdisBufferLength(buf)
	ndis_buffer		*buf;
{
	return(MmGetMdlByteCount(buf));
}

/*
 * Get the virtual address and length of a buffer.
 * Note: the vaddr argument is optional.
 */

__stdcall static void
NdisQueryBuffer(buf, vaddr, len)
	ndis_buffer		*buf;
	void			**vaddr;
	uint32_t		*len;
{
	if (vaddr != NULL)
		*vaddr = MmGetMdlVirtualAddress(buf);
	*len = MmGetMdlByteCount(buf);

	return;
}

/* Same as above -- we don't care about the priority. */

__stdcall static void
NdisQueryBufferSafe(buf, vaddr, len, prio)
	ndis_buffer		*buf;
	void			**vaddr;
	uint32_t		*len;
	uint32_t		prio;
{
	if (vaddr != NULL)
		*vaddr = MmGetMdlVirtualAddress(buf);
	*len = MmGetMdlByteCount(buf);

	return;
}

/* Damnit Microsoft!! How many ways can you do the same thing?! */

__stdcall static void *
NdisBufferVirtualAddress(buf)
	ndis_buffer		*buf;
{
	return(MmGetMdlVirtualAddress(buf));
}

__stdcall static void *
NdisBufferVirtualAddressSafe(buf, prio)
	ndis_buffer		*buf;
	uint32_t		prio;
{
	return(MmGetMdlVirtualAddress(buf));
}

__stdcall static void
NdisAdjustBufferLength(buf, len)
	ndis_buffer		*buf;
	int			len;
{
	MmGetMdlByteCount(buf) = len;

	return;
}

__stdcall static uint32_t
NdisInterlockedIncrement(addend)
	uint32_t		*addend;
{
	atomic_add_long((u_long *)addend, 1);
	return(*addend);
}

__stdcall static uint32_t
NdisInterlockedDecrement(addend)
	uint32_t		*addend;
{
	atomic_subtract_long((u_long *)addend, 1);
	return(*addend);
}

__stdcall static void
NdisInitializeEvent(event)
	ndis_event		*event;
{
	/*
	 * NDIS events are always notification
	 * events, and should be initialized to the
	 * not signaled state.
	 */
 
	KeInitializeEvent(&event->ne_event, EVENT_TYPE_NOTIFY, FALSE);
	return;
}

__stdcall static void
NdisSetEvent(event)
	ndis_event		*event;
{
	KeSetEvent(&event->ne_event, 0, 0);
	return;
}

__stdcall static void
NdisResetEvent(event)
	ndis_event		*event;
{
	KeResetEvent(&event->ne_event);
	return;
}

__stdcall static uint8_t
NdisWaitEvent(event, msecs)
	ndis_event		*event;
	uint32_t		msecs;
{
	int64_t			duetime;
	uint32_t		rval;

	duetime = ((int64_t)msecs * -10000);

	rval = KeWaitForSingleObject((nt_dispatch_header *)event,
	    0, 0, TRUE, msecs ? &duetime : NULL);

	if (rval == STATUS_TIMEOUT)
		return(FALSE);

	return(TRUE);
}

__stdcall static ndis_status
NdisUnicodeStringToAnsiString(dstr, sstr)
	ndis_ansi_string	*dstr;
	ndis_unicode_string	*sstr;
{
	if (dstr == NULL || sstr == NULL)
		return(NDIS_STATUS_FAILURE);
	if (ndis_unicode_to_ascii(sstr->us_buf,
	    sstr->us_len, &dstr->nas_buf))
		return(NDIS_STATUS_FAILURE);
	dstr->nas_len = dstr->nas_maxlen = strlen(dstr->nas_buf);
	return (NDIS_STATUS_SUCCESS);
}

__stdcall static ndis_status
NdisAnsiStringToUnicodeString(dstr, sstr)
	ndis_unicode_string	*dstr;
	ndis_ansi_string	*sstr;
{
	char			*str;
	if (dstr == NULL || sstr == NULL)
		return(NDIS_STATUS_FAILURE);
	str = malloc(sstr->nas_len + 1, M_DEVBUF, M_NOWAIT);
	if (str == NULL)
		return(NDIS_STATUS_FAILURE);
	strncpy(str, sstr->nas_buf, sstr->nas_len);
	*(str + sstr->nas_len) = '\0';
	if (ndis_ascii_to_unicode(str, &dstr->us_buf)) {
		free(str, M_DEVBUF);
		return(NDIS_STATUS_FAILURE);
	}
	dstr->us_len = dstr->us_maxlen = sstr->nas_len * 2;
	free(str, M_DEVBUF);
	return (NDIS_STATUS_SUCCESS);
}

__stdcall static ndis_status
NdisMPciAssignResources(adapter, slot, list)
	ndis_handle		adapter;
	uint32_t		slot;
	ndis_resource_list	**list;
{
	ndis_miniport_block	*block;

	if (adapter == NULL || list == NULL)
		return (NDIS_STATUS_FAILURE);

	block = (ndis_miniport_block *)adapter;
	*list = block->nmb_rlist;

	return (NDIS_STATUS_SUCCESS);
}

__stdcall static ndis_status
NdisMRegisterInterrupt(intr, adapter, ivec, ilevel, reqisr, shared, imode)
	ndis_miniport_interrupt	*intr;
	ndis_handle		adapter;
	uint32_t		ivec;
	uint32_t		ilevel;
	uint8_t			reqisr;
	uint8_t			shared;
	ndis_interrupt_mode	imode;
{
	ndis_miniport_block	*block;

	block = adapter;

	intr->ni_block = adapter;
	intr->ni_isrreq = reqisr;
	intr->ni_shared = shared;
	block->nmb_interrupt = intr;

	KeInitializeSpinLock(&intr->ni_dpccountlock);

	return(NDIS_STATUS_SUCCESS);
}	

__stdcall static void
NdisMDeregisterInterrupt(intr)
	ndis_miniport_interrupt	*intr;
{
	return;
}

__stdcall static void
NdisMRegisterAdapterShutdownHandler(adapter, shutdownctx, shutdownfunc)
	ndis_handle		adapter;
	void			*shutdownctx;
	ndis_shutdown_handler	shutdownfunc;
{
	ndis_miniport_block	*block;
	ndis_miniport_characteristics *chars;
	struct ndis_softc	*sc;

	if (adapter == NULL)
		return;

	block = (ndis_miniport_block *)adapter;
	sc = device_get_softc(block->nmb_physdeviceobj->do_devext);
	chars = sc->ndis_chars;

	chars->nmc_shutdown_handler = shutdownfunc;
	chars->nmc_rsvd0 = shutdownctx;

	return;
}

__stdcall static void
NdisMDeregisterAdapterShutdownHandler(adapter)
	ndis_handle		adapter;
{
	ndis_miniport_block	*block;
	ndis_miniport_characteristics *chars;
	struct ndis_softc	*sc;

	if (adapter == NULL)
		return;

	block = (ndis_miniport_block *)adapter;
	sc = device_get_softc(block->nmb_physdeviceobj->do_devext);
	chars = sc->ndis_chars;

	chars->nmc_shutdown_handler = NULL;
	chars->nmc_rsvd0 = NULL;

	return;
}

__stdcall static uint32_t
NDIS_BUFFER_TO_SPAN_PAGES(buf)
	ndis_buffer		*buf;
{
	if (buf == NULL)
		return(0);
	if (MmGetMdlByteCount(buf) == 0)
		return(1);
	return(SPAN_PAGES(MmGetMdlVirtualAddress(buf),
	    MmGetMdlByteCount(buf)));
}

__stdcall static void
NdisGetBufferPhysicalArraySize(buf, pages)
	ndis_buffer		*buf;
	uint32_t		*pages;
{
	if (buf == NULL)
		return;

	*pages = NDIS_BUFFER_TO_SPAN_PAGES(buf);
	return;
}

__stdcall static void
NdisQueryBufferOffset(buf, off, len)
	ndis_buffer		*buf;
	uint32_t		*off;
	uint32_t		*len;
{
	if (buf == NULL)
		return;

	*off = MmGetMdlByteOffset(buf);
	*len = MmGetMdlByteCount(buf);

	return;
}

__stdcall static void
NdisMSleep(usecs)
	uint32_t		usecs;
{
	struct timeval		tv;

	tv.tv_sec = 0;
	tv.tv_usec = usecs;

	ndis_thsuspend(curthread->td_proc, NULL, tvtohz(&tv));

	return;
}

__stdcall static uint32_t
NdisReadPcmciaAttributeMemory(handle, offset, buf, len)
	ndis_handle		handle;
	uint32_t		offset;
	void			*buf;
	uint32_t		len;
{
	struct ndis_softc	*sc;
	ndis_miniport_block	*block;
	bus_space_handle_t	bh;
	bus_space_tag_t		bt;
	char			*dest;
	int			i;

	if (handle == NULL)
		return(0);

	block = (ndis_miniport_block *)handle;
	sc = device_get_softc(block->nmb_physdeviceobj->do_devext);
	dest = buf;

	bh = rman_get_bushandle(sc->ndis_res_am);
	bt = rman_get_bustag(sc->ndis_res_am);

	for (i = 0; i < len; i++)
		dest[i] = bus_space_read_1(bt, bh, (offset + i) * 2);

	return(i);
}

__stdcall static uint32_t
NdisWritePcmciaAttributeMemory(handle, offset, buf, len)
	ndis_handle		handle;
	uint32_t		offset;
	void			*buf;
	uint32_t		len;
{
	struct ndis_softc	*sc;
	ndis_miniport_block	*block;
	bus_space_handle_t	bh;
	bus_space_tag_t		bt;
	char			*src;
	int			i;

	if (handle == NULL)
		return(0);

	block = (ndis_miniport_block *)handle;
	sc = device_get_softc(block->nmb_physdeviceobj->do_devext);
	src = buf;

	bh = rman_get_bushandle(sc->ndis_res_am);
	bt = rman_get_bustag(sc->ndis_res_am);

	for (i = 0; i < len; i++)
		bus_space_write_1(bt, bh, (offset + i) * 2, src[i]);

	return(i);
}

__stdcall static list_entry *
NdisInterlockedInsertHeadList(head, entry, lock)
	list_entry		*head;
	list_entry		*entry;
	ndis_spin_lock		*lock;
{
	list_entry		*flink;

	KeAcquireSpinLock(&lock->nsl_spinlock, &lock->nsl_kirql);
	flink = head->nle_flink;
	entry->nle_flink = flink;
	entry->nle_blink = head;
	flink->nle_blink = entry;
	head->nle_flink = entry;
	KeReleaseSpinLock(&lock->nsl_spinlock, lock->nsl_kirql);

	return(flink);
}

__stdcall static list_entry *
NdisInterlockedRemoveHeadList(head, lock)
	list_entry		*head;
	ndis_spin_lock		*lock;
{
	list_entry		*flink;
	list_entry		*entry;

	KeAcquireSpinLock(&lock->nsl_spinlock, &lock->nsl_kirql);
	entry = head->nle_flink;
	flink = entry->nle_flink;
	head->nle_flink = flink;
	flink->nle_blink = head;
	KeReleaseSpinLock(&lock->nsl_spinlock, lock->nsl_kirql);

	return(entry);
}

__stdcall static list_entry *
NdisInterlockedInsertTailList(head, entry, lock)
	list_entry		*head;
	list_entry		*entry;
	ndis_spin_lock		*lock;
{
	list_entry		*blink;

	KeAcquireSpinLock(&lock->nsl_spinlock, &lock->nsl_kirql);
	blink = head->nle_blink;
	entry->nle_flink = head;
	entry->nle_blink = blink;
	blink->nle_flink = entry;
	head->nle_blink = entry;
	KeReleaseSpinLock(&lock->nsl_spinlock, lock->nsl_kirql);

	return(blink);
}

__stdcall static uint8_t
NdisMSynchronizeWithInterrupt(intr, syncfunc, syncctx)
	ndis_miniport_interrupt	*intr;
	void			*syncfunc;
	void			*syncctx;
{
	__stdcall uint8_t (*sync)(void *);
	uint8_t			rval;
	uint8_t			irql;

	if (syncfunc == NULL || syncctx == NULL)
		return(0);

	sync = syncfunc;
	KeAcquireSpinLock(&intr->ni_dpccountlock, &irql);
	rval = MSCALL1(sync, syncctx);
	KeReleaseSpinLock(&intr->ni_dpccountlock, irql);

	return(rval);
}

/*
 * Return the number of 100 nanosecond intervals since
 * January 1, 1601. (?!?!)
 */
__stdcall static void
NdisGetCurrentSystemTime(tval)
	uint64_t		*tval;
{
	struct timespec		ts;

	nanotime(&ts);
	*tval = (uint64_t)ts.tv_nsec / 100 + (uint64_t)ts.tv_sec * 10000000 +
	    11644473600;

	return;
}

/*
 * Return the number of milliseconds since the system booted.
 */
__stdcall static void
NdisGetSystemUpTime(tval)
	uint32_t		*tval;
{
	*tval = (ticks * hz) / 1000;

	return;
}

__stdcall static void
NdisInitializeString(dst, src)
	ndis_unicode_string	*dst;
	char			*src;
{
	ndis_unicode_string	*u;

	u = dst;
	u->us_buf = NULL;
	if (ndis_ascii_to_unicode(src, &u->us_buf))
		return;
	u->us_len = u->us_maxlen = strlen(src) * 2;
	return;
}

__stdcall static void
NdisFreeString(str)
	ndis_unicode_string	*str;
{
	if (str == NULL)
		return;
	if (str->us_buf != NULL)
		free(str->us_buf, M_DEVBUF);
	free(str, M_DEVBUF);
	return;
}

__stdcall static ndis_status
NdisMRemoveMiniport(adapter)
	ndis_handle		*adapter;
{
	return(NDIS_STATUS_SUCCESS);
}

__stdcall static void
NdisInitAnsiString(dst, src)
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
NdisInitUnicodeString(dst, src)
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

__stdcall static void NdisMGetDeviceProperty(adapter, phydevobj,
	funcdevobj, nextdevobj, resources, transresources)
	ndis_handle		adapter;
	device_object		**phydevobj;
	device_object		**funcdevobj;
	device_object		**nextdevobj;
	cm_resource_list	*resources;
	cm_resource_list	*transresources;
{
	ndis_miniport_block	*block;

	block = (ndis_miniport_block *)adapter;

	if (phydevobj != NULL)
		*phydevobj = block->nmb_physdeviceobj;
	if (funcdevobj != NULL)
		*funcdevobj = block->nmb_deviceobj;
	if (nextdevobj != NULL)
		*nextdevobj = block->nmb_nextdeviceobj;

	return;
}

__stdcall static void
NdisGetFirstBufferFromPacket(packet, buf, firstva, firstlen, totlen)
	ndis_packet		*packet;
	ndis_buffer		**buf;
	void			**firstva;
	uint32_t		*firstlen;
	uint32_t		*totlen;
{
	ndis_buffer		*tmp;

	tmp = packet->np_private.npp_head;
	*buf = tmp;
	if (tmp == NULL) {
		*firstva = NULL;
		*firstlen = *totlen = 0;
	} else {
		*firstva = MmGetMdlVirtualAddress(tmp);
		*firstlen = *totlen = MmGetMdlByteCount(tmp);
		for (tmp = tmp->mdl_next; tmp != NULL; tmp = tmp->mdl_next)
			*totlen += MmGetMdlByteCount(tmp);
	}

	return;
}

__stdcall static void
NdisGetFirstBufferFromPacketSafe(packet, buf, firstva, firstlen, totlen, prio)
	ndis_packet		*packet;
	ndis_buffer		**buf;
	void			**firstva;
	uint32_t		*firstlen;
	uint32_t		*totlen;
	uint32_t		prio;
{
	NdisGetFirstBufferFromPacket(packet, buf, firstva, firstlen, totlen);
}

static int
ndis_find_sym(lf, filename, suffix, sym)
	linker_file_t		lf;
	char			*filename;
	char			*suffix;
	caddr_t			*sym;
{
	char			*fullsym;
	char			*suf;
	int			i;

	fullsym = ExAllocatePoolWithTag(NonPagedPool, MAXPATHLEN, 0);
	if (fullsym == NULL)
		return(ENOMEM);

	bzero(fullsym, MAXPATHLEN);
	strncpy(fullsym, filename, MAXPATHLEN);
	if (strlen(filename) < 4) {
		ExFreePool(fullsym);
		return(EINVAL);
	}

	/* If the filename has a .ko suffix, strip if off. */
	suf = fullsym + (strlen(filename) - 3);
	if (strcmp(suf, ".ko") == 0)
		*suf = '\0';

	for (i = 0; i < strlen(fullsym); i++) {
		if (fullsym[i] == '.')
			fullsym[i] = '_';
		else
			fullsym[i] = tolower(fullsym[i]);
	}
	strcat(fullsym, suffix);
	*sym = linker_file_lookup_symbol(lf, fullsym, 0);
	ExFreePool(fullsym);
	if (*sym == 0)
		return(ENOENT);

	return(0);
}

/* can also return NDIS_STATUS_RESOURCES/NDIS_STATUS_ERROR_READING_FILE */
__stdcall static void
NdisOpenFile(status, filehandle, filelength, filename, highestaddr)
	ndis_status		*status;
	ndis_handle		*filehandle;
	uint32_t		*filelength;
	ndis_unicode_string	*filename;
	ndis_physaddr		highestaddr;
{
	char			*afilename = NULL;
	struct thread		*td = curthread;
	struct nameidata	nd;
	int			flags, error;
	struct vattr		vat;
	struct vattr		*vap = &vat;
	ndis_fh			*fh;
	char			*path;
	linker_file_t		head, lf;
	caddr_t			kldstart, kldend;

	ndis_unicode_to_ascii(filename->us_buf,
	    filename->us_len, &afilename);

	fh = ExAllocatePoolWithTag(NonPagedPool, sizeof(ndis_fh), 0);
	if (fh == NULL) {
		*status = NDIS_STATUS_RESOURCES;
		return;
	}

	/*
	 * During system bootstrap, it's impossible to load files
	 * from the rootfs since it's not mounted yet. We therefore
	 * offer the possibility of opening files that have been
	 * preloaded as modules instead. Both choices will work
	 * when kldloading a module from multiuser, but only the
	 * module option will work during bootstrap. The module
	 * loading option works by using the ndiscvt(8) utility
	 * to convert the arbitrary file into a .ko using objcopy(1).
	 * This file will contain two special symbols: filename_start
	 * and filename_end. All we have to do is traverse the KLD
	 * list in search of those symbols and we've found the file
	 * data. As an added bonus, ndiscvt(8) will also generate
	 * a normal .o file which can be linked statically with
	 * the kernel. This means that the symbols will actual reside
	 * in the kernel's symbol table, but that doesn't matter to
	 * us since the kernel appears to us as just another module.
	 */

	/*
	 * This is an evil trick for getting the head of the linked
	 * file list, which is not exported from kern_linker.o. It
	 * happens that linker file #1 is always the kernel, and is
	 * always the first element in the list.
	 */

	head = linker_find_file_by_id(1);
	for (lf = head; lf != NULL; lf = TAILQ_NEXT(lf, link)) {
		if (ndis_find_sym(lf, afilename, "_start", &kldstart))
			continue;
		if (ndis_find_sym(lf, afilename, "_end", &kldend))
			continue;
		fh->nf_vp = lf;
		fh->nf_map = NULL;
		fh->nf_type = NDIS_FH_TYPE_MODULE;
		*filelength = fh->nf_maplen = (kldend - kldstart) & 0xFFFFFFFF;
		*filehandle = fh;
		free(afilename, M_DEVBUF);
		*status = NDIS_STATUS_SUCCESS;
		return;
	}

	if (TAILQ_EMPTY(&mountlist)) {
		ExFreePool(fh);
		*status = NDIS_STATUS_FILE_NOT_FOUND;
		printf("NDIS: could not find file %s in linker list\n",
		    afilename);
		printf("NDIS: and no filesystems mounted yet, "
		    "aborting NdisOpenFile()\n");
		free(afilename, M_DEVBUF);
		return;
	}

	path = ExAllocatePoolWithTag(NonPagedPool, MAXPATHLEN, 0);
	if (path == NULL) {
		ExFreePool(fh);
		*status = NDIS_STATUS_RESOURCES;
		return;
	}

	snprintf(path, MAXPATHLEN, "%s/%s", ndis_filepath, afilename);
	free(afilename, M_DEVBUF);

	mtx_lock(&Giant);

	/* Some threads don't have a current working directory. */

	if (td->td_proc->p_fd->fd_rdir == NULL)
		td->td_proc->p_fd->fd_rdir = rootvnode;
	if (td->td_proc->p_fd->fd_cdir == NULL)
		td->td_proc->p_fd->fd_cdir = rootvnode;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_SYSSPACE, path, td);

	flags = FREAD;
	error = vn_open(&nd, &flags, 0, -1);
	if (error) {
		mtx_unlock(&Giant);
		*status = NDIS_STATUS_FILE_NOT_FOUND;
		ExFreePool(fh);
		printf("NDIS: open file %s failed: %d\n", path, error);
		ExFreePool(path);
		return;
	}

	ExFreePool(path);

	NDFREE(&nd, NDF_ONLY_PNBUF);

	/* Get the file size. */
	VOP_GETATTR(nd.ni_vp, vap, td->td_ucred, td);
	VOP_UNLOCK(nd.ni_vp, 0, td);
	mtx_unlock(&Giant);

	fh->nf_vp = nd.ni_vp;
	fh->nf_map = NULL;
	fh->nf_type = NDIS_FH_TYPE_VFS;
	*filehandle = fh;
	*filelength = fh->nf_maplen = vap->va_size & 0xFFFFFFFF;
	*status = NDIS_STATUS_SUCCESS;

	return;
}

__stdcall static void
NdisMapFile(status, mappedbuffer, filehandle)
	ndis_status		*status;
	void			**mappedbuffer;
	ndis_handle		filehandle;
{
	ndis_fh			*fh;
	struct thread		*td = curthread;
	linker_file_t		lf;
	caddr_t			kldstart;
	int			error, resid;

	if (filehandle == NULL) {
		*status = NDIS_STATUS_FAILURE;
		return;
	}

	fh = (ndis_fh *)filehandle;

	if (fh->nf_vp == NULL) {
		*status = NDIS_STATUS_FAILURE;
		return;
	}

	if (fh->nf_map != NULL) {
		*status = NDIS_STATUS_ALREADY_MAPPED;
		return;
	}

	if (fh->nf_type == NDIS_FH_TYPE_MODULE) {
		lf = fh->nf_vp;
		if (ndis_find_sym(lf, lf->filename, "_start", &kldstart)) {
			*status = NDIS_STATUS_FAILURE;
			return;
		}
		fh->nf_map = kldstart;
		*status = NDIS_STATUS_SUCCESS;
		*mappedbuffer = fh->nf_map;
		return;
	}

	fh->nf_map = ExAllocatePoolWithTag(NonPagedPool, fh->nf_maplen, 0);

	if (fh->nf_map == NULL) {
		*status = NDIS_STATUS_RESOURCES;
		return;
	}

	mtx_lock(&Giant);
	error = vn_rdwr(UIO_READ, fh->nf_vp, fh->nf_map, fh->nf_maplen, 0,
	    UIO_SYSSPACE, 0, td->td_ucred, NOCRED, &resid, td);
	mtx_unlock(&Giant);

	if (error)
		*status = NDIS_STATUS_FAILURE;
	else {
		*status = NDIS_STATUS_SUCCESS;
		*mappedbuffer = fh->nf_map;
	}

	return;
}

__stdcall static void
NdisUnmapFile(filehandle)
	ndis_handle		filehandle;
{
	ndis_fh			*fh;
	fh = (ndis_fh *)filehandle;

	if (fh->nf_map == NULL)
		return;

	if (fh->nf_type == NDIS_FH_TYPE_VFS)
		ExFreePool(fh->nf_map);
	fh->nf_map = NULL;

	return;
}

__stdcall static void
NdisCloseFile(filehandle)
	ndis_handle		filehandle;
{
	struct thread		*td = curthread;
	ndis_fh			*fh;

	if (filehandle == NULL)
		return;

	fh = (ndis_fh *)filehandle;
	if (fh->nf_map != NULL) {
		if (fh->nf_type == NDIS_FH_TYPE_VFS)
			ExFreePool(fh->nf_map);
		fh->nf_map = NULL;
	}

	if (fh->nf_vp == NULL)
		return;

	if (fh->nf_type == NDIS_FH_TYPE_VFS) {
		mtx_lock(&Giant);
		vn_close(fh->nf_vp, FREAD, td->td_ucred, td);
		mtx_unlock(&Giant);
	}

	fh->nf_vp = NULL;
	ExFreePool(fh);

	return;
}

__stdcall static uint8_t
NdisSystemProcessorCount()
{
	return(mp_ncpus);
}

typedef void (*ndis_statusdone_handler)(ndis_handle);
typedef void (*ndis_status_handler)(ndis_handle, ndis_status,
        void *, uint32_t);

__stdcall static void
NdisMIndicateStatusComplete(adapter)
	ndis_handle		adapter;
{
	ndis_miniport_block	*block;
	__stdcall ndis_statusdone_handler	statusdonefunc;

	block = (ndis_miniport_block *)adapter;
	statusdonefunc = block->nmb_statusdone_func;

	MSCALL1(statusdonefunc, adapter);
	return;
}

__stdcall static void
NdisMIndicateStatus(adapter, status, sbuf, slen)
	ndis_handle		adapter;
	ndis_status		status;
	void			*sbuf;
	uint32_t		slen;
{
	ndis_miniport_block	*block;
	__stdcall ndis_status_handler	statusfunc;

	block = (ndis_miniport_block *)adapter;
	statusfunc = block->nmb_status_func;

	MSCALL4(statusfunc, adapter, status, sbuf, slen);
	return;
}

static void
ndis_workfunc(ctx)
	void			*ctx;
{
	ndis_work_item		*work;
	__stdcall ndis_proc	workfunc;

	work = ctx;
	workfunc = work->nwi_func;
	MSCALL2(workfunc, work, work->nwi_ctx);
	return;
}

__stdcall static ndis_status
NdisScheduleWorkItem(work)
	ndis_work_item		*work;
{
	ndis_sched(ndis_workfunc, work, NDIS_TASKQUEUE);
	return(NDIS_STATUS_SUCCESS);
}

__stdcall static void
NdisCopyFromPacketToPacket(dpkt, doff, reqlen, spkt, soff, cpylen)
	ndis_packet		*dpkt;
	uint32_t		doff;
	uint32_t		reqlen;
	ndis_packet		*spkt;
	uint32_t		soff;
	uint32_t		*cpylen;
{
	ndis_buffer		*src, *dst;
	char			*sptr, *dptr;
	int			resid, copied, len, scnt, dcnt;

	*cpylen = 0;

	src = spkt->np_private.npp_head;
	dst = dpkt->np_private.npp_head;

	sptr = MmGetMdlVirtualAddress(src);
	dptr = MmGetMdlVirtualAddress(dst);
	scnt = MmGetMdlByteCount(src);
	dcnt = MmGetMdlByteCount(dst);

	while (soff) {
		if (MmGetMdlByteCount(src) > soff) {
			sptr += soff;
			scnt = MmGetMdlByteCount(src)- soff;
			break;
		}
		soff -= MmGetMdlByteCount(src);
		src = src->mdl_next;
		if (src == NULL)
			return;
		sptr = MmGetMdlVirtualAddress(src);
	}

	while (doff) {
		if (MmGetMdlByteCount(dst) > doff) {
			dptr += doff;
			dcnt = MmGetMdlByteCount(dst) - doff;
			break;
		}
		doff -= MmGetMdlByteCount(dst);
		dst = dst->mdl_next;
		if (dst == NULL)
			return;
		dptr = MmGetMdlVirtualAddress(dst);
	}

	resid = reqlen;
	copied = 0;

	while(1) {
		if (resid < scnt)
			len = resid;
		else
			len = scnt;
		if (dcnt < len)
			len = dcnt;

		bcopy(sptr, dptr, len);

		copied += len;
		resid -= len;
		if (resid == 0)
			break;

		dcnt -= len;
		if (dcnt == 0) {
			dst = dst->mdl_next;
			if (dst == NULL)
				break;
			dptr = MmGetMdlVirtualAddress(dst);
			dcnt = MmGetMdlByteCount(dst);
		}

		scnt -= len;
		if (scnt == 0) {
			src = src->mdl_next;
			if (src == NULL)
				break;
			sptr = MmGetMdlVirtualAddress(src);
			scnt = MmGetMdlByteCount(src);
		}
	}

	*cpylen = copied;
	return;
}

__stdcall static void
NdisCopyFromPacketToPacketSafe(dpkt, doff, reqlen, spkt, soff, cpylen, prio)
	ndis_packet		*dpkt;
	uint32_t		doff;
	uint32_t		reqlen;
	ndis_packet		*spkt;
	uint32_t		soff;
	uint32_t		*cpylen;
	uint32_t		prio;
{
	NdisCopyFromPacketToPacket(dpkt, doff, reqlen, spkt, soff, cpylen);
	return;
}

__stdcall static ndis_status
NdisMRegisterDevice(handle, devname, symname, majorfuncs, devobj, devhandle)
	ndis_handle		handle;
	ndis_unicode_string	*devname;
	ndis_unicode_string	*symname;
	driver_dispatch		*majorfuncs[];
	void			**devobj;
	ndis_handle		*devhandle;
{
	ndis_miniport_block	*block;

	block = (ndis_miniport_block *)handle;
	*devobj = block->nmb_deviceobj;
	*devhandle = handle;

	return(NDIS_STATUS_SUCCESS);
}

__stdcall static ndis_status
NdisMDeregisterDevice(handle)
	ndis_handle		handle;
{
	return(NDIS_STATUS_SUCCESS);
}

__stdcall static ndis_status
NdisMQueryAdapterInstanceName(name, handle)
	ndis_unicode_string	*name;
	ndis_handle		handle;
{
	ndis_miniport_block	*block;
	device_t		dev;

	block = (ndis_miniport_block *)handle;
	dev = block->nmb_physdeviceobj->do_devext;

	ndis_ascii_to_unicode(__DECONST(char *,
	    device_get_nameunit(dev)), &name->us_buf);
	name->us_len = strlen(device_get_nameunit(dev)) * 2;

	return(NDIS_STATUS_SUCCESS);
}

__stdcall static void
NdisMRegisterUnloadHandler(handle, func)
	ndis_handle		handle;
	void			*func;
{
	return;
}

__stdcall static void
dummy()
{
	printf ("NDIS dummy called...\n");
	return;
}

image_patch_table ndis_functbl[] = {
	IMPORT_FUNC(NdisCopyFromPacketToPacket),
	IMPORT_FUNC(NdisCopyFromPacketToPacketSafe),
	IMPORT_FUNC(NdisScheduleWorkItem),
	IMPORT_FUNC(NdisMIndicateStatusComplete),
	IMPORT_FUNC(NdisMIndicateStatus),
	IMPORT_FUNC(NdisSystemProcessorCount),
	IMPORT_FUNC(NdisUnchainBufferAtBack),
	IMPORT_FUNC(NdisGetFirstBufferFromPacket),
	IMPORT_FUNC(NdisGetFirstBufferFromPacketSafe),
	IMPORT_FUNC(NdisGetBufferPhysicalArraySize),
	IMPORT_FUNC(NdisMGetDeviceProperty),
	IMPORT_FUNC(NdisInitAnsiString),
	IMPORT_FUNC(NdisInitUnicodeString),
	IMPORT_FUNC(NdisWriteConfiguration),
	IMPORT_FUNC(NdisAnsiStringToUnicodeString),
	IMPORT_FUNC(NdisTerminateWrapper),
	IMPORT_FUNC(NdisOpenConfigurationKeyByName),
	IMPORT_FUNC(NdisOpenConfigurationKeyByIndex),
	IMPORT_FUNC(NdisMRemoveMiniport),
	IMPORT_FUNC(NdisInitializeString),	
	IMPORT_FUNC(NdisFreeString),	
	IMPORT_FUNC(NdisGetCurrentSystemTime),
	IMPORT_FUNC(NdisGetSystemUpTime),
	IMPORT_FUNC(NdisMSynchronizeWithInterrupt),
	IMPORT_FUNC(NdisMAllocateSharedMemoryAsync),
	IMPORT_FUNC(NdisInterlockedInsertHeadList),
	IMPORT_FUNC(NdisInterlockedInsertTailList),
	IMPORT_FUNC(NdisInterlockedRemoveHeadList),
	IMPORT_FUNC(NdisInitializeWrapper),
	IMPORT_FUNC(NdisMRegisterMiniport),
	IMPORT_FUNC(NdisAllocateMemoryWithTag),
	IMPORT_FUNC(NdisAllocateMemory),
	IMPORT_FUNC(NdisMSetAttributesEx),
	IMPORT_FUNC(NdisCloseConfiguration),
	IMPORT_FUNC(NdisReadConfiguration),
	IMPORT_FUNC(NdisOpenConfiguration),
	IMPORT_FUNC(NdisAcquireSpinLock),
	IMPORT_FUNC(NdisReleaseSpinLock),
	IMPORT_FUNC(NdisDprAcquireSpinLock),
	IMPORT_FUNC(NdisDprReleaseSpinLock),
	IMPORT_FUNC(NdisAllocateSpinLock),
	IMPORT_FUNC(NdisFreeSpinLock),
	IMPORT_FUNC(NdisFreeMemory),
	IMPORT_FUNC(NdisReadPciSlotInformation),
	IMPORT_FUNC(NdisWritePciSlotInformation),
	IMPORT_FUNC_MAP(NdisImmediateReadPciSlotInformation,
	    NdisReadPciSlotInformation),
	IMPORT_FUNC_MAP(NdisImmediateWritePciSlotInformation,
	    NdisWritePciSlotInformation),
	IMPORT_FUNC(NdisWriteErrorLogEntry),
	IMPORT_FUNC(NdisMStartBufferPhysicalMapping),
	IMPORT_FUNC(NdisMCompleteBufferPhysicalMapping),
	IMPORT_FUNC(NdisMInitializeTimer),
	IMPORT_FUNC(NdisInitializeTimer),
	IMPORT_FUNC(NdisSetTimer),
	IMPORT_FUNC(NdisMCancelTimer),
	IMPORT_FUNC_MAP(NdisCancelTimer, NdisMCancelTimer),
	IMPORT_FUNC(NdisMSetPeriodicTimer),
	IMPORT_FUNC(NdisMQueryAdapterResources),
	IMPORT_FUNC(NdisMRegisterIoPortRange),
	IMPORT_FUNC(NdisMDeregisterIoPortRange),
	IMPORT_FUNC(NdisReadNetworkAddress),
	IMPORT_FUNC(NdisQueryMapRegisterCount),
	IMPORT_FUNC(NdisMAllocateMapRegisters),
	IMPORT_FUNC(NdisMFreeMapRegisters),
	IMPORT_FUNC(NdisMAllocateSharedMemory),
	IMPORT_FUNC(NdisMMapIoSpace),
	IMPORT_FUNC(NdisMUnmapIoSpace),
	IMPORT_FUNC(NdisGetCacheFillSize),
	IMPORT_FUNC(NdisMGetDmaAlignment),
	IMPORT_FUNC(NdisMInitializeScatterGatherDma),
	IMPORT_FUNC(NdisAllocatePacketPool),
	IMPORT_FUNC(NdisAllocatePacketPoolEx),
	IMPORT_FUNC(NdisAllocatePacket),
	IMPORT_FUNC(NdisFreePacket),
	IMPORT_FUNC(NdisFreePacketPool),
	IMPORT_FUNC_MAP(NdisDprAllocatePacket, NdisAllocatePacket),
	IMPORT_FUNC_MAP(NdisDprFreePacket, NdisFreePacket),
	IMPORT_FUNC(NdisAllocateBufferPool),
	IMPORT_FUNC(NdisAllocateBuffer),
	IMPORT_FUNC(NdisQueryBuffer),
	IMPORT_FUNC(NdisQueryBufferSafe),
	IMPORT_FUNC(NdisBufferVirtualAddress),
	IMPORT_FUNC(NdisBufferVirtualAddressSafe),
	IMPORT_FUNC(NdisBufferLength),
	IMPORT_FUNC(NdisFreeBuffer),
	IMPORT_FUNC(NdisFreeBufferPool),
	IMPORT_FUNC(NdisInterlockedIncrement),
	IMPORT_FUNC(NdisInterlockedDecrement),
	IMPORT_FUNC(NdisInitializeEvent),
	IMPORT_FUNC(NdisSetEvent),
	IMPORT_FUNC(NdisResetEvent),
	IMPORT_FUNC(NdisWaitEvent),
	IMPORT_FUNC(NdisUnicodeStringToAnsiString),
	IMPORT_FUNC(NdisMPciAssignResources),
	IMPORT_FUNC(NdisMFreeSharedMemory),
	IMPORT_FUNC(NdisMRegisterInterrupt),
	IMPORT_FUNC(NdisMDeregisterInterrupt),
	IMPORT_FUNC(NdisMRegisterAdapterShutdownHandler),
	IMPORT_FUNC(NdisMDeregisterAdapterShutdownHandler),
	IMPORT_FUNC(NDIS_BUFFER_TO_SPAN_PAGES),
	IMPORT_FUNC(NdisQueryBufferOffset),
	IMPORT_FUNC(NdisAdjustBufferLength),
	IMPORT_FUNC(NdisPacketPoolUsage),
	IMPORT_FUNC(NdisMSleep),
	IMPORT_FUNC(NdisUnchainBufferAtFront),
	IMPORT_FUNC(NdisReadPcmciaAttributeMemory),
	IMPORT_FUNC(NdisWritePcmciaAttributeMemory),
	IMPORT_FUNC(NdisOpenFile),
	IMPORT_FUNC(NdisMapFile),
	IMPORT_FUNC(NdisUnmapFile),
	IMPORT_FUNC(NdisCloseFile),
	IMPORT_FUNC(NdisMRegisterDevice),
	IMPORT_FUNC(NdisMDeregisterDevice),
	IMPORT_FUNC(NdisMQueryAdapterInstanceName),
	IMPORT_FUNC(NdisMRegisterUnloadHandler),
	IMPORT_FUNC(ndis_timercall),

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
