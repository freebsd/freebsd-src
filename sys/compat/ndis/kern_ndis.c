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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/callout.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/conf.h>

#include <sys/kernel.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_ioctl.h>

#include <dev/pccard/pccardvar.h>
#include "card_if.h"

#include <compat/ndis/pe_var.h>
#include <compat/ndis/resource_var.h>
#include <compat/ndis/ndis_var.h>
#include <compat/ndis/hal_var.h>
#include <compat/ndis/ntoskrnl_var.h>
#include <compat/ndis/cfg_var.h>
#include <dev/if_ndis/if_ndisvar.h>

#define __stdcall __attribute__((__stdcall__))
#define NDIS_DUMMY_PATH "\\\\some\\bogus\\path"

__stdcall static void ndis_status_func(ndis_handle, ndis_status,
	void *, uint32_t);
__stdcall static void ndis_statusdone_func(ndis_handle);
__stdcall static void ndis_setdone_func(ndis_handle, ndis_status);
__stdcall static void ndis_getdone_func(ndis_handle, ndis_status);
__stdcall static void ndis_resetdone_func(ndis_handle, ndis_status, uint8_t);

/*
 * This allows us to export our symbols to other modules.
 * Note that we call ourselves 'ndisapi' to avoid a namespace
 * collision with if_ndis.ko, which internally calls itself
 * 'ndis.'
 */
static int
ndis_modevent(module_t mod, int cmd, void *arg)
{
	return(0);
}
DEV_MODULE(ndisapi, ndis_modevent, NULL);
MODULE_VERSION(ndisapi, 1);


__stdcall static void
ndis_status_func(adapter, status, sbuf, slen)
	ndis_handle		adapter;
	ndis_status		status;
	void			*sbuf;
	uint32_t		slen;
{
	printf ("status: %x\n", status);
	return;
}

__stdcall static void
ndis_statusdone_func(adapter)
	ndis_handle		adapter;
{
	printf ("status complete\n");
	return;
}

__stdcall static void
ndis_setdone_func(adapter, status)
	ndis_handle		adapter;
	ndis_status		status;
{
	ndis_miniport_block	*block;
	block = adapter;

	block->nmb_setstat = status;
	wakeup(&block->nmb_wkupdpctimer);
	return;
}

__stdcall static void
ndis_getdone_func(adapter, status)
	ndis_handle		adapter;
	ndis_status		status;
{
	ndis_miniport_block	*block;
	block = adapter;

	block->nmb_getstat = status;
	wakeup(&block->nmb_wkupdpctimer);
	return;
}

__stdcall static void
ndis_resetdone_func(adapter, status, addressingreset)
	ndis_handle		adapter;
	ndis_status		status;
	uint8_t			addressingreset;
{
	printf ("reset done...\n");
	return;
}

#define NDIS_AM_RID	3

int
ndis_alloc_amem(arg)
	void			*arg;
{
	struct ndis_softc	*sc;
	int			error, rid;

	if (arg == NULL)
		return(EINVAL);

	sc = arg;
	rid = NDIS_AM_RID;
	sc->ndis_res_am = bus_alloc_resource(sc->ndis_dev, SYS_RES_MEMORY,
	    &rid, 0UL, ~0UL, 0x1000, RF_ACTIVE);

	if (sc->ndis_res_am == NULL) {
		printf("ndis%d: failed to allocate attribute memory\n",
		    sc->ndis_unit);
		return(ENXIO);
	}

	error = CARD_SET_MEMORY_OFFSET(device_get_parent(sc->ndis_dev),
	    sc->ndis_dev, rid, 0, NULL);

	if (error) {
		printf("ndis%d: CARD_SET_MEMORY_OFFSET() returned 0x%x\n",
		    sc->ndis_unit, error);
		return(error);
	}

	error = CARD_SET_RES_FLAGS(device_get_parent(sc->ndis_dev),
	    sc->ndis_dev, SYS_RES_MEMORY, rid, PCCARD_A_MEM_ATTR);

	if (error) {
		printf("ndis%d: CARD_SET_RES_FLAGS() returned 0x%x\n",
		    sc->ndis_unit, error);
		return(error);
	}

	return(0);
}

