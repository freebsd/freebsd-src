/*
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

#define FUNC void(*)(void)

static char ndis_filepath[MAXPATHLEN];
extern struct nd_head ndis_devhead;

SYSCTL_STRING(_hw, OID_AUTO, ndis_filepath, CTLFLAG_RW, ndis_filepath,
        MAXPATHLEN, "Path used by NdisOpenFile() to search for files");

__stdcall static void ndis_initwrap(ndis_handle *,
	device_object *, void *, void *);
__stdcall static ndis_status ndis_register_miniport(ndis_handle,
	ndis_miniport_characteristics *, int);
__stdcall static ndis_status ndis_malloc_withtag(void **, uint32_t, uint32_t);
__stdcall static ndis_status ndis_malloc(void **,
	uint32_t, uint32_t, ndis_physaddr);
__stdcall static void ndis_free(void *, uint32_t, uint32_t);
__stdcall static ndis_status ndis_setattr_ex(ndis_handle, ndis_handle,
	uint32_t, uint32_t, ndis_interface_type);
__stdcall static void ndis_open_cfg(ndis_status *, ndis_handle *, ndis_handle);
__stdcall static void ndis_open_cfgbyidx(ndis_status *, ndis_handle,
	uint32_t, ndis_unicode_string *, ndis_handle *);
__stdcall static void ndis_open_cfgbyname(ndis_status *, ndis_handle,
	ndis_unicode_string *, ndis_handle *);
static ndis_status ndis_encode_parm(ndis_miniport_block *,
	struct sysctl_oid *, ndis_parm_type, ndis_config_parm **);
static ndis_status ndis_decode_parm(ndis_miniport_block *,
	ndis_config_parm *, char *);
__stdcall static void ndis_read_cfg(ndis_status *, ndis_config_parm **,
	ndis_handle, ndis_unicode_string *, ndis_parm_type);
__stdcall static void ndis_write_cfg(ndis_status *, ndis_handle,
	ndis_unicode_string *, ndis_config_parm *);
__stdcall static void ndis_close_cfg(ndis_handle);
__stdcall static void ndis_create_lock(ndis_spin_lock *);
__stdcall static void ndis_destroy_lock(ndis_spin_lock *);
__stdcall static void ndis_lock(ndis_spin_lock *);
__stdcall static void ndis_unlock(ndis_spin_lock *);
__stdcall static void ndis_lock_dpr(ndis_spin_lock *);
__stdcall static void ndis_unlock_dpr(ndis_spin_lock *);
__stdcall static uint32_t ndis_read_pci(ndis_handle, uint32_t,
	uint32_t, void *, uint32_t);
__stdcall static uint32_t ndis_write_pci(ndis_handle, uint32_t,
	uint32_t, void *, uint32_t);
static void ndis_syslog(ndis_handle, ndis_error_code, uint32_t, ...);
static void ndis_map_cb(void *, bus_dma_segment_t *, int, int);
__stdcall static void ndis_vtophys_load(ndis_handle, ndis_buffer *,
	uint32_t, uint8_t, ndis_paddr_unit *, uint32_t *);
__stdcall static void ndis_vtophys_unload(ndis_handle, ndis_buffer *, uint32_t);
__stdcall static void ndis_create_timer(ndis_miniport_timer *, ndis_handle,
	ndis_timer_function, void *);
__stdcall static void ndis_init_timer(ndis_timer *,
	ndis_timer_function, void *);
__stdcall static void ndis_set_timer(ndis_timer *, uint32_t);
__stdcall static void ndis_set_periodic_timer(ndis_miniport_timer *, uint32_t);
__stdcall static void ndis_cancel_timer(ndis_timer *, uint8_t *);
__stdcall static void ndis_query_resources(ndis_status *, ndis_handle,
	ndis_resource_list *, uint32_t *);
__stdcall static ndis_status ndis_register_ioport(void **,
	ndis_handle, uint32_t, uint32_t);
__stdcall static void ndis_deregister_ioport(ndis_handle,
	uint32_t, uint32_t, void *);
__stdcall static void ndis_read_netaddr(ndis_status *, void **,
	uint32_t *, ndis_handle);
__stdcall static ndis_status ndis_mapreg_cnt(uint32_t, uint32_t *);
__stdcall static ndis_status ndis_alloc_mapreg(ndis_handle,
	uint32_t, uint8_t, uint32_t, uint32_t);
__stdcall static void ndis_free_mapreg(ndis_handle);
static void ndis_mapshared_cb(void *, bus_dma_segment_t *, int, int);
__stdcall static void ndis_alloc_sharedmem(ndis_handle, uint32_t,
	uint8_t, void **, ndis_physaddr *);
static void ndis_asyncmem_complete(void *);
__stdcall static ndis_status ndis_alloc_sharedmem_async(ndis_handle,
	uint32_t, uint8_t, void *);
__stdcall static void ndis_free_sharedmem(ndis_handle, uint32_t,
	uint8_t, void *, ndis_physaddr);
__stdcall static ndis_status ndis_map_iospace(void **, ndis_handle,
	ndis_physaddr, uint32_t);
__stdcall static void ndis_unmap_iospace(ndis_handle, void *, uint32_t);
__stdcall static uint32_t ndis_cachefill(void);
__stdcall static uint32_t ndis_dma_align(ndis_handle);
__stdcall static ndis_status ndis_init_sc_dma(ndis_handle,
	uint8_t, uint32_t);
__stdcall static void ndis_alloc_packetpool(ndis_status *,
	ndis_handle *, uint32_t, uint32_t);
__stdcall static void ndis_ex_alloc_packetpool(ndis_status *,
	ndis_handle *, uint32_t, uint32_t, uint32_t);
__stdcall static uint32_t ndis_packetpool_use(ndis_handle);
__stdcall static void ndis_free_packetpool(ndis_handle);
__stdcall static void ndis_alloc_packet(ndis_status *,
	ndis_packet **, ndis_handle);
__stdcall static void ndis_release_packet(ndis_packet *);
__stdcall static void ndis_unchain_headbuf(ndis_packet *, ndis_buffer **);
__stdcall static void ndis_unchain_tailbuf(ndis_packet *, ndis_buffer **);
__stdcall static void ndis_alloc_bufpool(ndis_status *,
	ndis_handle *, uint32_t);
__stdcall static void ndis_free_bufpool(ndis_handle);
__stdcall static void ndis_alloc_buf(ndis_status *, ndis_buffer **,
	ndis_handle, void *, uint32_t);
__stdcall static void ndis_release_buf(ndis_buffer *);
__stdcall static uint32_t ndis_buflen(ndis_buffer *);
__stdcall static void ndis_query_buf(ndis_buffer *, void **, uint32_t *);
__stdcall static void ndis_query_buf_safe(ndis_buffer *, void **,
	uint32_t *, uint32_t);
__stdcall static void *ndis_buf_vaddr(ndis_buffer *);
__stdcall static void *ndis_buf_vaddr_safe(ndis_buffer *, uint32_t);
__stdcall static void ndis_adjust_buflen(ndis_buffer *, int);
__stdcall static uint32_t ndis_interlock_inc(uint32_t *);
__stdcall static uint32_t ndis_interlock_dec(uint32_t *);
__stdcall static void ndis_init_event(ndis_event *);
__stdcall static void ndis_set_event(ndis_event *);
__stdcall static void ndis_reset_event(ndis_event *);
__stdcall static uint8_t ndis_wait_event(ndis_event *, uint32_t);
__stdcall static ndis_status ndis_unicode2ansi(ndis_ansi_string *,
	ndis_unicode_string *);
__stdcall static ndis_status ndis_ansi2unicode(ndis_unicode_string *,
	ndis_ansi_string *);
__stdcall static ndis_status ndis_assign_pcirsrc(ndis_handle,
	uint32_t, ndis_resource_list **);
__stdcall static ndis_status ndis_register_intr(ndis_miniport_interrupt *,
	ndis_handle, uint32_t, uint32_t, uint8_t,
	uint8_t, ndis_interrupt_mode);
__stdcall static void ndis_deregister_intr(ndis_miniport_interrupt *);
__stdcall static void ndis_register_shutdown(ndis_handle, void *,
	ndis_shutdown_handler);
__stdcall static void ndis_deregister_shutdown(ndis_handle);
__stdcall static uint32_t ndis_numpages(ndis_buffer *);
__stdcall static void ndis_buf_physpages(ndis_buffer *, uint32_t *);
__stdcall static void ndis_query_bufoffset(ndis_buffer *,
	uint32_t *, uint32_t *);
__stdcall static void ndis_sleep(uint32_t);
__stdcall static uint32_t ndis_read_pccard_amem(ndis_handle,
	uint32_t, void *, uint32_t);
__stdcall static uint32_t ndis_write_pccard_amem(ndis_handle,
	uint32_t, void *, uint32_t);
__stdcall static list_entry *ndis_insert_head(list_entry *,
	list_entry *, ndis_spin_lock *);
__stdcall static list_entry *ndis_remove_head(list_entry *,
	ndis_spin_lock *);
__stdcall static list_entry *ndis_insert_tail(list_entry *,
	list_entry *, ndis_spin_lock *);
__stdcall static uint8_t ndis_sync_with_intr(ndis_miniport_interrupt *,
	void *, void *);
__stdcall static void ndis_time(uint64_t *);
__stdcall static void ndis_uptime(uint32_t *);
__stdcall static void ndis_init_string(ndis_unicode_string *, char *);
__stdcall static void ndis_init_ansi_string(ndis_ansi_string *, char *);
__stdcall static void ndis_init_unicode_string(ndis_unicode_string *,
	uint16_t *);
__stdcall static void ndis_free_string(ndis_unicode_string *);
__stdcall static ndis_status ndis_remove_miniport(ndis_handle *);
__stdcall static void ndis_termwrap(ndis_handle, void *);
__stdcall static void ndis_get_devprop(ndis_handle, device_object **,
	device_object **, device_object **, cm_resource_list *,
	cm_resource_list *);
__stdcall static void ndis_firstbuf(ndis_packet *, ndis_buffer **,
	void **, uint32_t *, uint32_t *);
__stdcall static void ndis_firstbuf_safe(ndis_packet *, ndis_buffer **,
	void **, uint32_t *, uint32_t *, uint32_t);
static int ndis_find_sym(linker_file_t, char *, char *, caddr_t *);
__stdcall static void ndis_open_file(ndis_status *, ndis_handle *, uint32_t *,
	ndis_unicode_string *, ndis_physaddr);
__stdcall static void ndis_map_file(ndis_status *, void **, ndis_handle);
__stdcall static void ndis_unmap_file(ndis_handle);
__stdcall static void ndis_close_file(ndis_handle);
__stdcall static u_int8_t ndis_cpu_cnt(void);
__stdcall static void ndis_ind_statusdone(ndis_handle);
__stdcall static void ndis_ind_status(ndis_handle, ndis_status,
        void *, uint32_t);
static void ndis_workfunc(void *);
__stdcall static ndis_status ndis_sched_workitem(ndis_work_item *);
__stdcall static void ndis_pkt_to_pkt(ndis_packet *, uint32_t, uint32_t,
	ndis_packet *, uint32_t, uint32_t *);
__stdcall static void ndis_pkt_to_pkt_safe(ndis_packet *, uint32_t, uint32_t,
	ndis_packet *, uint32_t, uint32_t *, uint32_t);
__stdcall static ndis_status ndis_register_dev(ndis_handle,
	ndis_unicode_string *, ndis_unicode_string *, driver_dispatch **,
	void **, ndis_handle *);
__stdcall static ndis_status ndis_deregister_dev(ndis_handle);
__stdcall static ndis_status ndis_query_name(ndis_unicode_string *,
	ndis_handle);
__stdcall static void ndis_register_unload(ndis_handle, void *);
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
	strcpy(ndis_filepath, "/compat/ndis");
	return(0);
}

int
ndis_libfini()
{
	return(0);
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
		*unicode = malloc(strlen(ascii) * 2, M_DEVBUF, M_WAITOK);

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
		*ascii = malloc((ulen / 2) + 1, M_DEVBUF, M_WAITOK|M_ZERO);
	if (*ascii == NULL)
		return(ENOMEM);
	astr = *ascii;
	for (i = 0; i < ulen / 2; i++) {
		*astr = (uint8_t)unicode[i];
		astr++;
	}

	return(0);
}

__stdcall static void
ndis_initwrap(wrapper, drv_obj, path, unused)
	ndis_handle		*wrapper;
	device_object		*drv_obj;
	void			*path;
	void			*unused;
{
	ndis_miniport_block	*block;

	block = drv_obj->do_rsvd;
	*wrapper = block;

	return;
}

__stdcall static void
ndis_termwrap(handle, syspec)
	ndis_handle		handle;
	void			*syspec;
{
	return;
}

__stdcall static ndis_status
ndis_register_miniport(handle, characteristics, len)
	ndis_handle		handle;
	ndis_miniport_characteristics *characteristics;
	int			len;
{
	ndis_miniport_block	*block;
	struct ndis_softc	*sc;

	block = (ndis_miniport_block *)handle;
	sc = (struct ndis_softc *)block->nmb_ifp;
	bcopy((char *)characteristics, (char *)&sc->ndis_chars,
	    sizeof(ndis_miniport_characteristics));
	if (sc->ndis_chars.nmc_version_major < 5 ||
	    sc->ndis_chars.nmc_version_minor < 1) {
		sc->ndis_chars.nmc_shutdown_handler = NULL;
		sc->ndis_chars.nmc_canceltxpkts_handler = NULL;
		sc->ndis_chars.nmc_pnpevent_handler = NULL;
	}

	return(NDIS_STATUS_SUCCESS);
}

__stdcall static ndis_status
ndis_malloc_withtag(vaddr, len, tag)
	void			**vaddr;
	uint32_t		len;
	uint32_t		tag;
{
	void			*mem;

	mem = malloc(len, M_DEVBUF, M_NOWAIT);
	if (mem == NULL)
		return(NDIS_STATUS_RESOURCES);
	*vaddr = mem;

	return(NDIS_STATUS_SUCCESS);
}

__stdcall static ndis_status
ndis_malloc(vaddr, len, flags, highaddr)
	void			**vaddr;
	uint32_t		len;
	uint32_t		flags;
	ndis_physaddr		highaddr;
{
	void			*mem;

	mem = malloc(len, M_DEVBUF, M_NOWAIT);
	if (mem == NULL)
		return(NDIS_STATUS_RESOURCES);
	*vaddr = mem;

	return(NDIS_STATUS_SUCCESS);
}

__stdcall static void
ndis_free(vaddr, len, flags)
	void			*vaddr;
	uint32_t		len;
	uint32_t		flags;
{
	if (len == 0)
		return;
	free(vaddr, M_DEVBUF);

	return;
}

__stdcall static ndis_status
ndis_setattr_ex(adapter_handle, adapter_ctx, hangsecs,
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
ndis_open_cfg(status, cfg, wrapctx)
	ndis_status		*status;
	ndis_handle		*cfg;
	ndis_handle		wrapctx;
{
	*cfg = wrapctx;
	*status = NDIS_STATUS_SUCCESS;
	return;
}

__stdcall static void
ndis_open_cfgbyname(status, cfg, subkey, subhandle)
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
ndis_open_cfgbyidx(status, cfg, idx, subkey, subhandle)
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
		ustr->nus_len = strlen((char *)oid->oid_arg1) * 2;
		ustr->nus_buf = unicode;
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
ndis_read_cfg(status, parm, cfg, key, type)
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
	sc = (struct ndis_softc *)block->nmb_ifp;

	if (key->nus_len == 0 || key->nus_buf == NULL) {
		*status = NDIS_STATUS_FAILURE;
		return;
	}

	ndis_unicode_to_ascii(key->nus_buf, key->nus_len, &keystr);

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
		ndis_unicode_to_ascii(ustr->nus_buf, ustr->nus_len, &astr);
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
ndis_write_cfg(status, cfg, key, parm)
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
	sc = (struct ndis_softc *)block->nmb_ifp;

	ndis_unicode_to_ascii(key->nus_buf, key->nus_len, &keystr);

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
ndis_close_cfg(cfg)
	ndis_handle		cfg;
{
	return;
}

/*
 * Initialize a Windows spinlock.
 */
