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

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <machine/bus_memio.h>
#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <machine/resource.h>

#include <sys/bus.h>
#include <sys/rman.h>

#include <machine/stdarg.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <compat/ndis/pe_var.h>
#include <compat/ndis/resource_var.h>
#include <compat/ndis/ndis_var.h>
#include <compat/ndis/cfg_var.h>
#include <dev/if_ndis/if_ndisvar.h>

#define __stdcall __attribute__((__stdcall__))
#define FUNC void(*)(void)

static struct mtx ndis_interlock;
static int ndis_inits = 0;

__stdcall static void ndis_initwrap(ndis_handle,
	ndis_driver_object *, void *, void *);
__stdcall static ndis_status ndis_register_miniport(ndis_handle,
	ndis_miniport_characteristics *, int);
__stdcall static ndis_status ndis_malloc_withtag(void **, uint32_t, uint32_t);
__stdcall static ndis_status ndis_malloc(void **,
	uint32_t, uint32_t, ndis_physaddr);
__stdcall static void ndis_free(void *, uint32_t, uint32_t);
__stdcall static ndis_status ndis_setattr_ex(ndis_handle, ndis_handle,
	uint32_t, uint32_t, ndis_interface_type);
__stdcall static void ndis_open_cfg(ndis_status *, ndis_handle *, ndis_handle);
static ndis_status ndis_encode_parm(ndis_miniport_block *,
	struct sysctl_oid *, ndis_parm_type, ndis_config_parm **);
__stdcall static void ndis_read_cfg(ndis_status *, ndis_config_parm **,
	ndis_handle, ndis_unicode_string *, ndis_parm_type);
__stdcall static void ndis_close_cfg(ndis_handle);
__stdcall static void ndis_create_lock(ndis_spin_lock *);
__stdcall static void ndis_destroy_lock(ndis_spin_lock *);
__stdcall static void ndis_lock(ndis_spin_lock *);
__stdcall static void ndis_unlock(ndis_spin_lock *);
__stdcall static uint32_t ndis_read_pci(ndis_handle, uint32_t,
	uint32_t, void *, uint32_t);
__stdcall static uint32_t ndis_write_pci(ndis_handle, uint32_t,
	uint32_t, void *, uint32_t);
static void ndis_syslog(ndis_handle, ndis_error_code, uint32_t, ...);
static void ndis_map_cb(void *, bus_dma_segment_t *, int, int);
__stdcall static void ndis_vtophys_load(ndis_handle, ndis_buffer *,
	uint32_t, uint8_t, ndis_paddr_unit *, uint32_t *);
__stdcall static void ndis_vtophys_unload(ndis_handle, ndis_buffer *, uint32_t);
__stdcall static void ndis_create_timer(ndis_miniport_timer *, ndis_handle *,
	ndis_timer_function, void *);
static void ndis_timercall(void *);
__stdcall static void ndis_set_timer(ndis_miniport_timer *, uint32_t);
static void ndis_tick(void *);
__stdcall static void ndis_set_periodic_timer(ndis_miniport_timer *, uint32_t);
__stdcall static void ndis_cancel_timer(ndis_miniport_timer *, uint8_t *);
__stdcall static void ndis_query_resources(ndis_status *, ndis_handle,
	ndis_resource_list *, uint32_t *);
__stdcall static ndis_status ndis_register_ioport(void **,
	ndis_handle, uint32_t, uint32_t);
__stdcall static void ndis_deregister_ioport(ndis_handle,
	uint32_t, uint32_t, void *);
__stdcall static void ndis_read_netaddr(ndis_status *, void **,
	uint32_t *, ndis_handle);
__stdcall static ndis_status ndis_alloc_mapreg(ndis_handle,
	uint32_t, uint8_t, uint32_t, uint32_t);
__stdcall static void ndis_free_mapreg(ndis_handle);
static void ndis_mapshared_cb(void *, bus_dma_segment_t *, int, int);
__stdcall static void ndis_alloc_sharedmem(ndis_handle, uint32_t,
	uint8_t, void **, ndis_physaddr *);