int
ndis_create_sysctls(arg)
	void			*arg;
{
	struct ndis_softc	*sc;
	ndis_cfg		*vals;
	char			buf[256];

	if (arg == NULL)
		return(EINVAL);

	sc = arg;
	vals = sc->ndis_regvals;

	TAILQ_INIT(&sc->ndis_cfglist_head);

	/* Create the sysctl tree. */

	sc->ndis_tree = SYSCTL_ADD_NODE(&sc->ndis_ctx,
	    SYSCTL_STATIC_CHILDREN(_hw), OID_AUTO,
	    device_get_nameunit(sc->ndis_dev), CTLFLAG_RD, 0,
	    device_get_desc(sc->ndis_dev));

	/* Add the driver-specific registry keys. */

	vals = sc->ndis_regvals;
	while(1) {
		if (vals->nc_cfgkey == NULL)
			break;
		if (vals->nc_idx != sc->ndis_devidx) {
			vals++;
			continue;
		}
		SYSCTL_ADD_STRING(&sc->ndis_ctx,
		    SYSCTL_CHILDREN(sc->ndis_tree),
		    OID_AUTO, vals->nc_cfgkey,
		    CTLFLAG_RW, vals->nc_val,
		    sizeof(vals->nc_val),
		    vals->nc_cfgdesc);
		vals++;
	}

	/* Now add a couple of builtin keys. */

	/*
	 * Environment can be either Windows (0) or WindowsNT (1).
	 * We qualify as the latter.
	 */
	ndis_add_sysctl(sc, "Environment",
	    "Windows environment", "1", CTLFLAG_RD);

	/* NDIS version should be 5.1. */
	ndis_add_sysctl(sc, "NdisVersion",
	    "NDIS API Version", "0x00050001", CTLFLAG_RD);

	/* Bus type (PCI, PCMCIA, etc...) */
	sprintf(buf, "%d\n", (int)sc->ndis_iftype);
	ndis_add_sysctl(sc, "BusType", "Bus Type", buf, CTLFLAG_RD);

	if (sc->ndis_res_io != NULL) {
		sprintf(buf, "0x%lx\n", rman_get_start(sc->ndis_res_io));
		ndis_add_sysctl(sc, "IOBaseAddress",
		    "Base I/O Address", buf, CTLFLAG_RD);
	}

	if (sc->ndis_irq != NULL) {
		sprintf(buf, "%lu\n", rman_get_start(sc->ndis_irq));
		ndis_add_sysctl(sc, "InterruptNumber",
		    "Interrupt Number", buf, CTLFLAG_RD);
	}

	return(0);
}

int
ndis_add_sysctl(arg, key, desc, val, flag)
	void			*arg;
	char			*key;
	char			*desc;
	char			*val;
	int			flag;
{
	struct ndis_softc	*sc;
	struct ndis_cfglist	*cfg;
	char			descstr[256];

	sc = arg;

	cfg = malloc(sizeof(struct ndis_cfglist), M_DEVBUF, M_NOWAIT|M_ZERO);

	if (cfg == NULL)
		return(ENOMEM);

	cfg->ndis_cfg.nc_cfgkey = strdup(key, M_DEVBUF);
	if (desc == NULL) {
		snprintf(descstr, sizeof(descstr), "%s (dynamic)", key);
		cfg->ndis_cfg.nc_cfgdesc = strdup(descstr, M_DEVBUF);
	} else
		cfg->ndis_cfg.nc_cfgdesc = strdup(desc, M_DEVBUF);
	strcpy(cfg->ndis_cfg.nc_val, val);

	TAILQ_INSERT_TAIL(&sc->ndis_cfglist_head, cfg, link);

	SYSCTL_ADD_STRING(&sc->ndis_ctx, SYSCTL_CHILDREN(sc->ndis_tree),
	    OID_AUTO, cfg->ndis_cfg.nc_cfgkey, flag,
	    cfg->ndis_cfg.nc_val, sizeof(cfg->ndis_cfg.nc_val),
	    cfg->ndis_cfg.nc_cfgdesc);

	return(0);
}

int
ndis_flush_sysctls(arg)
	void			*arg;
{
	struct ndis_softc	*sc;
	struct ndis_cfglist	*cfg;

	sc = arg;

	while (!TAILQ_EMPTY(&sc->ndis_cfglist_head)) {
		cfg = TAILQ_FIRST(&sc->ndis_cfglist_head);
		TAILQ_REMOVE(&sc->ndis_cfglist_head, cfg, link);
		free(cfg->ndis_cfg.nc_cfgkey, M_DEVBUF);
		free(cfg->ndis_cfg.nc_cfgdesc, M_DEVBUF);
		free(cfg, M_DEVBUF);
	}

	return(0);
}