__stdcall static void
ndis_create_lock(lock)
	ndis_spin_lock		*lock;
{
	lock->nsl_spinlock = 0;
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
ndis_destroy_lock(lock)
	ndis_spin_lock		*lock;
{
#ifdef notdef
	lock->nsl_spinlock = 0;
	lock->nsl_kirql = 0;
#endif
	return;
}

/*
 * Acquire a spinlock from IRQL <= DISPATCH_LEVEL.
 */

__stdcall static void
ndis_lock(lock)
	ndis_spin_lock		*lock;
{
	lock->nsl_kirql = FASTCALL2(hal_lock,
	    &lock->nsl_spinlock, DISPATCH_LEVEL);
	return;
}

/*
 * Release a spinlock from IRQL == DISPATCH_LEVEL.
 */

__stdcall static void
ndis_unlock(lock)
	ndis_spin_lock		*lock;
{
	FASTCALL2(hal_unlock, &lock->nsl_spinlock, lock->nsl_kirql);
	return;
}

/*
 * Acquire a spinlock when already running at IRQL == DISPATCH_LEVEL.
 */
__stdcall static void
ndis_lock_dpr(lock)
	ndis_spin_lock		*lock;
{
	FASTCALL1(ntoskrnl_lock_dpc, &lock->nsl_spinlock);
	return;
}

/*
 * Release a spinlock without leaving IRQL == DISPATCH_LEVEL.
 */
__stdcall static void
ndis_unlock_dpr(lock)
	ndis_spin_lock		*lock;
{
	FASTCALL1(ntoskrnl_unlock_dpc, &lock->nsl_spinlock);
	return;
}

__stdcall static uint32_t
ndis_read_pci(adapter, slot, offset, buf, len)
	ndis_handle		adapter;
	uint32_t		slot;
	uint32_t		offset;
	void			*buf;
	uint32_t		len;
{
	ndis_miniport_block	*block;
	int			i;
	char			*dest;

	block = (ndis_miniport_block *)adapter;
	dest = buf;
	if (block == NULL || block->nmb_dev == NULL)
		return(0);

	for (i = 0; i < len; i++)
		dest[i] = pci_read_config(block->nmb_dev, i + offset, 1);

	return(len);
}

__stdcall static uint32_t
ndis_write_pci(adapter, slot, offset, buf, len)
	ndis_handle		adapter;
	uint32_t		slot;
	uint32_t		offset;
	void			*buf;
	uint32_t		len;
{
	ndis_miniport_block	*block;
	int			i;
	char			*dest;

	block = (ndis_miniport_block *)adapter;
	dest = buf;

	if (block == NULL || block->nmb_dev == NULL)
		return(0);

	for (i = 0; i < len; i++)
		pci_write_config(block->nmb_dev, i + offset, dest[i], 1);

	return(len);
}

/*
 * The errorlog routine uses a variable argument list, so we
 * have to declare it this way.
 */
#define ERRMSGLEN 512
static void
ndis_syslog(ndis_handle adapter, ndis_error_code code,
	uint32_t numerrors, ...)
{
	ndis_miniport_block	*block;
	va_list			ap;
	int			i, error;
	char			*str = NULL, *ustr = NULL;
	uint16_t		flags;
	char			msgbuf[ERRMSGLEN];


	block = (ndis_miniport_block *)adapter;

	error = pe_get_message(block->nmb_img, code, &str, &i, &flags);
	if (error == 0 && flags & MESSAGE_RESOURCE_UNICODE) {
		ustr = msgbuf;
		ndis_unicode_to_ascii((uint16_t *)str,
		    ((i / 2)) > (ERRMSGLEN - 1) ? ERRMSGLEN : i, &ustr);
		str = ustr;
	}
	device_printf (block->nmb_dev, "NDIS ERROR: %x (%s)\n", code,
	    str == NULL ? "unknown error" : str);
	device_printf (block->nmb_dev, "NDIS NUMERRORS: %x\n", numerrors);

	va_start(ap, numerrors);
	for (i = 0; i < numerrors; i++)
		device_printf (block->nmb_dev, "argptr: %p\n",
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
ndis_vtophys_load(adapter, buf, mapreg, writedev, addrarray, arraysize)
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
	sc = (struct ndis_softc *)(block->nmb_ifp);

	if (mapreg > sc->ndis_mmapcnt)
		return;

	map = sc->ndis_mmaps[mapreg];
	nma.nma_fraglist = addrarray;

	error = bus_dmamap_load(sc->ndis_mtag, map,
	    MDL_VA(buf), buf->nb_bytecount, ndis_map_cb,
	    (void *)&nma, BUS_DMA_NOWAIT);

	if (error)
		return;

	bus_dmamap_sync(sc->ndis_mtag, map,
	    writedev ? BUS_DMASYNC_PREWRITE : BUS_DMASYNC_PREREAD);

	*arraysize = nma.nma_cnt;

	return;
}

__stdcall static void
ndis_vtophys_unload(adapter, buf, mapreg)
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
	sc = (struct ndis_softc *)(block->nmb_ifp);

	if (mapreg > sc->ndis_mmapcnt)
		return;

	map = sc->ndis_mmaps[mapreg];

	bus_dmamap_sync(sc->ndis_mtag, map,
	    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

	bus_dmamap_unload(sc->ndis_mtag, map);

	return;
}

/*
 * This is an older pre-miniport timer init routine which doesn't
 * accept a miniport context handle. The function context (ctx)
 * is supposed to be a pointer to the adapter handle, which should
 * have been handed to us via NdisSetAttributesEx(). We use this
 * function context to track down the corresponding ndis_miniport_block
 * structure. It's vital that we track down the miniport block structure,
 * so if we can't do it, we panic. Note that we also play some games
 * here by treating ndis_timer and ndis_miniport_timer as the same
 * thing.
 */

__stdcall static void
ndis_init_timer(timer, func, ctx)
	ndis_timer		*timer;
	ndis_timer_function	func;
	void			*ctx;
{
	ntoskrnl_init_timer(&timer->nt_ktimer);
	ntoskrnl_init_dpc(&timer->nt_kdpc, func, ctx);

	return;
}

__stdcall static void
ndis_create_timer(timer, handle, func, ctx)
	ndis_miniport_timer	*timer;
	ndis_handle		handle;
	ndis_timer_function	func;
	void			*ctx;
{
	/* Save the funcptr and context */

	timer->nmt_timerfunc = func;
	timer->nmt_timerctx = ctx;
	timer->nmt_block = handle;

	ntoskrnl_init_timer(&timer->nmt_ktimer);
	ntoskrnl_init_dpc(&timer->nmt_kdpc, func, ctx);

	return;
}

/*
 * In Windows, there's both an NdisMSetTimer() and an NdisSetTimer(),
 * but the former is just a macro wrapper around the latter.
 */
__stdcall static void
ndis_set_timer(timer, msecs)
	ndis_timer		*timer;
	uint32_t		msecs;
{
	/*
	 * KeSetTimer() wants the period in
	 * hundred nanosecond intervals.
	 */
	ntoskrnl_set_timer(&timer->nt_ktimer,
	    ((int64_t)msecs * -10000), &timer->nt_kdpc);

	return;
}

__stdcall static void
ndis_set_periodic_timer(timer, msecs)
	ndis_miniport_timer	*timer;
	uint32_t		msecs;
{
	ntoskrnl_set_timer_ex(&timer->nmt_ktimer,
	    ((int64_t)msecs * -10000), msecs, &timer->nmt_kdpc);

	return;
}

/*
 * Technically, this is really NdisCancelTimer(), but we also
 * (ab)use it for NdisMCancelTimer(), since in our implementation
 * we don't need the extra info in the ndis_miniport_timer
 * structure.
 */

__stdcall static void
ndis_cancel_timer(timer, cancelled)
	ndis_timer		*timer;
	uint8_t			*cancelled;
{
	*cancelled = ntoskrnl_cancel_timer(&timer->nt_ktimer);

	return;
}

__stdcall static void
ndis_query_resources(status, adapter, list, buflen)
	ndis_status		*status;
	ndis_handle		adapter;
	ndis_resource_list	*list;
	uint32_t		*buflen;
{
	ndis_miniport_block	*block;
	struct ndis_softc	*sc;
	int			rsclen;

	block = (ndis_miniport_block *)adapter;
	sc = (struct ndis_softc *)block->nmb_ifp;

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
ndis_register_ioport(offset, adapter, port, numports)
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
	sc = (struct ndis_softc *)(block->nmb_ifp);

	if (sc->ndis_res_io == NULL)
		return(NDIS_STATUS_FAILURE);

	/* Don't let the device map more ports than we have. */
	if (rman_get_size(sc->ndis_res_io) < numports)
		return(NDIS_STATUS_INVALID_LENGTH);

	*offset = (void *)rman_get_start(sc->ndis_res_io);

	return(NDIS_STATUS_SUCCESS);
}

__stdcall static void
ndis_deregister_ioport(adapter, port, numports, offset)
	ndis_handle		adapter;
	uint32_t		port;
	uint32_t		numports;
	void			*offset;
{
	return;
}

__stdcall static void
ndis_read_netaddr(status, addr, addrlen, adapter)
	ndis_status		*status;
	void			**addr;
	uint32_t		*addrlen;
	ndis_handle		adapter;
{
	struct ndis_softc	*sc;
	ndis_miniport_block	*block;
	uint8_t			empty[] = { 0, 0, 0, 0, 0, 0 };

	block = (ndis_miniport_block *)adapter;
	sc = (struct ndis_softc *)block->nmb_ifp;

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
ndis_mapreg_cnt(bustype, cnt)
	uint32_t		bustype;
	uint32_t		*cnt;
{
	*cnt = 8192;
	return(NDIS_STATUS_SUCCESS);
}

__stdcall static ndis_status
ndis_alloc_mapreg(adapter, dmachannel, dmasize, physmapneeded, maxmap)
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
	sc = (struct ndis_softc *)block->nmb_ifp;

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
ndis_free_mapreg(adapter)
	ndis_handle		adapter;
{
	struct ndis_softc	*sc;
	ndis_miniport_block	*block;
	int			i;

	block = (ndis_miniport_block *)adapter;
	sc = (struct ndis_softc *)block->nmb_ifp;

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
ndis_alloc_sharedmem(adapter, len, cached, vaddr, paddr)
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
	sc = (struct ndis_softc *)(block->nmb_ifp);

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
	 * that lie above the 1GB mark. I don't know if this is a
	 * hardware limitation or if the addresses are being truncated
	 * within the driver, but this seems to be the only way to
	 * make these cards work reliably in systems with more than
	 * 1GB of physical memory.
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
	sc = (struct ndis_softc *)(block->nmb_ifp);

	vaddr = NULL;
	paddr.np_quad = 0;

	donefunc = sc->ndis_chars.nmc_allocate_complete_func;
	ndis_alloc_sharedmem(w->na_adapter, w->na_len,
	    w->na_cached, &vaddr, &paddr);
	donefunc(w->na_adapter, vaddr, &paddr, w->na_len, w->na_ctx);

	free(arg, M_DEVBUF);

	return;
}

__stdcall static ndis_status
ndis_alloc_sharedmem_async(adapter, len, cached, ctx)
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
ndis_free_sharedmem(adapter, len, cached, vaddr, paddr)
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
	sc = (struct ndis_softc *)(block->nmb_ifp);
	sh = prev = sc->ndis_shlist;

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
ndis_map_iospace(vaddr, adapter, paddr, len)
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
	sc = (struct ndis_softc *)(block->nmb_ifp);

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
ndis_unmap_iospace(adapter, vaddr, len)
	ndis_handle		adapter;
	void			*vaddr;
	uint32_t		len;
{
	return;
}

__stdcall static uint32_t
ndis_cachefill(void)
{
	return(128);
}

__stdcall static uint32_t
ndis_dma_align(handle)
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
ndis_init_sc_dma(adapter, is64, maxphysmap)
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
	sc = (struct ndis_softc *)block->nmb_ifp;

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

__stdcall static void
ndis_alloc_packetpool(status, pool, descnum, protrsvdlen)
	ndis_status		*status;
	ndis_handle		*pool;
	uint32_t		descnum;
	uint32_t		protrsvdlen;
{
	ndis_packet		*cur;
	int			i;

	*pool = malloc(sizeof(ndis_packet) *
	    ((descnum + NDIS_POOL_EXTRA) + 1),
	    M_DEVBUF, M_NOWAIT|M_ZERO);

	if (pool == NULL) {
		*status = NDIS_STATUS_RESOURCES;
		return;
	}

	cur = (ndis_packet *)*pool;
	cur->np_private.npp_flags = 0x1; /* mark the head of the list */
	cur->np_private.npp_totlen = 0; /* init deletetion flag */
	for (i = 0; i < (descnum + NDIS_POOL_EXTRA); i++) {
		cur->np_private.npp_head = (ndis_handle)(cur + 1);
		cur++;
	}

	*status = NDIS_STATUS_SUCCESS;
	return;
}

__stdcall static void
ndis_ex_alloc_packetpool(status, pool, descnum, oflowdescnum, protrsvdlen)
	ndis_status		*status;
	ndis_handle		*pool;
	uint32_t		descnum;
	uint32_t		oflowdescnum;
	uint32_t		protrsvdlen;
{
	return(ndis_alloc_packetpool(status, pool,
	    descnum + oflowdescnum, protrsvdlen));
}

__stdcall static uint32_t
ndis_packetpool_use(pool)
	ndis_handle		pool;
{
	ndis_packet		*head;

	head = (ndis_packet *)pool;

	return(head->np_private.npp_count);
}

__stdcall static void
ndis_free_packetpool(pool)
	ndis_handle		pool;
{
	ndis_packet		*head;

	head = pool;

	/* Mark this pool as 'going away.' */

	head->np_private.npp_totlen = 1;

	/* If there are no buffers loaned out, destroy the pool. */

	if (head->np_private.npp_count == 0)
		free(pool, M_DEVBUF);
	else
		printf("NDIS: buggy driver deleting active packet pool!\n");

	return;
}

__stdcall static void
ndis_alloc_packet(status, packet, pool)
	ndis_status		*status;
	ndis_packet		**packet;
	ndis_handle		pool;
{
	ndis_packet		*head, *pkt;

	head = (ndis_packet *)pool;

	if (head->np_private.npp_flags != 0x1) {
		*status = NDIS_STATUS_FAILURE;
		return;
	}

	/*
	 * If this pool is marked as 'going away' don't allocate any
	 * more packets out of it.
	 */

	if (head->np_private.npp_totlen) {
		*status = NDIS_STATUS_FAILURE;
		return;
	}

	pkt = (ndis_packet *)head->np_private.npp_head;

	if (pkt == NULL) {
		*status = NDIS_STATUS_RESOURCES;
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
	 * NDIS_GET_PACKET_MEDIA_SPECIFIC_INFO() to work correctly.
	 */
	pkt->np_private.npp_ndispktflags = NDIS_PACKET_ALLOCATED_BY_NDIS;

	*packet = pkt;

	head->np_private.npp_count++;
	*status = NDIS_STATUS_SUCCESS;
	return;
}

__stdcall static void
ndis_release_packet(packet)
	ndis_packet		*packet;
{
	ndis_packet		*head;

	if (packet == NULL || packet->np_private.npp_pool == NULL)
		return;

	head = packet->np_private.npp_pool;
	if (head->np_private.npp_flags != 0x1)
		return;

	packet->np_private.npp_head = head->np_private.npp_head;
	head->np_private.npp_head = (ndis_buffer *)packet;
	head->np_private.npp_count--;

	/*
	 * If the pool has been marked for deletion and there are
	 * no more packets outstanding, nuke the pool.
	 */

	if (head->np_private.npp_totlen && head->np_private.npp_count == 0)
		free(head, M_DEVBUF);

	return;
}

__stdcall static void
ndis_unchain_headbuf(packet, buf)
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
		priv->npp_head = (*buf)->nb_next;
	}

	return;
}

__stdcall static void
ndis_unchain_tailbuf(packet, buf)
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
		while (tmp->nb_next != priv->npp_tail)
			tmp = tmp->nb_next;
		priv->npp_tail = tmp;
		tmp->nb_next = NULL;
	}

	return;
}