__stdcall static void ndis_alloc_sharedmem_async(ndis_handle,
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
__stdcall static void ndis_alloc_bufpool(ndis_status *,
	ndis_handle *, uint32_t);
__stdcall static void ndis_free_bufpool(ndis_handle);
__stdcall static void ndis_alloc_buf(ndis_status *, ndis_buffer **,
	ndis_handle, void *, uint32_t);
__stdcall static void ndis_release_buf(ndis_buffer *);
__stdcall static void ndis_query_buf(ndis_buffer *, void **, uint32_t *);
__stdcall static void ndis_query_buf_safe(ndis_buffer *, void **,
	uint32_t *, uint32_t);
__stdcall static void ndis_adjust_buflen(ndis_buffer *, int);
__stdcall static uint32_t ndis_interlock_inc(uint32_t *);
__stdcall static uint32_t ndis_interlock_dec(uint32_t *);
__stdcall static void ndis_init_event(ndis_event *);
__stdcall static void ndis_set_event(ndis_event *);
__stdcall static void ndis_reset_event(ndis_event *);
__stdcall static uint8_t ndis_wait_event(ndis_event *, uint32_t);
__stdcall static ndis_status ndis_unicode2ansi(ndis_ansi_string *,
	ndis_unicode_string *);
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
__stdcall static void ndis_query_bufoffset(ndis_buffer *,
	uint32_t *, uint32_t *);
__stdcall static void ndis_sleep(uint32_t);
__stdcall static uint32_t ndis_read_pccard_amem(ndis_handle,
	uint32_t, void *, uint32_t);
__stdcall static uint32_t ndis_write_pccard_amem(ndis_handle,
	uint32_t, void *, uint32_t);
__stdcall static ndis_list_entry *ndis_insert_head(ndis_list_entry *,
	ndis_list_entry *, ndis_spin_lock *);
__stdcall static ndis_list_entry *ndis_remove_head(ndis_list_entry *,
	ndis_spin_lock *);
__stdcall static ndis_list_entry *ndis_insert_tail(ndis_list_entry *,
	ndis_list_entry *, ndis_spin_lock *);
__stdcall static uint8_t ndis_sync_with_intr(ndis_miniport_interrupt *,
	void *, void *);
__stdcall static void dummy(void);


int
ndis_libinit()
{
	if (ndis_inits) {
		ndis_inits++;
		return(0);
	}

	mtx_init(&ndis_interlock, "ndislock", MTX_NETWORK_LOCK,
	    MTX_DEF | MTX_RECURSE | MTX_DUPOK);

	ndis_inits++;
	return(0);
}

int
ndis_libfini()
{
	if (ndis_inits != 1) {
		ndis_inits--;
		return(0);
	}

	mtx_destroy(&ndis_interlock);
	ndis_inits--;

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
		*ascii = malloc(ulen, M_DEVBUF, M_WAITOK);

	if (*ascii == NULL)
		return(ENOMEM);
	astr = *ascii;
	for (i = 0; i < ulen; i++) {
		*astr = (uint8_t)unicode[i];
		astr++;
	}

	return(0);
}

__stdcall static void
ndis_initwrap(wrapper, drv_obj, path, unused)
	ndis_handle		wrapper;
	ndis_driver_object	*drv_obj;
	void			*path;
	void			*unused;
{
	ndis_driver_object	**drv;

	drv = wrapper;
	*drv = drv_obj;

	return;
}

__stdcall static ndis_status
ndis_register_miniport(handle, characteristics, len)
	ndis_handle		handle;
	ndis_miniport_characteristics *characteristics;
	int			len;
{
	ndis_driver_object	*drv;

	drv = handle;
	bcopy((char *)characteristics, (char *)&drv->ndo_chars,
	    sizeof(ndis_miniport_characteristics));
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

static ndis_status
ndis_encode_parm(block, oid, type, parm)
	ndis_miniport_block	*block;
        struct sysctl_oid	*oid;
	ndis_parm_type		type;
	ndis_config_parm	**parm;
{
	uint16_t		*unicode;
	ndis_unicode_string	*ustr;

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
		(*parm)->ncp_type = ndis_parm_int;
		(*parm)->ncp_parmdata.ncp_intdata =
		    strtol((char *)oid->oid_arg1, NULL, 10);
		break;
	case ndis_parm_hexint:
		(*parm)->ncp_type = ndis_parm_hexint;
		(*parm)->ncp_parmdata.ncp_intdata =
		    strtoul((char *)oid->oid_arg1, NULL, 16);
		break;
	default:
		return(NDIS_STATUS_FAILURE);
		break;
	}

	return(NDIS_STATUS_SUCCESS);
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

	ndis_unicode_to_ascii(key->nus_buf, key->nus_len, &keystr);

	*parm = &block->nmb_replyparm;
	bzero((char *)&block->nmb_replyparm, sizeof(ndis_config_parm));
	unicode = (uint16_t *)&block->nmb_dummybuf;

	/*
	 * See if registry key is already in a list of known keys
	 * included with the driver.
	 */
	TAILQ_FOREACH(e, &sc->ndis_ctx, link) {
		oidp = e->entry;
		if (strcmp(oidp->oid_name, keystr) == 0) {
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

__stdcall static void
ndis_close_cfg(cfg)
	ndis_handle		cfg;
{
	return;
}

__stdcall static void
ndis_create_lock(lock)
	ndis_spin_lock		*lock;
{
	struct mtx		*mtx;

	mtx = malloc(sizeof(struct mtx), M_DEVBUF, M_NOWAIT|M_ZERO);
	if (mtx == NULL)
		return;
	mtx_init(mtx, "ndislock", MTX_NETWORK_LOCK,
	    MTX_DEF | MTX_RECURSE | MTX_DUPOK);
	lock->nsl_spinlock = (ndis_kspin_lock)mtx;

	return;
}

__stdcall static void
ndis_destroy_lock(lock)
	ndis_spin_lock		*lock;
{
	struct mtx		*ndis_mtx;

	ndis_mtx = (struct mtx *)lock->nsl_spinlock;
	mtx_destroy(ndis_mtx);
	free(ndis_mtx, M_DEVBUF);

	return;
}

__stdcall static void
ndis_lock(lock)
	ndis_spin_lock		*lock;
{
	if (lock == NULL)
		return;
	mtx_lock((struct mtx *)lock->nsl_spinlock);

	return;
}

__stdcall static void
ndis_unlock(lock)
	ndis_spin_lock		*lock;
{
	if (lock == NULL)
		return;
	mtx_unlock((struct mtx *)lock->nsl_spinlock);

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
static void
ndis_syslog(ndis_handle adapter, ndis_error_code code,
	uint32_t numerrors, ...)
{
	ndis_miniport_block	*block;
	va_list			ap;
	int			i;

	block = (ndis_miniport_block *)adapter;

	printf ("NDIS ERROR: %x\n", code);
	printf ("NDIS NUMERRORS: %x\n", numerrors);

	va_start(ap, numerrors);
	for (i = 0; i < numerrors; i++)
		printf ("argptr: %p\n", va_arg(ap, void *));
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
	    buf->nb_mappedsystemva, buf->nb_bytecount, ndis_map_cb,
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

__stdcall static void
ndis_create_timer(timer, handle, func, ctx)
	ndis_miniport_timer	*timer;
	ndis_handle		*handle;
	ndis_timer_function	func;
	void			*ctx;
{
	struct callout_handle	*ch;

	ch = (struct callout_handle *)&timer->nmt_dpc;
	callout_handle_init(ch);
	timer->nmt_timerfunc = func;
	timer->nmt_timerctx = ctx;

	return;
}

/*
 * The driver's timer callout is __stdcall function, so we need this
 * intermediate step.
 */

static void
ndis_timercall(arg)
	void		*arg;
{
	ndis_miniport_timer	*timer;
	__stdcall ndis_timer_function	timerfunc;

	timer = arg;

	timerfunc = timer->nmt_timerfunc;
	timerfunc(NULL, timer->nmt_timerctx, NULL, NULL);

	return;
}

/*
 * Windows specifies timeouts in milliseconds. We specify timeouts
 * in hz. Trying to compute a tenth of a second based on hz is tricky.
 * so we approximate. Note that we abuse the dpc portion of the
 * miniport timer structure to hold the UNIX callout handle.
 */
__stdcall static void
ndis_set_timer(timer, msecs)
	ndis_miniport_timer	*timer;
	uint32_t		msecs;
{
	struct callout_handle	*ch;
	struct timeval		tv;

	tv.tv_sec = 0;
	tv.tv_usec = msecs * 1000;

	ch = (struct callout_handle *)&timer->nmt_dpc;
	timer->nmt_dpc.nk_sysarg2 = ndis_timercall;
	*ch = timeout((timeout_t *)timer->nmt_dpc.nk_sysarg2, (void *)timer,
	    tvtohz(&tv));

	return;
}

static void
ndis_tick(arg)
	void			*arg;
{
	ndis_miniport_timer	*timer;
	struct callout_handle	*ch;
	__stdcall ndis_timer_function	timerfunc;
	struct timeval		tv;

	timer = arg;

	timerfunc = timer->nmt_timerfunc;
	timerfunc(NULL, timer->nmt_timerctx, NULL, NULL);

	/* Automatically reload timer. */

	tv.tv_sec = 0;
	tv.tv_usec = timer->nmt_ktimer.nk_period * 1000;
	ch = (struct callout_handle *)&timer->nmt_dpc;
	timer->nmt_dpc.nk_sysarg2 = ndis_tick;
	*ch = timeout((timeout_t *)timer->nmt_dpc.nk_sysarg2, timer,
	    tvtohz(&tv));

	return;
}

__stdcall static void
ndis_set_periodic_timer(timer, msecs)
	ndis_miniport_timer	*timer;
	uint32_t		msecs;
{
	struct callout_handle	*ch;
	struct timeval		tv;

	tv.tv_sec = 0;
	tv.tv_usec = msecs * 1000;

	timer->nmt_ktimer.nk_period = msecs;
	ch = (struct callout_handle *)&timer->nmt_dpc;
	timer->nmt_dpc.nk_sysarg2 = ndis_tick;
	*ch = timeout((timeout_t *)timer->nmt_dpc.nk_sysarg2, timer,
	    tvtohz(&tv));

	return;
}

__stdcall static void
ndis_cancel_timer(timer, cancelled)
	ndis_miniport_timer	*timer;
	uint8_t			*cancelled;
{
	struct callout_handle	*ch;

	ch = (struct callout_handle *)&timer->nmt_dpc;
	untimeout((timeout_t *)timer->nmt_dpc.nk_sysarg2, timer, *ch);

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

	block = (ndis_miniport_block *)adapter;
	sc = (struct ndis_softc *)block->nmb_ifp;
 
	*buflen = sizeof(ndis_resource_list) +
	    (sizeof(cm_partial_resource_desc) * (sc->ndis_rescnt - 1));

	bcopy((char *)block->nmb_rlist, (char *)list, *buflen);
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

	if (rman_get_size(sc->ndis_res_io) != numports)
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

	error = bus_dma_tag_create(sc->ndis_parent_tag, 64,
	    0, BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL,
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

__stdcall static void
ndis_alloc_sharedmem_async(adapter, len, cached, ctx)
	ndis_handle		adapter;
	uint32_t		len;
	uint8_t			cached;
	void			*ctx;
{
	ndis_miniport_block	*block;
	struct ndis_softc	*sc;
	void			*vaddr;
	ndis_physaddr		paddr;
	__stdcall ndis_allocdone_handler	donefunc;

	if (adapter == NULL)
		return;

	block = (ndis_miniport_block *)adapter;
	sc = (struct ndis_softc *)(block->nmb_ifp);
	donefunc = sc->ndis_chars.nmc_allocate_complete_func;

	ndis_alloc_sharedmem(adapter, len, cached, &vaddr, &paddr);
	donefunc(adapter, vaddr, &paddr, len, ctx);

	return;
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

	if (sc->ndis_res_mem == NULL)
		return(NDIS_STATUS_FAILURE);

	*vaddr = (void *)rman_get_virtual(sc->ndis_res_mem);

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

	*pool = malloc(sizeof(ndis_packet) * (descnum + 1),
	    M_DEVBUF, M_NOWAIT|M_ZERO);

	if (pool == NULL) {
		*status = NDIS_STATUS_RESOURCES;
		return;
	}

	cur = (ndis_packet *)*pool;
	cur->np_private.npp_flags = 0x1; /* mark the head of the list */
	for (i = 0; i < descnum; i++) {
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
	free(pool, M_DEVBUF);
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

	*pool = malloc(sizeof(ndis_buffer) * (descnum + 1),
	    M_DEVBUF, M_NOWAIT|M_ZERO);

	if (pool == NULL) {
		*status = NDIS_STATUS_RESOURCES;
		return;
	}

	cur = (ndis_buffer *)*pool;
	cur->nb_flags = 0x1; /* mark the head of the list */
	for (i = 0; i < descnum; i++) {
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
	free(pool, M_DEVBUF);
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

	buf = head->nb_next;

	if (buf == NULL) {
		*status = NDIS_STATUS_RESOURCES;
		return;
	}

	head->nb_next = buf->nb_next;

	/* Save pointer to the pool. */
	buf->nb_process = head;

	buf->nb_mappedsystemva = vaddr;
	buf->nb_size = len;
	buf->nb_next = NULL;

	*buffer = buf;

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

	return;
}

/* Get the virtual address and length of a buffer */

__stdcall static void
ndis_query_buf(buf, vaddr, len)
	ndis_buffer		*buf;
	void			**vaddr;
	uint32_t		*len;
{
	*vaddr = buf->nb_mappedsystemva;
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
	*vaddr = buf->nb_mappedsystemva;
	*len = buf->nb_bytecount;

	return;
}

__stdcall static void
ndis_adjust_buflen(buf, len)
	ndis_buffer		*buf;
	int			len;
{
	if (len > buf->nb_size)
		return;
	buf->nb_bytecount = len;

	return;
}

__stdcall static uint32_t
ndis_interlock_inc(addend)
	uint32_t		*addend;
{
	mtx_lock(&ndis_interlock);
	*addend++;
	mtx_unlock(&ndis_interlock);
	return(*addend);
}

__stdcall static uint32_t
ndis_interlock_dec(addend)
	uint32_t		*addend;
{
	mtx_lock(&ndis_interlock);
	*addend--;
	mtx_unlock(&ndis_interlock);
	return(*addend);
}

__stdcall static void
ndis_init_event(event)
	ndis_event		*event;
{
	event->ne_event.nk_header.dh_sigstate = FALSE;
	return;
}

__stdcall static void
ndis_set_event(event)
	ndis_event		*event;
{
	event->ne_event.nk_header.dh_sigstate = TRUE;
	wakeup(event);
	return;
}

__stdcall static void
ndis_reset_event(event)
	ndis_event		*event;
{
	event->ne_event.nk_header.dh_sigstate = FALSE;
	wakeup(event);
	return;
}

__stdcall static uint8_t
ndis_wait_event(event, msecs)
	ndis_event		*event;
	uint32_t		msecs;
{
	int			error;
	struct timeval		tv;

	if (event->ne_event.nk_header.dh_sigstate == TRUE)
		return(TRUE);

	tv.tv_sec = 0;
	tv.tv_usec = msecs * 1000;

	error = tsleep(event, PPAUSE|PCATCH, "ndis", tvtohz(&tv));

	return(event->ne_event.nk_header.dh_sigstate);
}

__stdcall static ndis_status
ndis_unicode2ansi(dstr, sstr)
	ndis_ansi_string		*dstr;
	ndis_unicode_string		*sstr;
{
	ndis_unicode_to_ascii(sstr->nus_buf, sstr->nus_len, &dstr->nas_buf);
	dstr->nas_len = strlen(dstr->nas_buf);
	printf ("unicode 2 ansi...\n");
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

	printf ("assign PCI resources...\n");
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
	return(howmany(buf->nb_bytecount, PAGE_SIZE));
}

__stdcall static void
ndis_query_bufoffset(buf, off, len)
	ndis_buffer		*buf;
	uint32_t		*off;
	uint32_t		*len;
{
	*off = (uint32_t)buf->nb_mappedsystemva & (PAGE_SIZE - 1);
	*len = buf->nb_bytecount;

	return;
}

__stdcall static void
ndis_sleep(usecs)
	uint32_t		usecs;
{
	struct timeval		tv;
	uint32_t		dummy;

	tv.tv_sec = 0;
	tv.tv_usec = usecs;

	tsleep(&dummy, PPAUSE|PCATCH, "ndis", tvtohz(&tv));
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
		dest[i] = bus_space_read_1(bt, bh, (offset * 2) + (i * 2));

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
		bus_space_write_1(bt, bh, (offset * 2) + (i * 2), src[i]);

	return(i);
}

__stdcall static ndis_list_entry *
ndis_insert_head(head, entry, lock)
	ndis_list_entry		*head;
	ndis_list_entry		*entry;
	ndis_spin_lock		*lock;
{
	ndis_list_entry		*flink;

	mtx_lock_spin((struct mtx *)lock->nsl_spinlock);
	flink = head->nle_flink;
	entry->nle_flink = flink;
	entry->nle_blink = head;
	flink->nle_blink = entry;
	head->nle_flink = entry;
	mtx_unlock_spin((struct mtx *)lock->nsl_spinlock);

	return(flink);
}

__stdcall static ndis_list_entry *
ndis_remove_head(head, lock)
	ndis_list_entry		*head;
	ndis_spin_lock		*lock;
{
	ndis_list_entry		*flink;
	ndis_list_entry		*entry;

	mtx_lock_spin((struct mtx *)lock->nsl_spinlock);
	entry = head->nle_flink;
	flink = entry->nle_flink;
	head->nle_flink = flink;
	flink->nle_blink = head;
	mtx_unlock_spin((struct mtx *)lock->nsl_spinlock);

	return(entry);
}

__stdcall static ndis_list_entry *
ndis_insert_tail(head, entry, lock)
	ndis_list_entry		*head;
	ndis_list_entry		*entry;
	ndis_spin_lock		*lock;
{
	ndis_list_entry		*blink;

	mtx_lock_spin((struct mtx *)lock->nsl_spinlock);
	blink = head->nle_blink;
	entry->nle_flink = head;
	entry->nle_blink = blink;
	blink->nle_flink = entry;
	head->nle_blink = entry;
	mtx_unlock_spin((struct mtx *)lock->nsl_spinlock);

	return(blink);
}

__stdcall static uint8_t
ndis_sync_with_intr(intr, syncfunc, syncctx)
	ndis_miniport_interrupt	*intr;
	void			*syncfunc;
	void			*syncctx;
{
	__stdcall uint8_t (*sync)(void *);

	if (syncfunc == NULL || syncctx == NULL)
		return(0);

	sync = syncfunc;
	return(sync(syncctx));
}

__stdcall static void
dummy()
{
	printf ("NDIS dummy called...\n");
	return;
}

image_patch_table ndis_functbl[] = {
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
	{ "NdisReleaseSpinLock",	(FUNC)ndis_unlock },
	{ "NdisDprAcquireSpinLock",	(FUNC)ndis_lock },
	{ "NdisDprReleaseSpinLock",	(FUNC)ndis_unlock },
	{ "NdisAcquireSpinLock",	(FUNC)ndis_lock },
	{ "NdisAllocateSpinLock",	(FUNC)ndis_create_lock },
	{ "NdisFreeSpinLock",		(FUNC)ndis_destroy_lock },
	{ "NdisFreeMemory",		(FUNC)ndis_free },
	{ "NdisReadPciSlotInformation",	(FUNC)ndis_read_pci },
	{ "NdisWritePciSlotInformation",(FUNC)ndis_write_pci },
	{ "NdisWriteErrorLogEntry",	(FUNC)ndis_syslog },
	{ "NdisMStartBufferPhysicalMapping", (FUNC)ndis_vtophys_load },
	{ "NdisMCompleteBufferPhysicalMapping", (FUNC)ndis_vtophys_unload },
	{ "NdisMInitializeTimer",	(FUNC)ndis_create_timer },
	{ "NdisSetTimer",		(FUNC)ndis_set_timer },
	{ "NdisMCancelTimer",		(FUNC)ndis_cancel_timer },
	{ "NdisMSetPeriodicTimer",	(FUNC)ndis_set_periodic_timer },
	{ "NdisMQueryAdapterResources",	(FUNC)ndis_query_resources },
	{ "NdisMRegisterIoPortRange",	(FUNC)ndis_register_ioport },
	{ "NdisMDeregisterIoPortRange",	(FUNC)ndis_deregister_ioport },
	{ "NdisReadNetworkAddress",	(FUNC)ndis_read_netaddr },
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
	{ "NdisAllocateBufferPool",	(FUNC)ndis_alloc_bufpool },
	{ "NdisAllocateBuffer",		(FUNC)ndis_alloc_buf },
	{ "NdisQueryBuffer",		(FUNC)ndis_query_buf },
	{ "NdisQueryBufferSafe",	(FUNC)ndis_query_buf_safe },
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