void
ndis_return_packet(packet, arg)
	void			*packet;
	void			*arg;
{
	struct ndis_softc	*sc;
	ndis_handle		adapter;
	ndis_packet		*p;
	__stdcall ndis_return_handler	returnfunc;

	if (arg == NULL || packet == NULL)
		return;

	p = packet;

	/* Decrement refcount. */
	p->np_private.npp_count--;

	/* Release packet when refcount hits zero, otherwise return. */
	if (p->np_private.npp_count)
		return;

	sc = arg;
	returnfunc = sc->ndis_chars.nmc_return_packet_func;
	adapter = sc->ndis_block.nmb_miniportadapterctx;
	if (returnfunc == NULL)
		ndis_free_packet((ndis_packet *)packet);
	else
		returnfunc(adapter, (ndis_packet *)packet);
	return;
}

void
ndis_free_bufs(b0)
	ndis_buffer		*b0;
{
	ndis_buffer		*next;

	if (b0 == NULL)
		return;

	while(b0 != NULL) {
		next = b0->nb_next;
		free (b0, M_DEVBUF);
		b0 = next;
	}

	return;
}

void
ndis_free_packet(p)
	ndis_packet		*p;
{
	if (p == NULL)
		return;

	ndis_free_bufs(p->np_private.npp_head);
	free(p, M_DEVBUF);

	return;
}

int
ndis_convert_res(arg)
	void			*arg;
{
	struct ndis_softc	*sc;
	ndis_resource_list	*rl = NULL;
	cm_partial_resource_desc	*prd = NULL;
	ndis_miniport_block	*block;

	sc = arg;
	block = &sc->ndis_block;

	rl = malloc(sizeof(ndis_resource_list) +
	    (sizeof(cm_partial_resource_desc) * (sc->ndis_rescnt - 1)),
	    M_DEVBUF, M_NOWAIT|M_ZERO);

	if (rl == NULL)
		return(ENOMEM);

	rl->cprl_version = 5;
	rl->cprl_version = 1;
	rl->cprl_count = sc->ndis_rescnt;

	prd = rl->cprl_partial_descs;
	if (sc->ndis_res_io) {
		prd->cprd_type = CmResourceTypePort;
		prd->u.cprd_port.cprd_start.np_quad =
		    rman_get_start(sc->ndis_res_io);
		prd->u.cprd_port.cprd_len =
		    rman_get_size(sc->ndis_res_io);
		prd++;
	}

	if (sc->ndis_res_mem) {
		prd->cprd_type = CmResourceTypeMemory;
		prd->u.cprd_mem.cprd_start.np_quad =
		    rman_get_start(sc->ndis_res_mem);
		prd->u.cprd_mem.cprd_len =
		    rman_get_size(sc->ndis_res_mem);
		prd++;
	}

	if (sc->ndis_irq) {
		prd->cprd_type = CmResourceTypeInterrupt;
		prd->u.cprd_intr.cprd_level =
		    rman_get_start(sc->ndis_irq);
		prd->u.cprd_intr.cprd_vector =
		    rman_get_start(sc->ndis_irq);
		prd->u.cprd_intr.cprd_affinity = 0;
	}

	block->nmb_rlist = rl;

	return(0);
}

/*
 * Map an NDIS packet to an mbuf list. When an NDIS driver receives a
 * packet, it will hand it to us in the form of an ndis_packet,
 * which we need to convert to an mbuf that is then handed off
 * to the stack. Note: we configure the mbuf list so that it uses
 * the memory regions specified by the ndis_buffer structures in
 * the ndis_packet as external storage. In most cases, this will
 * point to a memory region allocated by the driver (either by
 * ndis_malloc_withtag() or ndis_alloc_sharedmem()). We expect
 * the driver to handle free()ing this region for is, so we set up
 * a dummy no-op free handler for it.
 */ 