/*
 * The NDIS "buffer" manipulation functions are somewhat misnamed.
 * They don't really allocate buffers: they allocate buffer mappings.
 * The idea is you reserve a chunk of DMA-able memory using
 * NdisMAllocateSharedMemory() and then use NdisAllocateBuffer()
 * to obtain the virtual address of the DMA-able region.
 * ndis_alloc_bufpool() is analagous to bus_dma_tag_create().
 */

__stdcall static void
ndis_alloc_bufpool(status, pool, descnum)
	ndis_status		*status;
	ndis_handle		*pool;
	uint32_t		descnum;
{
	ndis_buffer		*cur;
	int			i;

	*pool = malloc(sizeof(ndis_buffer) *
	    ((descnum + NDIS_POOL_EXTRA) + 1),
	    M_DEVBUF, M_NOWAIT|M_ZERO);

	if (pool == NULL) {
		*status = NDIS_STATUS_RESOURCES;
		return;
	}

	cur = (ndis_buffer *)*pool;
	cur->nb_flags = 0x1; /* mark the head of the list */
	cur->nb_bytecount = 0; /* init usage count */
	cur->nb_byteoffset = 0; /* init deletetion flag */
	for (i = 0; i < (descnum + NDIS_POOL_EXTRA); i++) {
		cur->nb_next = cur + 1;
		cur++;
	}

	*status = NDIS_STATUS_SUCCESS;
	return;
}

__stdcall static void
ndis_free_bufpool(pool)
	ndis_handle		pool;
{
	ndis_buffer		*head;

	head = pool;

	/* Mark this pool as 'going away.' */

	head->nb_byteoffset = 1;

	/* If there are no buffers loaned out, destroy the pool. */
	if (head->nb_bytecount == 0)
		free(pool, M_DEVBUF);
	else
		printf("NDIS: buggy driver deleting active buffer pool!\n");

	return;
}

/*
 * This maps to a bus_dmamap_create() and bus_dmamap_load().
 */
__stdcall static void
ndis_alloc_buf(status, buffer, pool, vaddr, len)
	ndis_status		*status;
	ndis_buffer		**buffer;
	ndis_handle		pool;
	void			*vaddr;
	uint32_t		len;
{
	ndis_buffer		*head, *buf;

	head = (ndis_buffer *)pool;
	if (head->nb_flags != 0x1) {
		*status = NDIS_STATUS_FAILURE;
		return;
	}

	/*
	 * If this pool is marked as 'going away' don't allocate any
	 * more buffers out of it.
	 */

	if (head->nb_byteoffset) {
		*status = NDIS_STATUS_FAILURE;
		return;
	}

	buf = head->nb_next;

	if (buf == NULL) {
		*status = NDIS_STATUS_RESOURCES;
		return;
	}

	head->nb_next = buf->nb_next;

	/* Save pointer to the pool. */
	buf->nb_process = head;

	MDL_INIT(buf, vaddr, len);

	*buffer = buf;

	/* Increment count of busy buffers. */

	head->nb_bytecount++;

	*status = NDIS_STATUS_SUCCESS;
	return;
}