int
ndis_ptom(m0, p)
	struct mbuf		**m0;
	ndis_packet		*p;
{
	struct mbuf		*m, *prev = NULL;
	ndis_buffer		*buf;
	ndis_packet_private	*priv;
	uint32_t		totlen = 0;

	if (p == NULL || m0 == NULL)
		return(EINVAL);

	priv = &p->np_private;
	buf = priv->npp_head;
	priv->npp_count = 0;

	for (buf = priv->npp_head; buf != NULL; buf = buf->nb_next) {
		if (buf == priv->npp_head)
			MGETHDR(m, M_DONTWAIT, MT_HEADER);
		else
			MGET(m, M_DONTWAIT, MT_DATA);
		if (m == NULL) {
			m_freem(*m0);
			*m0 = NULL;
			return(ENOBUFS);
		}
		if (buf->nb_bytecount > buf->nb_size)
			m->m_len = buf->nb_size;
		else
			m->m_len = buf->nb_bytecount;
		m->m_data = buf->nb_mappedsystemva;
		MEXTADD(m, m->m_data, m->m_len, ndis_return_packet,
		    p->np_rsvd[0], 0, EXT_NDIS);
		m->m_ext.ext_buf = (void *)p; /* XXX */
		priv->npp_count++;
		totlen += m->m_len;
		if (m->m_flags & MT_HEADER)
			*m0 = m;
		else
			prev->m_next = m;
		prev = m;
	}

	(*m0)->m_pkthdr.len = totlen;

	return(0);
}

/*
 * Create an mbuf chain from an NDIS packet chain.
 * This is used mainly when transmitting packets, where we need
 * to turn an mbuf off an interface's send queue and transform it
 * into an NDIS packet which will be fed into the NDIS driver's
 * send routine.
 *
 * NDIS packets consist of two parts: an ndis_packet structure,
 * which is vaguely analagous to the pkthdr portion of an mbuf,
 * and one or more ndis_buffer structures, which define the
 * actual memory segments in which the packet data resides.
 * We need to allocate one ndis_buffer for each mbuf in a chain,
 * plus one ndis_packet as the header.
 */

int
ndis_mtop(m0, p)
	struct mbuf		*m0;
	ndis_packet		**p;
{
	struct mbuf		*m;
	ndis_buffer		*buf = NULL, *prev = NULL;
	ndis_packet_private	*priv;

	if (p == NULL || m0 == NULL)
		return(EINVAL);

	/* If caller didn't supply a packet, make one. */
	if (*p == NULL) {
		*p = malloc(sizeof(ndis_packet), M_DEVBUF, M_NOWAIT|M_ZERO);

		if (*p == NULL)
			return(ENOMEM);
	}
	
	priv = &(*p)->np_private;
	priv->npp_totlen = m0->m_pkthdr.len;
        priv->npp_packetooboffset = offsetof(ndis_packet, np_oob);

	for (m = m0; m != NULL; m = m->m_next) {
		if (m->m_len == NULL)
			continue;
		buf = malloc(sizeof(ndis_buffer), M_DEVBUF, M_NOWAIT|M_ZERO);
		if (buf == NULL) {
			ndis_free_packet(*p);
			*p = NULL;
			return(ENOMEM);
		}

		buf->nb_bytecount = m->m_len;
		buf->nb_mappedsystemva = m->m_data;
		if (priv->npp_head == NULL)
			priv->npp_head = buf;
		else
			prev->nb_next = buf;
		prev = buf;
	}

	priv->npp_tail = buf;

	return(0);
}

int
ndis_get_supported_oids(arg, oids, oidcnt)
	void			*arg;
	ndis_oid		**oids;
	int			*oidcnt;
{
	int			len, rval;
	ndis_oid		*o;

	if (arg == NULL || oids == NULL || oidcnt == NULL)
		return(EINVAL);
	len = 0;
	ndis_get_info(arg, OID_GEN_SUPPORTED_LIST, NULL, &len);

	o = malloc(len, M_DEVBUF, M_NOWAIT);
	if (o == NULL)
		return(ENOMEM);

	rval = ndis_get_info(arg, OID_GEN_SUPPORTED_LIST, o, &len);

	if (rval) {
		free(o, M_DEVBUF);
		return(rval);
	}

	*oids = o;
	*oidcnt = len / 4;

	return(0);
}