__stdcall static void
ndis_release_buf(buf)
	ndis_buffer		*buf;
{
	ndis_buffer		*head;

	if (buf == NULL || buf->nb_process == NULL)
		return;

	head = buf->nb_process;

	if (head->nb_flags != 0x1)
		return;

	buf->nb_next = head->nb_next;
	head->nb_next = buf;

	/* Decrement count of busy buffers. */

	head->nb_bytecount--;

	/*
	 * If the pool has been marked for deletion and there are
	 * no more buffers outstanding, nuke the pool.
	 */

	if (head->nb_byteoffset && head->nb_bytecount == 0)
		free(head, M_DEVBUF);

	return;
}

/* Aw c'mon. */

__stdcall static uint32_t
ndis_buflen(buf)
	ndis_buffer		*buf;
{
	return(buf->nb_bytecount);
}

/*
 * Get the virtual address and length of a buffer.
 * Note: the vaddr argument is optional.
 */

__stdcall static void
ndis_query_buf(buf, vaddr, len)
	ndis_buffer		*buf;
	void			**vaddr;
	uint32_t		*len;
{
	if (vaddr != NULL)
		*vaddr = MDL_VA(buf);
	*len = buf->nb_bytecount;

	return;
}

/* Same as above -- we don't care about the priority. */

__stdcall static void
ndis_query_buf_safe(buf, vaddr, len, prio)
	ndis_buffer		*buf;
	void			**vaddr;
	uint32_t		*len;
	uint32_t		prio;
{
	if (vaddr != NULL)
		*vaddr = MDL_VA(buf);
	*len = buf->nb_bytecount;

	return;
}

/* Damnit Microsoft!! How many ways can you do the same thing?! */

__stdcall static void *
ndis_buf_vaddr(buf)
	ndis_buffer		*buf;
{
	return(MDL_VA(buf));
}

__stdcall static void *
ndis_buf_vaddr_safe(buf, prio)
	ndis_buffer		*buf;
	uint32_t		prio;
{
	return(MDL_VA(buf));
}

__stdcall static void
ndis_adjust_buflen(buf, len)
	ndis_buffer		*buf;
	int			len;
{
	buf->nb_bytecount = len;

	return;
}

__stdcall static uint32_t
ndis_interlock_inc(addend)
	uint32_t		*addend;
{
	atomic_add_long((u_long *)addend, 1);
	return(*addend);
}

__stdcall static uint32_t
ndis_interlock_dec(addend)
	uint32_t		*addend;
{
	atomic_subtract_long((u_long *)addend, 1);
	return(*addend);
}

__stdcall static void
ndis_init_event(event)
	ndis_event		*event;
{
	/*
	 * NDIS events are always notification
	 * events, and should be initialized to the
	 * not signaled state.
	 */
 
	ntoskrnl_init_event(&event->ne_event, EVENT_TYPE_NOTIFY, FALSE);
	return;
}

__stdcall static void
ndis_set_event(event)
	ndis_event		*event;
{
	ntoskrnl_set_event(&event->ne_event, 0, 0);
	return;
}

__stdcall static void
ndis_reset_event(event)
	ndis_event		*event;
{
	ntoskrnl_reset_event(&event->ne_event);
	return;
}

__stdcall static uint8_t
ndis_wait_event(event, msecs)
	ndis_event		*event;
	uint32_t		msecs;
{
	int64_t			duetime;
	uint32_t		rval;

	duetime = ((int64_t)msecs * -10000);

	rval = ntoskrnl_waitforobj((nt_dispatch_header *)event,
	    0, 0, TRUE, msecs ? &duetime : NULL);

	if (rval == STATUS_TIMEOUT)
		return(FALSE);

	return(TRUE);
}

__stdcall static ndis_status
ndis_unicode2ansi(dstr, sstr)
	ndis_ansi_string	*dstr;
	ndis_unicode_string	*sstr;
{
	if (dstr == NULL || sstr == NULL)
		return(NDIS_STATUS_FAILURE);
	if (ndis_unicode_to_ascii(sstr->nus_buf,
	    sstr->nus_len, &dstr->nas_buf))
		return(NDIS_STATUS_FAILURE);
	dstr->nas_len = dstr->nas_maxlen = strlen(dstr->nas_buf);
	return (NDIS_STATUS_SUCCESS);
}

__stdcall static ndis_status
ndis_ansi2unicode(dstr, sstr)
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
	if (ndis_ascii_to_unicode(str, &dstr->nus_buf)) {
		free(str, M_DEVBUF);
		return(NDIS_STATUS_FAILURE);
	}
	dstr->nus_len = dstr->nus_maxlen = sstr->nas_len * 2;
	free(str, M_DEVBUF);
	return (NDIS_STATUS_SUCCESS);
}

__stdcall static ndis_status
ndis_assign_pcirsrc(adapter, slot, list)
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
ndis_register_intr(intr, adapter, ivec, ilevel, reqisr, shared, imode)
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
	return(NDIS_STATUS_SUCCESS);
}	

__stdcall static void
ndis_deregister_intr(intr)
	ndis_miniport_interrupt	*intr;
{
	return;
}

__stdcall static void
ndis_register_shutdown(adapter, shutdownctx, shutdownfunc)
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
	sc = (struct ndis_softc *)block->nmb_ifp;
	chars = &sc->ndis_chars;

	chars->nmc_shutdown_handler = shutdownfunc;
	chars->nmc_rsvd0 = shutdownctx;

	return;
}

__stdcall static void
ndis_deregister_shutdown(adapter)
	ndis_handle		adapter;
{
	ndis_miniport_block	*block;
	ndis_miniport_characteristics *chars;
	struct ndis_softc	*sc;

	if (adapter == NULL)
		return;

	block = (ndis_miniport_block *)adapter;
	sc = (struct ndis_softc *)block->nmb_ifp;
	chars = &sc->ndis_chars;

	chars->nmc_shutdown_handler = NULL;
	chars->nmc_rsvd0 = NULL;

	return;
}

__stdcall static uint32_t
ndis_numpages(buf)
	ndis_buffer		*buf;
{
	if (buf == NULL)
		return(0);
	if (buf->nb_bytecount == 0)
		return(1);
	return(SPAN_PAGES(MDL_VA(buf), buf->nb_bytecount));
}

__stdcall static void
ndis_buf_physpages(buf, pages)
	ndis_buffer		*buf;
	uint32_t		*pages;
{
	if (buf == NULL)
		return;

	*pages = ndis_numpages(buf);
	return;
}

__stdcall static void
ndis_query_bufoffset(buf, off, len)
	ndis_buffer		*buf;
	uint32_t		*off;
	uint32_t		*len;
{
	if (buf == NULL)
		return;

	*off = buf->nb_byteoffset;
	*len = buf->nb_bytecount;

	return;
}

__stdcall static void
ndis_sleep(usecs)
	uint32_t		usecs;
{
	struct timeval		tv;

	tv.tv_sec = 0;
	tv.tv_usec = usecs;

	ndis_thsuspend(curthread->td_proc, tvtohz(&tv));

	return;
}

__stdcall static uint32_t
ndis_read_pccard_amem(handle, offset, buf, len)
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
	sc = (struct ndis_softc *)block->nmb_ifp;
	dest = buf;

	bh = rman_get_bushandle(sc->ndis_res_am);
	bt = rman_get_bustag(sc->ndis_res_am);

	for (i = 0; i < len; i++)
		dest[i] = bus_space_read_1(bt, bh, (offset + i) * 2);

	return(i);
}

__stdcall static uint32_t
ndis_write_pccard_amem(handle, offset, buf, len)
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
	sc = (struct ndis_softc *)block->nmb_ifp;
	src = buf;

	bh = rman_get_bushandle(sc->ndis_res_am);
	bt = rman_get_bustag(sc->ndis_res_am);

	for (i = 0; i < len; i++)
		bus_space_write_1(bt, bh, (offset + i) * 2, src[i]);

	return(i);
}

__stdcall static list_entry *
ndis_insert_head(head, entry, lock)
	list_entry		*head;
	list_entry		*entry;
	ndis_spin_lock		*lock;
{
	list_entry		*flink;

	lock->nsl_kirql = FASTCALL2(hal_lock,
	    &lock->nsl_spinlock, DISPATCH_LEVEL);
	flink = head->nle_flink;
	entry->nle_flink = flink;
	entry->nle_blink = head;
	flink->nle_blink = entry;
	head->nle_flink = entry;
	FASTCALL2(hal_unlock, &lock->nsl_spinlock, lock->nsl_kirql);

	return(flink);
}

__stdcall static list_entry *
ndis_remove_head(head, lock)
	list_entry		*head;
	ndis_spin_lock		*lock;
{
	list_entry		*flink;
	list_entry		*entry;

	lock->nsl_kirql = FASTCALL2(hal_lock,
	    &lock->nsl_spinlock, DISPATCH_LEVEL);
	entry = head->nle_flink;
	flink = entry->nle_flink;
	head->nle_flink = flink;
	flink->nle_blink = head;
	FASTCALL2(hal_unlock, &lock->nsl_spinlock, lock->nsl_kirql);

	return(entry);
}

__stdcall static list_entry *
ndis_insert_tail(head, entry, lock)
	list_entry		*head;
	list_entry		*entry;
	ndis_spin_lock		*lock;
{
	list_entry		*blink;

	lock->nsl_kirql = FASTCALL2(hal_lock,
	    &lock->nsl_spinlock, DISPATCH_LEVEL);
	blink = head->nle_blink;
	entry->nle_flink = head;
	entry->nle_blink = blink;
	blink->nle_flink = entry;
	head->nle_blink = entry;
	FASTCALL2(hal_unlock, &lock->nsl_spinlock, lock->nsl_kirql);

	return(blink);
}

__stdcall static uint8_t
ndis_sync_with_intr(intr, syncfunc, syncctx)
	ndis_miniport_interrupt	*intr;
	void			*syncfunc;
	void			*syncctx;
{
	struct ndis_softc	*sc;
	__stdcall uint8_t (*sync)(void *);
	uint8_t			rval;

	if (syncfunc == NULL || syncctx == NULL)
		return(0);

	sc = (struct ndis_softc *)intr->ni_block->nmb_ifp;
	sync = syncfunc;
	mtx_lock(&sc->ndis_intrmtx);
	rval = sync(syncctx);
	mtx_unlock(&sc->ndis_intrmtx);

	return(rval);
}

/*
 * Return the number of 100 nanosecond intervals since
 * January 1, 1601. (?!?!)
 */
__stdcall static void
ndis_time(tval)
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
ndis_uptime(tval)
	uint32_t		*tval;
{
	struct timespec		ts;

	nanouptime(&ts);
	*tval = ts.tv_nsec / 1000000 + ts.tv_sec * 1000;

	return;
}

__stdcall static void
ndis_init_string(dst, src)
	ndis_unicode_string	*dst;
	char			*src;
{
	ndis_unicode_string	*u;

	u = dst;
	u->nus_buf = NULL;
	if (ndis_ascii_to_unicode(src, &u->nus_buf))
		return;
	u->nus_len = u->nus_maxlen = strlen(src) * 2;
	return;
}

__stdcall static void
ndis_free_string(str)
	ndis_unicode_string	*str;
{
	if (str == NULL)
		return;
	if (str->nus_buf != NULL)
		free(str->nus_buf, M_DEVBUF);
	free(str, M_DEVBUF);
	return;
}

__stdcall static ndis_status
ndis_remove_miniport(adapter)
	ndis_handle		*adapter;
{
	return(NDIS_STATUS_SUCCESS);
}

__stdcall static void
ndis_init_ansi_string(dst, src)
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
ndis_init_unicode_string(dst, src)
	ndis_unicode_string	*dst;
	uint16_t		*src;
{
	ndis_unicode_string	*u;
	int			i;

	u = dst;
	if (u == NULL)
		return;
	if (src == NULL) {
		u->nus_len = u->nus_maxlen = 0;
		u->nus_buf = NULL;
	} else {
		i = 0;
		while(src[i] != 0)
			i++;
		u->nus_buf = src;
		u->nus_len = u->nus_maxlen = i * 2;
	}

	return;
}

__stdcall static void ndis_get_devprop(adapter, phydevobj,
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
		*phydevobj = &block->nmb_devobj;
	if (funcdevobj != NULL)
		*funcdevobj = &block->nmb_devobj;

	return;
}

__stdcall static void
ndis_firstbuf(packet, buf, firstva, firstlen, totlen)
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
		*firstva = MDL_VA(tmp);
		*firstlen = *totlen = tmp->nb_bytecount;
		for (tmp = tmp->nb_next; tmp != NULL; tmp = tmp->nb_next)
			*totlen += tmp->nb_bytecount;
	}

	return;
}

__stdcall static void
ndis_firstbuf_safe(packet, buf, firstva, firstlen, totlen, prio)
	ndis_packet		*packet;
	ndis_buffer		**buf;
	void			**firstva;
	uint32_t		*firstlen;
	uint32_t		*totlen;
	uint32_t		prio;
{
	ndis_firstbuf(packet, buf, firstva, firstlen, totlen);
}

static int
ndis_find_sym(lf, filename, suffix, sym)
	linker_file_t		lf;
	char			*filename;
	char			*suffix;
	caddr_t			*sym;
{
	char			fullsym[MAXPATHLEN];
	int			i;

	bzero(fullsym, sizeof(fullsym));
	strcpy(fullsym, filename);
	for (i = 0; i < strlen(fullsym); i++) {
		if (fullsym[i] == '.')
			fullsym[i] = '_';
		else
			fullsym[i] = tolower(fullsym[i]);
	}
	strcat(fullsym, suffix);
	*sym = linker_file_lookup_symbol(lf, fullsym, 0);
	if (*sym == 0)
		return(ENOENT);

	return(0);
}