int
ndis_set_info(arg, oid, buf, buflen)
	void			*arg;
	ndis_oid		oid;
	void			*buf;
	int			*buflen;
{
	struct ndis_softc	*sc;
	ndis_status		rval;
	ndis_handle		adapter;
	__stdcall ndis_setinfo_handler	setfunc;
	uint32_t		byteswritten = 0, bytesneeded = 0;
	struct timeval		tv;
	int			error;

	sc = arg;
	setfunc = sc->ndis_chars.nmc_setinfo_func;
	adapter = sc->ndis_block.nmb_miniportadapterctx;

	rval = setfunc(adapter, oid, buf, *buflen,
	    &byteswritten, &bytesneeded);

	if (rval == NDIS_STATUS_PENDING) {
		tv.tv_sec = 60;
		tv.tv_usec = 0;
		error = tsleep(&sc->ndis_block.nmb_wkupdpctimer,
		    PPAUSE|PCATCH, "ndis", tvtohz(&tv));
		rval = sc->ndis_block.nmb_setstat;
	}

	if (byteswritten)
		*buflen = byteswritten;
	if (bytesneeded)
		*buflen = bytesneeded;

	if (rval == NDIS_STATUS_INVALID_LENGTH)
		return(ENOSPC);

	if (rval == NDIS_STATUS_INVALID_OID)
		return(EINVAL);

	if (rval == NDIS_STATUS_NOT_SUPPORTED ||
	    rval == NDIS_STATUS_NOT_ACCEPTED)
		return(ENOTSUP);

	return(0);
}

int
ndis_send_packets(arg, packets, cnt)
	void			*arg;
	ndis_packet		**packets;
	int			cnt;
{
	struct ndis_softc	*sc;
	ndis_handle		adapter;
	__stdcall ndis_sendmulti_handler	sendfunc;

	sc = arg;
	adapter = sc->ndis_block.nmb_miniportadapterctx;
	sendfunc = sc->ndis_chars.nmc_sendmulti_func;
	sendfunc(adapter, packets, cnt);

	return(0);
}

int
ndis_init_dma(arg)
	void			*arg;
{
	struct ndis_softc	*sc;
	int			i, error;

	sc = arg;

	sc->ndis_tmaps = malloc(sizeof(bus_dmamap_t) * sc->ndis_maxpkts,
	    M_DEVBUF, M_NOWAIT|M_ZERO);

	if (sc->ndis_tmaps == NULL)
		return(ENOMEM);

	for (i = 0; i < sc->ndis_maxpkts; i++) {
		error = bus_dmamap_create(sc->ndis_ttag, 0,
		    &sc->ndis_tmaps[i]);
		if (error) {
			free(sc->ndis_tmaps, M_DEVBUF);
			return(ENODEV);
		}
	}

	return(0);
}

int
ndis_destroy_dma(arg)
	void			*arg;
{
	struct ndis_softc	*sc;
	struct mbuf		*m;
	ndis_packet		*p = NULL;
	int			i;

	sc = arg;

	for (i = 0; i < sc->ndis_maxpkts; i++) {
		if (sc->ndis_txarray[i] != NULL) {
			p = sc->ndis_txarray[i];
			m = (struct mbuf *)p->np_rsvd[1];
			if (m != NULL)
				m_freem(m);
			ndis_free_packet(sc->ndis_txarray[i]);
		}
		bus_dmamap_destroy(sc->ndis_ttag, sc->ndis_tmaps[i]);
	}

	free(sc->ndis_tmaps, M_DEVBUF);

	bus_dma_tag_destroy(sc->ndis_ttag);

	return(0);
}

int
ndis_reset_nic(arg)
	void			*arg;
{
	struct ndis_softc	*sc;
	ndis_handle		adapter;
	__stdcall ndis_reset_handler	resetfunc;
	uint8_t			addressing_reset;
	struct ifnet		*ifp;

	sc = arg;
	ifp = &sc->arpcom.ac_if;
	adapter = sc->ndis_block.nmb_miniportadapterctx;
	if (adapter == NULL)
		return(EIO);
	resetfunc = sc->ndis_chars.nmc_reset_func;

	if (resetfunc == NULL)
		return(EINVAL);

	resetfunc(&addressing_reset, adapter);

	return(0);
}

int
ndis_halt_nic(arg)
	void			*arg;
{
	struct ndis_softc	*sc;
	ndis_handle		adapter;
	__stdcall ndis_halt_handler	haltfunc;
	struct ifnet		*ifp;

	sc = arg;
	ifp = &sc->arpcom.ac_if;
	adapter = sc->ndis_block.nmb_miniportadapterctx;
	if (adapter == NULL)
		return(EIO);
	haltfunc = sc->ndis_chars.nmc_halt_func;

	if (haltfunc == NULL)
		return(EINVAL);

	haltfunc(adapter);

	/*
	 * The adapter context is only valid after the init
	 * handler has been called, and is invalid once the
	 * halt handler has been called.
	 */

	sc->ndis_block.nmb_miniportadapterctx = NULL;

	return(0);
}