/* can also return NDIS_STATUS_RESOURCES/NDIS_STATUS_ERROR_READING_FILE */
__stdcall static void
ndis_open_file(status, filehandle, filelength, filename, highestaddr)
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
	char			path[MAXPATHLEN];
	linker_file_t		head, lf;
	caddr_t			kldstart, kldend;

	ndis_unicode_to_ascii(filename->nus_buf,
	    filename->nus_len, &afilename);

	fh = malloc(sizeof(ndis_fh), M_TEMP, M_NOWAIT);
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
		fh->nf_type = NDIS_FH_TYPE_MODULE;
		fh->nf_map = kldstart;
		*filelength = fh->nf_maplen = (kldend - kldstart) & 0xFFFFFFFF;
		*filehandle = fh;
		free(afilename, M_DEVBUF);
		*status = NDIS_STATUS_SUCCESS;
		return;
	}

	if (TAILQ_EMPTY(&mountlist)) {
		free(fh, M_TEMP);
		*status = NDIS_STATUS_FILE_NOT_FOUND;
		printf("NDIS: could not find file %s in linker list\n",
		    afilename);
		printf("NDIS: and no filesystems mounted yet, "
		    "aborting NdisOpenFile()\n");
		free(afilename, M_DEVBUF);
		return;
	}

	sprintf(path, "%s/%s", ndis_filepath, afilename);
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
		free(fh, M_TEMP);
		printf("NDIS: open file %s failed: %d\n", path, error);
		return;
	}

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
ndis_map_file(status, mappedbuffer, filehandle)
	ndis_status		*status;
	void			**mappedbuffer;
	ndis_handle		filehandle;
{
	ndis_fh			*fh;
	struct thread		*td = curthread;
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
		/* Already found the mapping address during the open. */
		*status = NDIS_STATUS_SUCCESS;
		*mappedbuffer = fh->nf_map;
		return;
	}

	fh->nf_map = malloc(fh->nf_maplen, M_DEVBUF, M_NOWAIT);

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
ndis_unmap_file(filehandle)
	ndis_handle		filehandle;
{
	ndis_fh			*fh;
	fh = (ndis_fh *)filehandle;

	if (fh->nf_map == NULL)
		return;

	if (fh->nf_type == NDIS_FH_TYPE_VFS)
		free(fh->nf_map, M_DEVBUF);
	fh->nf_map = NULL;

	return;
}

__stdcall static void
ndis_close_file(filehandle)
	ndis_handle		filehandle;
{
	struct thread		*td = curthread;
	ndis_fh			*fh;

	if (filehandle == NULL)
		return;

	fh = (ndis_fh *)filehandle;
	if (fh->nf_map != NULL) {
		if (fh->nf_type == NDIS_FH_TYPE_VFS)
			free(fh->nf_map, M_DEVBUF);
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
	free(fh, M_DEVBUF);

	return;
}

__stdcall static uint8_t
ndis_cpu_cnt()
{
	return(mp_ncpus);
}

typedef void (*ndis_statusdone_handler)(ndis_handle);
typedef void (*ndis_status_handler)(ndis_handle, ndis_status,
        void *, uint32_t);

__stdcall static void
ndis_ind_statusdone(adapter)
	ndis_handle		adapter;
{
	ndis_miniport_block	*block;
	__stdcall ndis_statusdone_handler	statusdonefunc;

	block = (ndis_miniport_block *)adapter;
	statusdonefunc = block->nmb_statusdone_func;

	statusdonefunc(adapter);
	return;
}

__stdcall static void
ndis_ind_status(adapter, status, sbuf, slen)
	ndis_handle		adapter;
	ndis_status		status;
	void			*sbuf;
	uint32_t		slen;
{
	ndis_miniport_block	*block;
	__stdcall ndis_status_handler	statusfunc;

	block = (ndis_miniport_block *)adapter;
	statusfunc = block->nmb_status_func;

	statusfunc(adapter, status, sbuf, slen);
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
	workfunc(work, work->nwi_ctx);
	return;
}

__stdcall static ndis_status
ndis_sched_workitem(work)
	ndis_work_item		*work;
{
	ndis_sched(ndis_workfunc, work, NDIS_TASKQUEUE);
	return(NDIS_STATUS_SUCCESS);
}

__stdcall static void
ndis_pkt_to_pkt(dpkt, doff, reqlen, spkt, soff, cpylen)
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

	sptr = MDL_VA(src);
	dptr = MDL_VA(dst);
	scnt = src->nb_bytecount;
	dcnt = dst->nb_bytecount;

	while (soff) {
		if (src->nb_bytecount > soff) {
			sptr += soff;
			scnt = src->nb_bytecount - soff;
			break;
		}
		soff -= src->nb_bytecount;
		src = src->nb_next;
		if (src == NULL)
			return;
		sptr = MDL_VA(src);
	}

	while (doff) {
		if (dst->nb_bytecount > doff) {
			dptr += doff;
			dcnt = dst->nb_bytecount - doff;
			break;
		}
		doff -= dst->nb_bytecount;
		dst = dst->nb_next;
		if (dst == NULL)
			return;
		dptr = MDL_VA(dst);
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
			dst = dst->nb_next;
			if (dst == NULL)
				break;
			dptr = MDL_VA(dst);
			dcnt = dst->nb_bytecount;
		}

		scnt -= len;
		if (scnt == 0) {
			src = src->nb_next;
			if (src == NULL)
				break;
			sptr = MDL_VA(src);
			scnt = src->nb_bytecount;
		}
	}

	*cpylen = copied;
	return;
}

__stdcall static void
ndis_pkt_to_pkt_safe(dpkt, doff, reqlen, spkt, soff, cpylen, prio)
	ndis_packet		*dpkt;
	uint32_t		doff;
	uint32_t		reqlen;
	ndis_packet		*spkt;
	uint32_t		soff;
	uint32_t		*cpylen;
	uint32_t		prio;
{
	ndis_pkt_to_pkt(dpkt, doff, reqlen, spkt, soff, cpylen);
	return;
}

__stdcall static ndis_status
ndis_register_dev(handle, devname, symname, majorfuncs, devobj, devhandle)
	ndis_handle		handle;
	ndis_unicode_string	*devname;
	ndis_unicode_string	*symname;
	driver_dispatch		*majorfuncs[];
	void			**devobj;
	ndis_handle		*devhandle;
{
	ndis_miniport_block	*block;

	block = (ndis_miniport_block *)handle;
	*devobj = &block->nmb_devobj;
	*devhandle = handle;

	return(NDIS_STATUS_SUCCESS);
}

__stdcall static ndis_status
ndis_deregister_dev(handle)
	ndis_handle		handle;
{
	return(NDIS_STATUS_SUCCESS);
}

__stdcall static ndis_status
ndis_query_name(name, handle)
	ndis_unicode_string	*name;
	ndis_handle		handle;
{
	ndis_miniport_block	*block;

	block = (ndis_miniport_block *)handle;
	ndis_ascii_to_unicode(__DECONST(char *,
	    device_get_nameunit(block->nmb_dev)), &name->nus_buf);
	name->nus_len = strlen(device_get_nameunit(block->nmb_dev)) * 2;

	return(NDIS_STATUS_SUCCESS);
}

__stdcall static void
ndis_register_unload(handle, func)
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
	{ "NdisCopyFromPacketToPacket",	(FUNC)ndis_pkt_to_pkt },
	{ "NdisCopyFromPacketToPacketSafe", (FUNC)ndis_pkt_to_pkt_safe },
	{ "NdisScheduleWorkItem",	(FUNC)ndis_sched_workitem },
	{ "NdisMIndicateStatusComplete", (FUNC)ndis_ind_statusdone },
	{ "NdisMIndicateStatus",	(FUNC)ndis_ind_status },
	{ "NdisSystemProcessorCount",	(FUNC)ndis_cpu_cnt },
	{ "NdisUnchainBufferAtBack",	(FUNC)ndis_unchain_tailbuf, },
	{ "NdisGetFirstBufferFromPacket", (FUNC)ndis_firstbuf },
	{ "NdisGetFirstBufferFromPacketSafe", (FUNC)ndis_firstbuf_safe },
	{ "NdisGetBufferPhysicalArraySize", (FUNC)ndis_buf_physpages },
	{ "NdisMGetDeviceProperty",	(FUNC)ndis_get_devprop },
	{ "NdisInitAnsiString",		(FUNC)ndis_init_ansi_string },
	{ "NdisInitUnicodeString",	(FUNC)ndis_init_unicode_string },
	{ "NdisWriteConfiguration",	(FUNC)ndis_write_cfg },
	{ "NdisAnsiStringToUnicodeString", (FUNC)ndis_ansi2unicode },
	{ "NdisTerminateWrapper",	(FUNC)ndis_termwrap },
	{ "NdisOpenConfigurationKeyByName", (FUNC)ndis_open_cfgbyname },
	{ "NdisOpenConfigurationKeyByIndex", (FUNC)ndis_open_cfgbyidx },
	{ "NdisMRemoveMiniport",	(FUNC)ndis_remove_miniport },
	{ "NdisInitializeString",	(FUNC)ndis_init_string },	
	{ "NdisFreeString",		(FUNC)ndis_free_string },	
	{ "NdisGetCurrentSystemTime",	(FUNC)ndis_time },
	{ "NdisGetSystemUpTime",	(FUNC)ndis_uptime },
	{ "NdisMSynchronizeWithInterrupt", (FUNC)ndis_sync_with_intr },
	{ "NdisMAllocateSharedMemoryAsync", (FUNC)ndis_alloc_sharedmem_async },
	{ "NdisInterlockedInsertHeadList", (FUNC)ndis_insert_head },
	{ "NdisInterlockedInsertTailList", (FUNC)ndis_insert_tail },
	{ "NdisInterlockedRemoveHeadList", (FUNC)ndis_remove_head },
	{ "NdisInitializeWrapper",	(FUNC)ndis_initwrap },
	{ "NdisMRegisterMiniport",	(FUNC)ndis_register_miniport },
	{ "NdisAllocateMemoryWithTag",	(FUNC)ndis_malloc_withtag },
	{ "NdisAllocateMemory",		(FUNC)ndis_malloc },
	{ "NdisMSetAttributesEx",	(FUNC)ndis_setattr_ex },
	{ "NdisCloseConfiguration",	(FUNC)ndis_close_cfg },
	{ "NdisReadConfiguration",	(FUNC)ndis_read_cfg },
	{ "NdisOpenConfiguration",	(FUNC)ndis_open_cfg },
	{ "NdisAcquireSpinLock",	(FUNC)ndis_lock },
	{ "NdisReleaseSpinLock",	(FUNC)ndis_unlock },
	{ "NdisDprAcquireSpinLock",	(FUNC)ndis_lock_dpr },
	{ "NdisDprReleaseSpinLock",	(FUNC)ndis_unlock_dpr },
	{ "NdisAllocateSpinLock",	(FUNC)ndis_create_lock },
	{ "NdisFreeSpinLock",		(FUNC)ndis_destroy_lock },
	{ "NdisFreeMemory",		(FUNC)ndis_free },
	{ "NdisReadPciSlotInformation",	(FUNC)ndis_read_pci },
	{ "NdisWritePciSlotInformation",(FUNC)ndis_write_pci },
	{ "NdisImmediateReadPciSlotInformation", (FUNC)ndis_read_pci },
	{ "NdisImmediateWritePciSlotInformation", (FUNC)ndis_write_pci },
	{ "NdisWriteErrorLogEntry",	(FUNC)ndis_syslog },
	{ "NdisMStartBufferPhysicalMapping", (FUNC)ndis_vtophys_load },
	{ "NdisMCompleteBufferPhysicalMapping", (FUNC)ndis_vtophys_unload },
	{ "NdisMInitializeTimer",	(FUNC)ndis_create_timer },
	{ "NdisInitializeTimer",	(FUNC)ndis_init_timer },
	{ "NdisSetTimer",		(FUNC)ndis_set_timer },
	{ "NdisMCancelTimer",		(FUNC)ndis_cancel_timer },
	{ "NdisCancelTimer",		(FUNC)ndis_cancel_timer },
	{ "NdisMSetPeriodicTimer",	(FUNC)ndis_set_periodic_timer },
	{ "NdisMQueryAdapterResources",	(FUNC)ndis_query_resources },
	{ "NdisMRegisterIoPortRange",	(FUNC)ndis_register_ioport },
	{ "NdisMDeregisterIoPortRange",	(FUNC)ndis_deregister_ioport },
	{ "NdisReadNetworkAddress",	(FUNC)ndis_read_netaddr },
	{ "NdisQueryMapRegisterCount",	(FUNC)ndis_mapreg_cnt },
	{ "NdisMAllocateMapRegisters",	(FUNC)ndis_alloc_mapreg },
	{ "NdisMFreeMapRegisters",	(FUNC)ndis_free_mapreg },
	{ "NdisMAllocateSharedMemory",	(FUNC)ndis_alloc_sharedmem },
	{ "NdisMMapIoSpace",		(FUNC)ndis_map_iospace },
	{ "NdisMUnmapIoSpace",		(FUNC)ndis_unmap_iospace },
	{ "NdisGetCacheFillSize",	(FUNC)ndis_cachefill },
	{ "NdisMGetDmaAlignment",	(FUNC)ndis_dma_align },
	{ "NdisMInitializeScatterGatherDma", (FUNC)ndis_init_sc_dma },
	{ "NdisAllocatePacketPool",	(FUNC)ndis_alloc_packetpool },
	{ "NdisAllocatePacketPoolEx",	(FUNC)ndis_ex_alloc_packetpool },
	{ "NdisAllocatePacket",		(FUNC)ndis_alloc_packet },
	{ "NdisFreePacket",		(FUNC)ndis_release_packet },
	{ "NdisFreePacketPool",		(FUNC)ndis_free_packetpool },
	{ "NdisDprAllocatePacket",	(FUNC)ndis_alloc_packet },
	{ "NdisDprFreePacket",		(FUNC)ndis_release_packet },
	{ "NdisAllocateBufferPool",	(FUNC)ndis_alloc_bufpool },
	{ "NdisAllocateBuffer",		(FUNC)ndis_alloc_buf },
	{ "NdisQueryBuffer",		(FUNC)ndis_query_buf },
	{ "NdisQueryBufferSafe",	(FUNC)ndis_query_buf_safe },
	{ "NdisBufferVirtualAddress",	(FUNC)ndis_buf_vaddr },
	{ "NdisBufferVirtualAddressSafe", (FUNC)ndis_buf_vaddr_safe },
	{ "NdisBufferLength",		(FUNC)ndis_buflen },
	{ "NdisFreeBuffer",		(FUNC)ndis_release_buf },
	{ "NdisFreeBufferPool",		(FUNC)ndis_free_bufpool },
	{ "NdisInterlockedIncrement",	(FUNC)ndis_interlock_inc },
	{ "NdisInterlockedDecrement",	(FUNC)ndis_interlock_dec },
	{ "NdisInitializeEvent",	(FUNC)ndis_init_event },
	{ "NdisSetEvent",		(FUNC)ndis_set_event },
	{ "NdisResetEvent",		(FUNC)ndis_reset_event },
	{ "NdisWaitEvent",		(FUNC)ndis_wait_event },
	{ "NdisUnicodeStringToAnsiString", (FUNC)ndis_unicode2ansi },
	{ "NdisMPciAssignResources",	(FUNC)ndis_assign_pcirsrc },
	{ "NdisMFreeSharedMemory",	(FUNC)ndis_free_sharedmem },
	{ "NdisMRegisterInterrupt",	(FUNC)ndis_register_intr },
	{ "NdisMDeregisterInterrupt",	(FUNC)ndis_deregister_intr },
	{ "NdisMRegisterAdapterShutdownHandler", (FUNC)ndis_register_shutdown },
	{ "NdisMDeregisterAdapterShutdownHandler", (FUNC)ndis_deregister_shutdown },
	{ "NDIS_BUFFER_TO_SPAN_PAGES",	(FUNC)ndis_numpages },
	{ "NdisQueryBufferOffset",	(FUNC)ndis_query_bufoffset },
	{ "NdisAdjustBufferLength",	(FUNC)ndis_adjust_buflen },
	{ "NdisPacketPoolUsage",	(FUNC)ndis_packetpool_use },
	{ "NdisMSleep",			(FUNC)ndis_sleep },
	{ "NdisUnchainBufferAtFront",	(FUNC)ndis_unchain_headbuf },
	{ "NdisReadPcmciaAttributeMemory", (FUNC)ndis_read_pccard_amem },
	{ "NdisWritePcmciaAttributeMemory", (FUNC)ndis_write_pccard_amem },
	{ "NdisOpenFile",		(FUNC)ndis_open_file },
	{ "NdisMapFile",		(FUNC)ndis_map_file },
	{ "NdisUnmapFile",		(FUNC)ndis_unmap_file },
	{ "NdisCloseFile",		(FUNC)ndis_close_file },
	{ "NdisMRegisterDevice",	(FUNC)ndis_register_dev },
	{ "NdisMDeregisterDevice",	(FUNC)ndis_deregister_dev },
	{ "NdisMQueryAdapterInstanceName", (FUNC)ndis_query_name },
	{ "NdisMRegisterUnloadHandler",	(FUNC)ndis_register_unload },

	/*
	 * This last entry is a catch-all for any function we haven't
	 * implemented yet. The PE import list patching routine will
	 * use it for any function that doesn't have an explicit match
	 * in this table.
	 */

	{ NULL, (FUNC)dummy },

	/* End of list. */

	{ NULL, NULL },
};