int
ndis_shutdown_nic(arg)
	void			*arg;
{
	struct ndis_softc	*sc;
	ndis_handle		adapter;
	__stdcall ndis_shutdown_handler	shutdownfunc;


	sc = arg;
	adapter = sc->ndis_block.nmb_miniportadapterctx;
	if (adapter == NULL)
		return(EIO);
	shutdownfunc = sc->ndis_chars.nmc_shutdown_handler;

	if (shutdownfunc == NULL)
		return(EINVAL);

	if (sc->ndis_chars.nmc_rsvd0 == NULL)
		shutdownfunc(adapter);
	else
		shutdownfunc(sc->ndis_chars.nmc_rsvd0);

	return(0);
}

int
ndis_init_nic(arg)
	void			*arg;
{
	struct ndis_softc	*sc;
	ndis_miniport_block	*block;
        __stdcall ndis_init_handler	initfunc;
	ndis_status		status, openstatus = 0;
	ndis_medium		mediumarray[NdisMediumMax];
	uint32_t		chosenmedium, i;

	if (arg == NULL)
		return(EINVAL);

	sc = arg;
	block = &sc->ndis_block;
	initfunc = sc->ndis_chars.nmc_init_func;

	for (i = 0; i < NdisMediumMax; i++)
		mediumarray[i] = i;

        status = initfunc(&openstatus, &chosenmedium,
            mediumarray, NdisMediumMax, block, block);

	/*
	 * If the init fails, blow away the other exported routines
	 * we obtained from the driver so we can't call them later.
	 * If the init failed, none of these will work.
	 */
	if (status != NDIS_STATUS_SUCCESS) {
		bzero((char *)&sc->ndis_chars,
		    sizeof(ndis_miniport_characteristics));
		return(ENXIO);
	}

	return(0);
}

void
ndis_enable_intr(arg)
	void			*arg;
{
	struct ndis_softc	*sc;
	ndis_handle		adapter;
	__stdcall ndis_enable_interrupts_handler	intrenbfunc;

	sc = arg;
	adapter = sc->ndis_block.nmb_miniportadapterctx;
	if (adapter == NULL)
	    return;
	intrenbfunc = sc->ndis_chars.nmc_enable_interrupts_func;
	if (intrenbfunc == NULL)
		return;
	intrenbfunc(adapter);

	return;
}

void
ndis_disable_intr(arg)
	void			*arg;
{
	struct ndis_softc	*sc;
	ndis_handle		adapter;
	__stdcall ndis_disable_interrupts_handler	intrdisfunc;

	sc = arg;
	adapter = sc->ndis_block.nmb_miniportadapterctx;
	if (adapter == NULL)
	    return;
	intrdisfunc = sc->ndis_chars.nmc_disable_interrupts_func;
	if (intrdisfunc == NULL)
		return;
	intrdisfunc(adapter);

	return;
}

int
ndis_isr(arg, ourintr, callhandler)
	void			*arg;
	int			*ourintr;
	int			*callhandler;
{
	struct ndis_softc	*sc;
	ndis_handle		adapter;
	__stdcall ndis_isr_handler	isrfunc;
	uint8_t			accepted, queue;

	if (arg == NULL || ourintr == NULL || callhandler == NULL)
		return(EINVAL);

	sc = arg;
	adapter = sc->ndis_block.nmb_miniportadapterctx;
	isrfunc = sc->ndis_chars.nmc_isr_func;
	isrfunc(&accepted, &queue, adapter);
	*ourintr = accepted;
	*callhandler = queue;

	return(0);
}

int
ndis_intrhand(arg)
	void			*arg;
{
	struct ndis_softc	*sc;
	ndis_handle		adapter;
	__stdcall ndis_interrupt_handler	intrfunc;

	if (arg == NULL)
		return(EINVAL);

	sc = arg;
	adapter = sc->ndis_block.nmb_miniportadapterctx;
	intrfunc = sc->ndis_chars.nmc_interrupt_func;
	intrfunc(adapter);

	return(0);
}

int
ndis_get_info(arg, oid, buf, buflen)
	void			*arg;
	ndis_oid		oid;
	void			*buf;
	int			*buflen;
{
	struct ndis_softc	*sc;
	ndis_status		rval;
	ndis_handle		adapter;
	__stdcall ndis_queryinfo_handler	queryfunc;
	uint32_t		byteswritten = 0, bytesneeded = 0;
	struct timeval		tv;
	int			error;

	sc = arg;
	queryfunc = sc->ndis_chars.nmc_queryinfo_func;
	adapter = sc->ndis_block.nmb_miniportadapterctx;

	rval = queryfunc(adapter, oid, buf, *buflen,
	    &byteswritten, &bytesneeded);

	/* Wait for requests that block. */

	if (rval == NDIS_STATUS_PENDING) {
		tv.tv_sec = 60;
		tv.tv_usec = 0;
		error = tsleep(&sc->ndis_block.nmb_wkupdpctimer,
		    PPAUSE|PCATCH, "ndis", tvtohz(&tv));
		rval = sc->ndis_block.nmb_getstat;
	}

	if (byteswritten)
		*buflen = byteswritten;
	if (bytesneeded)
		*buflen = bytesneeded;

	if (rval == NDIS_STATUS_INVALID_LENGTH ||
	    rval == NDIS_STATUS_BUFFER_TOO_SHORT)
		return(ENOSPC);

	if (rval == NDIS_STATUS_INVALID_OID)
		return(EINVAL);

	if (rval == NDIS_STATUS_NOT_SUPPORTED ||
	    rval == NDIS_STATUS_NOT_ACCEPTED)
		return(ENOTSUP);

	return(0);
}

int
ndis_unload_driver(arg)
	void			*arg;
{
	struct ndis_softc	*sc;

	sc = arg;

	free(sc->ndis_block.nmb_rlist, M_DEVBUF);

	ndis_flush_sysctls(sc);
	ndis_libfini();
	ntoskrnl_libfini();

	return(0);
}

int
ndis_load_driver(img, arg)
	vm_offset_t		img;
	void			*arg;
{
	__stdcall driver_entry	entry;
	image_optional_header	opt_hdr;
	image_import_descriptor imp_desc;
	ndis_unicode_string	dummystr;
	ndis_driver_object	drv;
        ndis_miniport_block     *block;
	ndis_status		status;
	int			idx;
	uint32_t		*ptr;
	struct ndis_softc	*sc;

	sc = arg;

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

	/* Initialize subsystems */
	ndis_libinit();
	ntoskrnl_libinit();

        /* Locate the driver entry point */
	pe_get_optional_header(img, &opt_hdr);
	entry = (driver_entry)pe_translate_addr(img, opt_hdr.ioh_entryaddr);

	/*
	 * Now call the DriverEntry() routine. This will cause
	 * a callout to the NdisInitializeWrapper() and
	 * NdisMRegisterMiniport() routines.
	 */
	dummystr.nus_len = strlen(NDIS_DUMMY_PATH);
	dummystr.nus_maxlen = strlen(NDIS_DUMMY_PATH);
	dummystr.nus_buf = NULL;
	ndis_ascii_to_unicode(NDIS_DUMMY_PATH, &dummystr.nus_buf);
	drv.ndo_ifname = "ndis0";

	status = entry(&drv, &dummystr);

	free (dummystr.nus_buf, M_DEVBUF);

	if (status != NDIS_STATUS_SUCCESS)
		return(ENODEV);

	/*
	 * Now that we have the miniport driver characteristics,
	 * create an NDIS block and call the init handler.
	 * This will cause the driver to try to probe for
	 * a device.
	 */

	block = &sc->ndis_block;
	bcopy((char *)&drv.ndo_chars, (char *)&sc->ndis_chars,
	    sizeof(ndis_miniport_characteristics));

	/*block->nmb_signature = 0xcafebabe;*/

		ptr = (uint32_t *)block;
	for (idx = 0; idx < sizeof(ndis_miniport_block) / 4; idx++) {
		*ptr = idx | 0xdead0000;
		ptr++;
	}

	block->nmb_signature = (void *)0xcafebabe;
	block->nmb_setdone_func = ndis_setdone_func;
	block->nmb_querydone_func = ndis_getdone_func;
	block->nmb_status_func = ndis_status_func;
	block->nmb_statusdone_func = ndis_statusdone_func;
	block->nmb_resetdone_func = ndis_resetdone_func;

	block->nmb_ifp = &sc->arpcom.ac_if;
	block->nmb_dev = sc->ndis_dev;

	return(0);
}
