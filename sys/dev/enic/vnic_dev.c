/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2017 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 */

#include "enic.h"
#include "vnic_dev.h"
#include "vnic_resource.h"
#include "vnic_devcmd.h"
#include "vnic_nic.h"
#include "vnic_stats.h"

#define VNIC_MAX_RES_HDR_SIZE \
	(sizeof(struct vnic_resource_header) + \
	sizeof(struct vnic_resource) * RES_TYPE_MAX)
#define VNIC_RES_STRIDE	128

#define VNIC_MAX_FLOW_COUNTERS 2048

void *vnic_dev_priv(struct vnic_dev *vdev)
{
	return vdev->priv;
}

void vnic_register_cbacks(struct vnic_dev *vdev,
	void *(*alloc_consistent)(void *priv, size_t size,
	    bus_addr_t *dma_handle, struct iflib_dma_info *res,u8 *name),
	void (*free_consistent)(void *priv,
	    size_t size, void *vaddr,
	    bus_addr_t dma_handle,struct iflib_dma_info *res))
{
	vdev->alloc_consistent = alloc_consistent;
	vdev->free_consistent = free_consistent;
}

static int vnic_dev_discover_res(struct vnic_dev *vdev,
	struct vnic_dev_bar *bar, unsigned int num_bars)
{
	struct enic_softc *softc = vdev->softc;
	struct vnic_resource_header __iomem *rh;
	struct mgmt_barmap_hdr __iomem *mrh;
	struct vnic_resource __iomem *r;
	int r_offset;
	u8 type;

	if (num_bars == 0)
		return (EINVAL);

	rh = malloc(sizeof(*rh), M_DEVBUF, M_NOWAIT | M_ZERO);
	mrh = malloc(sizeof(*mrh), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (!rh) {
		pr_err("vNIC BAR0 res hdr not mem-mapped\n");
		free(rh, M_DEVBUF);
		free(mrh, M_DEVBUF);
		return (EINVAL);
	}

	/* Check for mgmt vnic in addition to normal vnic */
	ENIC_BUS_READ_REGION_4(softc, mem, 0, (void *)rh, sizeof(*rh) / 4);
	ENIC_BUS_READ_REGION_4(softc, mem, 0, (void *)mrh, sizeof(*mrh) / 4);
	if ((rh->magic != VNIC_RES_MAGIC) ||
	    (rh->version != VNIC_RES_VERSION)) {
		if ((mrh->magic != MGMTVNIC_MAGIC) ||
			mrh->version != MGMTVNIC_VERSION) {
			pr_err("vNIC BAR0 res magic/version error " \
				"exp (%lx/%lx) or (%lx/%lx), curr (%x/%x)\n",
				VNIC_RES_MAGIC, VNIC_RES_VERSION,
				MGMTVNIC_MAGIC, MGMTVNIC_VERSION,
				rh->magic, rh->version);
			free(rh, M_DEVBUF);
			free(mrh, M_DEVBUF);
			return (EINVAL);
		}
	}

	if (mrh->magic == MGMTVNIC_MAGIC)
		r_offset = sizeof(*mrh);
	else
		r_offset = sizeof(*rh);

	r = malloc(sizeof(*r), M_DEVBUF, M_NOWAIT | M_ZERO);
	ENIC_BUS_READ_REGION_4(softc, mem, r_offset, (void *)r, sizeof(*r) / 4);
	while ((type = r->type) != RES_TYPE_EOL) {
		u8 bar_num = r->bar;
		u32 bar_offset =r->bar_offset;
		u32 count = r->count;

		r_offset += sizeof(*r);

		if (bar_num >= num_bars)
			continue;

		switch (type) {
		case RES_TYPE_WQ:
		case RES_TYPE_RQ:
		case RES_TYPE_CQ:
		case RES_TYPE_INTR_CTRL:
		case RES_TYPE_INTR_PBA_LEGACY:
		case RES_TYPE_DEVCMD:
		case RES_TYPE_DEVCMD2:
			break;
		default:
			ENIC_BUS_READ_REGION_4(softc, mem, r_offset, (void *)r, sizeof(*r) / 4);
			continue;
		}

		vdev->res[type].count = count;
		bcopy(&softc->mem, &vdev->res[type].bar, sizeof(softc->mem));
		vdev->res[type].bar.offset = bar_offset;
		ENIC_BUS_READ_REGION_4(softc, mem, r_offset, (void *)r, sizeof(*r) / 4);
	}

	free(rh, M_DEVBUF);
	free(mrh, M_DEVBUF);
	free(r, M_DEVBUF);
	return 0;
}

unsigned int vnic_dev_get_res_count(struct vnic_dev *vdev,
	enum vnic_res_type type)
{
	return vdev->res[type].count;
}

void __iomem *vnic_dev_get_res(struct vnic_dev *vdev, enum vnic_res_type type,
	unsigned int index)
{
	struct vnic_res *res;

	if (!vdev->res[type].bar.tag)
		return NULL;

	res = malloc(sizeof(*res), M_DEVBUF, M_NOWAIT | M_ZERO);
	bcopy(&vdev->res[type], res, sizeof(*res));

	switch (type) {
	case RES_TYPE_WQ:
	case RES_TYPE_RQ:
	case RES_TYPE_CQ:
	case RES_TYPE_INTR_CTRL:
		res->bar.offset +=
		    index * VNIC_RES_STRIDE;
	default:
		res->bar.offset += 0;
	}

	return res;
}

unsigned int vnic_dev_desc_ring_size(struct vnic_dev_ring *ring,
	unsigned int desc_count, unsigned int desc_size)
{
	/* The base address of the desc rings must be 512 byte aligned.
	 * Descriptor count is aligned to groups of 32 descriptors.  A
	 * count of 0 means the maximum 4096 descriptors.  Descriptor
	 * size is aligned to 16 bytes.
	 */

	unsigned int count_align = 32;
	unsigned int desc_align = 16;

	ring->base_align = 512;

	if (desc_count == 0)
		desc_count = 4096;

	ring->desc_count = VNIC_ALIGN(desc_count, count_align);

	ring->desc_size = VNIC_ALIGN(desc_size, desc_align);

	ring->size_unaligned = ring->desc_count * ring->desc_size \
		+ ring->base_align;

	return ring->size_unaligned;
}

static int _vnic_dev_cmd(struct vnic_dev *vdev, enum vnic_devcmd_cmd cmd,
	int wait)
{
	struct vnic_res __iomem *devcmd = vdev->devcmd;
	int delay;
	u32 status;
	int err;

	status = ENIC_BUS_READ_4(devcmd, DEVCMD_STATUS);
	if (status == 0xFFFFFFFF) {
		/* PCI-e target device is gone */
		return (ENODEV);
	}
	if (status & STAT_BUSY) {

		pr_err("Busy devcmd %d\n",  _CMD_N(cmd));
		return (EBUSY);
	}

	if (_CMD_DIR(cmd) & _CMD_DIR_WRITE) {
		ENIC_BUS_WRITE_REGION_4(devcmd, DEVCMD_ARGS(0), (void *)&vdev->args[0], VNIC_DEVCMD_NARGS * 2);
	}

	ENIC_BUS_WRITE_4(devcmd, DEVCMD_CMD, cmd);

	if ((_CMD_FLAGS(cmd) & _CMD_FLAGS_NOWAIT)) {
		return 0;
	}

	for (delay = 0; delay < wait; delay++) {

		udelay(100);

		status = ENIC_BUS_READ_4(devcmd, DEVCMD_STATUS);
		if (status == 0xFFFFFFFF) {
			/* PCI-e target device is gone */
			return (ENODEV);
		}

		if (!(status & STAT_BUSY)) {
			if (status & STAT_ERROR) {
				err = -(int)ENIC_BUS_READ_8(devcmd, DEVCMD_ARGS(0));

				if (cmd != CMD_CAPABILITY)
					pr_err("Devcmd %d failed " \
						"with error code %d\n",
						_CMD_N(cmd), err);
				return (err);
			}

			if (_CMD_DIR(cmd) & _CMD_DIR_READ) {
				ENIC_BUS_READ_REGION_4(devcmd, bar, DEVCMD_ARGS(0), (void *)&vdev->args[0], VNIC_DEVCMD_NARGS * 2);
			}

			return 0;
		}
	}

	pr_err("Timedout devcmd %d\n", _CMD_N(cmd));
	return (ETIMEDOUT);
}

static int _vnic_dev_cmd2(struct vnic_dev *vdev, enum vnic_devcmd_cmd cmd,
	int wait)
{
	struct devcmd2_controller *dc2c = vdev->devcmd2;
	struct devcmd2_result *result;
	u8 color;
	unsigned int i;
	u32 fetch_index, new_posted;
	int delay, err;
	u32 posted = dc2c->posted;

	fetch_index = ENIC_BUS_READ_4(dc2c->wq_ctrl, TX_FETCH_INDEX);
	if (fetch_index == 0xFFFFFFFF)
		return (ENODEV);

	new_posted = (posted + 1) % DEVCMD2_RING_SIZE;

	if (new_posted == fetch_index) {
		device_printf(dev_from_vnic_dev(vdev),
		    "devcmd2 %d: wq is full. fetch index: %u, posted index: %u\n",
		    _CMD_N(cmd), fetch_index, posted);
		return (EBUSY);
	}

	dc2c->cmd_ring[posted].cmd = cmd;
	dc2c->cmd_ring[posted].flags = 0;

	if ((_CMD_FLAGS(cmd) & _CMD_FLAGS_NOWAIT))
		dc2c->cmd_ring[posted].flags |= DEVCMD2_FNORESULT;
	if (_CMD_DIR(cmd) & _CMD_DIR_WRITE)
		for (i = 0; i < VNIC_DEVCMD_NARGS; i++)
			dc2c->cmd_ring[posted].args[i] = vdev->args[i];

	ENIC_BUS_WRITE_4(dc2c->wq_ctrl, TX_POSTED_INDEX, new_posted);
	dc2c->posted = new_posted;

	if (dc2c->cmd_ring[posted].flags & DEVCMD2_FNORESULT)
		return (0);

	result = dc2c->result + dc2c->next_result;
	color = dc2c->color;

	dc2c->next_result++;
	if (dc2c->next_result == dc2c->result_size) {
		dc2c->next_result = 0;
		dc2c->color = dc2c->color ? 0 : 1;
	}

	for (delay = 0; delay < wait; delay++) {
		if (result->color == color) {
			if (result->error) {
				err = result->error;
				if (err != ERR_ECMDUNKNOWN ||
				     cmd != CMD_CAPABILITY)
					device_printf(dev_from_vnic_dev(vdev),
					     "Error %d devcmd %d\n", err,
					     _CMD_N(cmd));
				return (err);
			}
			if (_CMD_DIR(cmd) & _CMD_DIR_READ)
				for (i = 0; i < VNIC_DEVCMD2_NARGS; i++)
					vdev->args[i] = result->results[i];

			return 0;
		}
		udelay(100);
	}

	device_printf(dev_from_vnic_dev(vdev),
	    "devcmd %d timed out\n", _CMD_N(cmd));


	return (ETIMEDOUT);
}

static int vnic_dev_cmd_proxy(struct vnic_dev *vdev,
	enum vnic_devcmd_cmd proxy_cmd, enum vnic_devcmd_cmd cmd,
	u64 *args, int nargs, int wait)
{
	u32 status;
	int err;

	/*
	 * Proxy command consumes 2 arguments. One for proxy index,
	 * the other is for command to be proxied
	 */
	if (nargs > VNIC_DEVCMD_NARGS - 2) {
		pr_err("number of args %d exceeds the maximum\n", nargs);
		return (EINVAL);
	}
	memset(vdev->args, 0, sizeof(vdev->args));

	vdev->args[0] = vdev->proxy_index;
	vdev->args[1] = cmd;
	memcpy(&vdev->args[2], args, nargs * sizeof(args[0]));

	err = vdev->devcmd_rtn(vdev, proxy_cmd, wait);
	if (err)
		return (err);

	status = (u32)vdev->args[0];
	if (status & STAT_ERROR) {
		err = (int)vdev->args[1];
		if (err != ERR_ECMDUNKNOWN ||
		    cmd != CMD_CAPABILITY)
			pr_err("Error %d proxy devcmd %d\n", err, _CMD_N(cmd));
		return (err);
	}

	memcpy(args, &vdev->args[1], nargs * sizeof(args[0]));

	return 0;
}

static int vnic_dev_cmd_no_proxy(struct vnic_dev *vdev,
	enum vnic_devcmd_cmd cmd, u64 *args, int nargs, int wait)
{
	int err;

	if (nargs > VNIC_DEVCMD_NARGS) {
		pr_err("number of args %d exceeds the maximum\n", nargs);
		return (EINVAL);
	}
	memset(vdev->args, 0, sizeof(vdev->args));
	memcpy(vdev->args, args, nargs * sizeof(args[0]));

	err = vdev->devcmd_rtn(vdev, cmd, wait);

	memcpy(args, vdev->args, nargs * sizeof(args[0]));

	return (err);
}

int vnic_dev_cmd(struct vnic_dev *vdev, enum vnic_devcmd_cmd cmd,
	u64 *a0, u64 *a1, int wait)
{
	u64 args[2];
	int err;

	args[0] = *a0;
	args[1] = *a1;
	memset(vdev->args, 0, sizeof(vdev->args));

	switch (vdev->proxy) {
	case PROXY_BY_INDEX:
		err =  vnic_dev_cmd_proxy(vdev, CMD_PROXY_BY_INDEX, cmd,
				args, ARRAY_SIZE(args), wait);
		break;
	case PROXY_BY_BDF:
		err =  vnic_dev_cmd_proxy(vdev, CMD_PROXY_BY_BDF, cmd,
				args, ARRAY_SIZE(args), wait);
		break;
	case PROXY_NONE:
	default:
		err = vnic_dev_cmd_no_proxy(vdev, cmd, args, 2, wait);
		break;
	}

	if (err == 0) {
		*a0 = args[0];
		*a1 = args[1];
	}

	return (err);
}

int vnic_dev_cmd_args(struct vnic_dev *vdev, enum vnic_devcmd_cmd cmd,
		      u64 *args, int nargs, int wait)
{
	switch (vdev->proxy) {
	case PROXY_BY_INDEX:
		return vnic_dev_cmd_proxy(vdev, CMD_PROXY_BY_INDEX, cmd,
				args, nargs, wait);
	case PROXY_BY_BDF:
		return vnic_dev_cmd_proxy(vdev, CMD_PROXY_BY_BDF, cmd,
				args, nargs, wait);
	case PROXY_NONE:
	default:
		return vnic_dev_cmd_no_proxy(vdev, cmd, args, nargs, wait);
	}
}

static int vnic_dev_advanced_filters_cap(struct vnic_dev *vdev, u64 *args,
		int nargs)
{
	memset(args, 0, nargs * sizeof(*args));
	args[0] = CMD_ADD_ADV_FILTER;
	args[1] = FILTER_CAP_MODE_V1_FLAG;
	return vnic_dev_cmd_args(vdev, CMD_CAPABILITY, args, nargs, 1000);
}

int vnic_dev_capable_adv_filters(struct vnic_dev *vdev)
{
	u64 a0 = CMD_ADD_ADV_FILTER, a1 = 0;
	int wait = 1000;
	int err;

	err = vnic_dev_cmd(vdev, CMD_CAPABILITY, &a0, &a1, wait);
	if (err)
		return 0;
	return (a1 >= (u32)FILTER_DPDK_1);
}

/*  Determine the "best" filtering mode VIC is capaible of. Returns one of 3
 *  value or 0 on error:
 *	FILTER_DPDK_1- advanced filters availabile
 *	FILTER_USNIC_IP_FLAG - advanced filters but with the restriction that
 *		the IP layer must explicitly specified. I.e. cannot have a UDP
 *		filter that matches both IPv4 and IPv6.
 *	FILTER_IPV4_5TUPLE - fallback if either of the 2 above aren't available.
 *		all other filter types are not available.
 *   Retrun true in filter_tags if supported
 */
int vnic_dev_capable_filter_mode(struct vnic_dev *vdev, u32 *mode,
				 u8 *filter_actions)
{
	u64 args[4];
	int err;
	u32 max_level = 0;

	err = vnic_dev_advanced_filters_cap(vdev, args, 4);

	/* determine supported filter actions */
	*filter_actions = FILTER_ACTION_RQ_STEERING_FLAG; /* always available */
	if (args[2] == FILTER_CAP_MODE_V1)
		*filter_actions = args[3];

	if (err || ((args[0] == 1) && (args[1] == 0))) {
		/* Adv filter Command not supported or adv filters available but
		 * not enabled. Try the normal filter capability command.
		 */
		args[0] = CMD_ADD_FILTER;
		args[1] = 0;
		err = vnic_dev_cmd_args(vdev, CMD_CAPABILITY, args, 2, 1000);
		if (err)
			return (err);
		max_level = args[1];
		goto parse_max_level;
	} else if (args[2] == FILTER_CAP_MODE_V1) {
		/* parse filter capability mask in args[1] */
		if (args[1] & FILTER_DPDK_1_FLAG)
			*mode = FILTER_DPDK_1;
		else if (args[1] & FILTER_USNIC_IP_FLAG)
			*mode = FILTER_USNIC_IP;
		else if (args[1] & FILTER_IPV4_5TUPLE_FLAG)
			*mode = FILTER_IPV4_5TUPLE;
		return 0;
	}
	max_level = args[1];
parse_max_level:
	if (max_level >= (u32)FILTER_USNIC_IP)
		*mode = FILTER_USNIC_IP;
	else
		*mode = FILTER_IPV4_5TUPLE;
	return 0;
}

void vnic_dev_capable_udp_rss_weak(struct vnic_dev *vdev, bool *cfg_chk,
				   bool *weak)
{
	u64 a0 = CMD_NIC_CFG, a1 = 0;
	int wait = 1000;
	int err;

	*cfg_chk = false;
	*weak = false;
	err = vnic_dev_cmd(vdev, CMD_CAPABILITY, &a0, &a1, wait);
	if (err == 0 && a0 != 0 && a1 != 0) {
		*cfg_chk = true;
		*weak = !!((a1 >> 32) & CMD_NIC_CFG_CAPF_UDP_WEAK);
	}
}

int vnic_dev_capable(struct vnic_dev *vdev, enum vnic_devcmd_cmd cmd)
{
	u64 a0 = (u32)cmd, a1 = 0;
	int wait = 1000;
	int err;

	err = vnic_dev_cmd(vdev, CMD_CAPABILITY, &a0, &a1, wait);

	return !(err || a0);
}

int vnic_dev_spec(struct vnic_dev *vdev, unsigned int offset, size_t size,
	void *value)
{
	u64 a0, a1;
	int wait = 1000;
	int err;

	a0 = offset;
	a1 = size;

	err = vnic_dev_cmd(vdev, CMD_DEV_SPEC, &a0, &a1, wait);

	switch (size) {
	case 1:
		*(u8 *)value = (u8)a0;
		break;
	case 2:
		*(u16 *)value = (u16)a0;
		break;
	case 4:
		*(u32 *)value = (u32)a0;
		break;
	case 8:
		*(u64 *)value = a0;
		break;
	default:
		BUG();
		break;
	}

	return (err);
}

int vnic_dev_stats_clear(struct vnic_dev *vdev)
{
	u64 a0 = 0, a1 = 0;
	int wait = 1000;

	return vnic_dev_cmd(vdev, CMD_STATS_CLEAR, &a0, &a1, wait);
}

int vnic_dev_stats_dump(struct vnic_dev *vdev, struct vnic_stats **stats)
{
	u64 a0, a1;
	int wait = 1000;
	int rc;

	if (!vdev->stats)
		return (ENOMEM);

	*stats = vdev->stats;
	a0 = vdev->stats_res.idi_paddr;
	a1 = sizeof(struct vnic_stats);

	bus_dmamap_sync(vdev->stats_res.idi_tag,
			vdev->stats_res.idi_map,
			BUS_DMASYNC_POSTREAD);
	rc = vnic_dev_cmd(vdev, CMD_STATS_DUMP, &a0, &a1, wait);
	bus_dmamap_sync(vdev->stats_res.idi_tag,
			vdev->stats_res.idi_map,
			BUS_DMASYNC_PREREAD);
	return (rc);
}

/*
 * Configure counter DMA
 */
int vnic_dev_counter_dma_cfg(struct vnic_dev *vdev, u32 period,
			     u32 num_counters)
{
	u64 args[3];
	int wait = 1000;
	int err;

	if (num_counters > VNIC_MAX_FLOW_COUNTERS)
		return (ENOMEM);
	if (period > 0 && (period < VNIC_COUNTER_DMA_MIN_PERIOD ||
	    num_counters == 0))
		return (EINVAL);

	args[0] = num_counters;
	args[1] = vdev->flow_counters_res.idi_paddr;
	args[2] = period;
	bus_dmamap_sync(vdev->flow_counters_res.idi_tag,
			vdev->flow_counters_res.idi_map,
			BUS_DMASYNC_POSTREAD);
	err =  vnic_dev_cmd_args(vdev, CMD_COUNTER_DMA_CONFIG, args, 3, wait);
	bus_dmamap_sync(vdev->flow_counters_res.idi_tag,
			vdev->flow_counters_res.idi_map,
			BUS_DMASYNC_PREREAD);

	/* record if DMAs need to be stopped on close */
	if (!err)
		vdev->flow_counters_dma_active = (num_counters != 0 &&
						  period != 0);

	return (err);
}

int vnic_dev_close(struct vnic_dev *vdev)
{
	u64 a0 = 0, a1 = 0;
	int wait = 1000;

	return vnic_dev_cmd(vdev, CMD_CLOSE, &a0, &a1, wait);
}

int vnic_dev_enable_wait(struct vnic_dev *vdev)
{
	u64 a0 = 0, a1 = 0;
	int wait = 1000;

	if (vnic_dev_capable(vdev, CMD_ENABLE_WAIT))
		return vnic_dev_cmd(vdev, CMD_ENABLE_WAIT, &a0, &a1, wait);
	else
		return vnic_dev_cmd(vdev, CMD_ENABLE, &a0, &a1, wait);
}

int vnic_dev_disable(struct vnic_dev *vdev)
{
	u64 a0 = 0, a1 = 0;
	int wait = 1000;

	return vnic_dev_cmd(vdev, CMD_DISABLE, &a0, &a1, wait);
}

int vnic_dev_open(struct vnic_dev *vdev, int arg)
{
	u64 a0 = (u32)arg, a1 = 0;
	int wait = 1000;

	return vnic_dev_cmd(vdev, CMD_OPEN, &a0, &a1, wait);
}

int vnic_dev_open_done(struct vnic_dev *vdev, int *done)
{
	u64 a0 = 0, a1 = 0;
	int wait = 1000;
	int err;

	*done = 0;

	err = vnic_dev_cmd(vdev, CMD_OPEN_STATUS, &a0, &a1, wait);
	if (err)
		return (err);

	*done = (a0 == 0);

	return 0;
}

int vnic_dev_get_mac_addr(struct vnic_dev *vdev, u8 *mac_addr)
{
	u64 a0 = 0, a1 = 0;
	int wait = 1000;
	int err, i;

	for (i = 0; i < ETH_ALEN; i++)
		mac_addr[i] = 0;

	err = vnic_dev_cmd(vdev, CMD_GET_MAC_ADDR, &a0, &a1, wait);
	if (err)
		return (err);

	for (i = 0; i < ETH_ALEN; i++)
		mac_addr[i] = ((u8 *)&a0)[i];

	return 0;
}

int vnic_dev_packet_filter(struct vnic_dev *vdev, int directed, int multicast,
	int broadcast, int promisc, int allmulti)
{
	u64 a0, a1 = 0;
	int wait = 1000;
	int err;

	a0 = (directed ? CMD_PFILTER_DIRECTED : 0) |
	     (multicast ? CMD_PFILTER_MULTICAST : 0) |
	     (broadcast ? CMD_PFILTER_BROADCAST : 0) |
	     (promisc ? CMD_PFILTER_PROMISCUOUS : 0) |
	     (allmulti ? CMD_PFILTER_ALL_MULTICAST : 0);

	err = vnic_dev_cmd(vdev, CMD_PACKET_FILTER, &a0, &a1, wait);
	if (err)
		pr_err("Can't set packet filter\n");

	return (err);
}

int vnic_dev_add_addr(struct vnic_dev *vdev, u8 *addr)
{
	u64 a0 = 0, a1 = 0;
	int wait = 1000;
	int err;
	int i;

	for (i = 0; i < ETH_ALEN; i++)
		((u8 *)&a0)[i] = addr[i];

	err = vnic_dev_cmd(vdev, CMD_ADDR_ADD, &a0, &a1, wait);
	if (err)
		pr_err("Can't add addr [%02x:%02x:%02x:%02x:%02x:%02x], %d\n",
			addr[0], addr[1], addr[2], addr[3], addr[4], addr[5],
			err);

	return (err);
}

int vnic_dev_del_addr(struct vnic_dev *vdev, u8 *addr)
{
	u64 a0 = 0, a1 = 0;
	int wait = 1000;
	int err;
	int i;

	for (i = 0; i < ETH_ALEN; i++)
		((u8 *)&a0)[i] = addr[i];

	err = vnic_dev_cmd(vdev, CMD_ADDR_DEL, &a0, &a1, wait);
	if (err)
		pr_err("Can't del addr [%02x:%02x:%02x:%02x:%02x:%02x], %d\n",
			addr[0], addr[1], addr[2], addr[3], addr[4], addr[5],
			err);

	return (err);
}

int vnic_dev_set_ig_vlan_rewrite_mode(struct vnic_dev *vdev,
	u8 ig_vlan_rewrite_mode)
{
	u64 a0 = ig_vlan_rewrite_mode, a1 = 0;
	int wait = 1000;

	if (vnic_dev_capable(vdev, CMD_IG_VLAN_REWRITE_MODE))
		return vnic_dev_cmd(vdev, CMD_IG_VLAN_REWRITE_MODE,
				&a0, &a1, wait);
	else
		return 0;
}

void vnic_dev_set_reset_flag(struct vnic_dev *vdev, int state)
{
	vdev->in_reset = state;
}

static inline int vnic_dev_in_reset(struct vnic_dev *vdev)
{
	return vdev->in_reset;
}

int vnic_dev_notify_setcmd(struct vnic_dev *vdev,
	void *notify_addr, bus_addr_t notify_pa, u16 intr)
{
	u64 a0, a1;
	int wait = 1000;
	int r;

	bus_dmamap_sync(vdev->notify_res.idi_tag,
			vdev->notify_res.idi_map,
			BUS_DMASYNC_PREWRITE);
	memset(notify_addr, 0, sizeof(struct vnic_devcmd_notify));
	bus_dmamap_sync(vdev->notify_res.idi_tag,
			vdev->notify_res.idi_map,
			BUS_DMASYNC_POSTWRITE);
	if (!vnic_dev_in_reset(vdev)) {
		vdev->notify = notify_addr;
		vdev->notify_pa = notify_pa;
	}

	a0 = (u64)notify_pa;
	a1 = ((u64)intr << 32) & 0x0000ffff00000000ULL;
	a1 += sizeof(struct vnic_devcmd_notify);

	r = vnic_dev_cmd(vdev, CMD_NOTIFY, &a0, &a1, wait);
	if (!vnic_dev_in_reset(vdev))
		vdev->notify_sz = (r == 0) ? (u32)a1 : 0;

	return r;
}

int vnic_dev_notify_set(struct vnic_dev *vdev, u16 intr)
{
	void *notify_addr = NULL;
	bus_addr_t notify_pa = 0;
	char name[NAME_MAX];
	static u32 instance;

	if (vdev->notify || vdev->notify_pa) {
		return vnic_dev_notify_setcmd(vdev, vdev->notify,
					      vdev->notify_pa, intr);
	}
	if (!vnic_dev_in_reset(vdev)) {
		snprintf((char *)name, sizeof(name),
			"vnic_notify-%u", instance++);
		iflib_dma_alloc(vdev->softc->ctx,
				     sizeof(struct vnic_devcmd_notify),
				     &vdev->notify_res, BUS_DMA_NOWAIT);
		notify_pa = vdev->notify_res.idi_paddr;
		notify_addr = vdev->notify_res.idi_vaddr;
	}

	return vnic_dev_notify_setcmd(vdev, notify_addr, notify_pa, intr);
}

int vnic_dev_notify_unsetcmd(struct vnic_dev *vdev)
{
	u64 a0, a1;
	int wait = 1000;
	int err;

	a0 = 0;  /* paddr = 0 to unset notify buffer */
	a1 = 0x0000ffff00000000ULL; /* intr num = -1 to unreg for intr */
	a1 += sizeof(struct vnic_devcmd_notify);

	err = vnic_dev_cmd(vdev, CMD_NOTIFY, &a0, &a1, wait);
	if (!vnic_dev_in_reset(vdev)) {
		vdev->notify = NULL;
		vdev->notify_pa = 0;
		vdev->notify_sz = 0;
	}

	return (err);
}

int vnic_dev_notify_unset(struct vnic_dev *vdev)
{
	if (vdev->notify && !vnic_dev_in_reset(vdev)) {
		iflib_dma_free(&vdev->notify_res);
	}

	return vnic_dev_notify_unsetcmd(vdev);
}

static int vnic_dev_notify_ready(struct vnic_dev *vdev)
{
	u32 *words;
	unsigned int nwords = vdev->notify_sz / 4;
	unsigned int i;
	u32 csum;

	if (!vdev->notify || !vdev->notify_sz)
		return 0;

	do {
		csum = 0;
		bus_dmamap_sync(vdev->notify_res.idi_tag,
				vdev->notify_res.idi_map,
				BUS_DMASYNC_PREREAD);
		memcpy(&vdev->notify_copy, vdev->notify, vdev->notify_sz);
		bus_dmamap_sync(vdev->notify_res.idi_tag,
				vdev->notify_res.idi_map,
				BUS_DMASYNC_POSTREAD);
		words = (u32 *)&vdev->notify_copy;
		for (i = 1; i < nwords; i++)
			csum += words[i];
	} while (csum != words[0]);


	return (1);
}

int vnic_dev_init(struct vnic_dev *vdev, int arg)
{
	u64 a0 = (u32)arg, a1 = 0;
	int wait = 1000;
	int r = 0;

	if (vnic_dev_capable(vdev, CMD_INIT))
		r = vnic_dev_cmd(vdev, CMD_INIT, &a0, &a1, wait);
	else {
		vnic_dev_cmd(vdev, CMD_INIT_v1, &a0, &a1, wait);
		if (a0 & CMD_INITF_DEFAULT_MAC) {
			/* Emulate these for old CMD_INIT_v1 which
			 * didn't pass a0 so no CMD_INITF_*.
			 */
			vnic_dev_cmd(vdev, CMD_GET_MAC_ADDR, &a0, &a1, wait);
			vnic_dev_cmd(vdev, CMD_ADDR_ADD, &a0, &a1, wait);
		}
	}
	return r;
}

void vnic_dev_intr_coal_timer_info_default(struct vnic_dev *vdev)
{
	/* Default: hardware intr coal timer is in units of 1.5 usecs */
	vdev->intr_coal_timer_info.mul = 2;
	vdev->intr_coal_timer_info.div = 3;
	vdev->intr_coal_timer_info.max_usec =
		vnic_dev_intr_coal_timer_hw_to_usec(vdev, 0xffff);
}

int vnic_dev_link_status(struct vnic_dev *vdev)
{
	if (!vnic_dev_notify_ready(vdev))
		return 0;

	return vdev->notify_copy.link_state;
}

u32 vnic_dev_port_speed(struct vnic_dev *vdev)
{
	if (!vnic_dev_notify_ready(vdev))
		return 0;

	return vdev->notify_copy.port_speed;
}

u32 vnic_dev_intr_coal_timer_usec_to_hw(struct vnic_dev *vdev, u32 usec)
{
	return (usec * vdev->intr_coal_timer_info.mul) /
		vdev->intr_coal_timer_info.div;
}

u32 vnic_dev_intr_coal_timer_hw_to_usec(struct vnic_dev *vdev, u32 hw_cycles)
{
	return (hw_cycles * vdev->intr_coal_timer_info.div) /
		vdev->intr_coal_timer_info.mul;
}

u32 vnic_dev_get_intr_coal_timer_max(struct vnic_dev *vdev)
{
	return vdev->intr_coal_timer_info.max_usec;
}

u32 vnic_dev_mtu(struct vnic_dev *vdev)
{
	if (!vnic_dev_notify_ready(vdev))
		return 0;

	return vdev->notify_copy.mtu;
}

void vnic_dev_set_intr_mode(struct vnic_dev *vdev,
        enum vnic_dev_intr_mode intr_mode)
{
	vdev->intr_mode = intr_mode;
}

enum vnic_dev_intr_mode vnic_dev_get_intr_mode(
        struct vnic_dev *vdev)
{
	return vdev->intr_mode;
}


int vnic_dev_alloc_stats_mem(struct vnic_dev *vdev)
{
	char name[NAME_MAX];
	static u32 instance;
	struct enic_softc *softc;

	softc = vdev->softc;

	snprintf((char *)name, sizeof(name), "vnic_stats-%u", instance++);
	iflib_dma_alloc(softc->ctx, sizeof(struct vnic_stats), &vdev->stats_res, 0);
	vdev->stats = (struct vnic_stats *)vdev->stats_res.idi_vaddr;
	return vdev->stats == NULL ? -ENOMEM : 0;
}

/*
 * Initialize for up to VNIC_MAX_FLOW_COUNTERS
 */
int vnic_dev_alloc_counter_mem(struct vnic_dev *vdev)
{
	char name[NAME_MAX];
	static u32 instance;
	struct enic_softc *softc;

	softc = vdev->softc;

	snprintf((char *)name, sizeof(name), "vnic_flow_ctrs-%u", instance++);
	iflib_dma_alloc(softc->ctx, sizeof(struct vnic_counter_counts) * VNIC_MAX_FLOW_COUNTERS, &vdev->flow_counters_res, 0);
	vdev->flow_counters = (struct vnic_counter_counts *)vdev->flow_counters_res.idi_vaddr;
	vdev->flow_counters_dma_active = 0;
	return (vdev->flow_counters == NULL ? ENOMEM : 0);
}

struct vnic_dev *vnic_dev_register(struct vnic_dev *vdev,
    struct enic_bar_info *mem, unsigned int num_bars)
{
	if (vnic_dev_discover_res(vdev, NULL, num_bars))
		goto err_out;

	vdev->devcmd = vnic_dev_get_res(vdev, RES_TYPE_DEVCMD, 0);
	if (!vdev->devcmd)
		goto err_out;

	return vdev;

err_out:
	return NULL;
}

static int vnic_dev_init_devcmd1(struct vnic_dev *vdev)
{
	vdev->devcmd = vnic_dev_get_res(vdev, RES_TYPE_DEVCMD, 0);
	if (!vdev->devcmd)
		return (ENODEV);
	vdev->devcmd_rtn = _vnic_dev_cmd;

	return 0;
}

static int vnic_dev_init_devcmd2(struct vnic_dev *vdev)
{
	int err;
	unsigned int fetch_index;


	err = 0;

	if (vdev->devcmd2)
		return (0);

	vdev->devcmd2 = malloc(sizeof(*vdev->devcmd2), M_DEVBUF,
	    M_NOWAIT | M_ZERO);

	if (!vdev->devcmd2) {
		return (ENOMEM);
	}

	vdev->devcmd2->color = 1;
	vdev->devcmd2->result_size = DEVCMD2_RING_SIZE;

	err = enic_wq_devcmd2_alloc(vdev, &vdev->devcmd2->wq, DEVCMD2_RING_SIZE,
	    DEVCMD2_DESC_SIZE);

	if (err) {
		goto err_free_devcmd2;
	}
	vdev->devcmd2->wq_ctrl = vdev->devcmd2->wq.ctrl;
	vdev->devcmd2->cmd_ring = vdev->devcmd2->wq.ring.descs;

	fetch_index = ENIC_BUS_READ_4(vdev->devcmd2->wq.ctrl, TX_FETCH_INDEX);
	if (fetch_index == 0xFFFFFFFF)
		return (ENODEV);

	enic_wq_init_start(&vdev->devcmd2->wq, 0, fetch_index, fetch_index, 0,
	    0);
	vdev->devcmd2->posted = fetch_index;
	vnic_wq_enable(&vdev->devcmd2->wq);

	err = vnic_dev_alloc_desc_ring(vdev, &vdev->devcmd2->results_ring,
            DEVCMD2_RING_SIZE, DEVCMD2_DESC_SIZE);
        if (err)
                goto err_free_devcmd2;

	vdev->devcmd2->result = vdev->devcmd2->results_ring.descs;
	vdev->args[0] = (u64)vdev->devcmd2->results_ring.base_addr |
	    VNIC_PADDR_TARGET;
	vdev->args[1] = DEVCMD2_RING_SIZE;

	err = _vnic_dev_cmd2(vdev, CMD_INITIALIZE_DEVCMD2, 1000);
	if (err)
		goto err_free_devcmd2;

	vdev->devcmd_rtn = _vnic_dev_cmd2;

	return (err);

err_free_devcmd2:
	err = ENOMEM;
	if (vdev->devcmd2->wq_ctrl)
		vnic_wq_free(&vdev->devcmd2->wq);
	if (vdev->devcmd2->result)
		vnic_dev_free_desc_ring(vdev, &vdev->devcmd2->results_ring);
	free(vdev->devcmd2, M_DEVBUF);
	vdev->devcmd2 = NULL;

	return (err);
}

/*
 *  vnic_dev_classifier: Add/Delete classifier entries
 *  @vdev: vdev of the device
 *  @cmd: CLSF_ADD for Add filter
 *        CLSF_DEL for Delete filter
 *  @entry: In case of ADD filter, the caller passes the RQ number in this
 *          variable.
 *          This function stores the filter_id returned by the
 *          firmware in the same variable before return;
 *
 *          In case of DEL filter, the caller passes the RQ number. Return
 *          value is irrelevant.
 * @data: filter data
 * @action: action data
 */

int vnic_dev_overlay_offload_ctrl(struct vnic_dev *vdev, u8 overlay, u8 config)
{
	u64 a0 = overlay;
	u64 a1 = config;
	int wait = 1000;

	return vnic_dev_cmd(vdev, CMD_OVERLAY_OFFLOAD_CTRL, &a0, &a1, wait);
}

int vnic_dev_overlay_offload_cfg(struct vnic_dev *vdev, u8 overlay,
				 u16 vxlan_udp_port_number)
{
	u64 a1 = vxlan_udp_port_number;
	u64 a0 = overlay;
	int wait = 1000;

	return vnic_dev_cmd(vdev, CMD_OVERLAY_OFFLOAD_CFG, &a0, &a1, wait);
}

int vnic_dev_capable_vxlan(struct vnic_dev *vdev)
{
	u64 a0 = VIC_FEATURE_VXLAN;
	u64 a1 = 0;
	int wait = 1000;
	int ret;

	ret = vnic_dev_cmd(vdev, CMD_GET_SUPP_FEATURE_VER, &a0, &a1, wait);
	/* 1 if the NIC can do VXLAN for both IPv4 and IPv6 with multiple WQs */
	return ret == 0 &&
		(a1 & (FEATURE_VXLAN_IPV6 | FEATURE_VXLAN_MULTI_WQ)) ==
		(FEATURE_VXLAN_IPV6 | FEATURE_VXLAN_MULTI_WQ);
}

bool vnic_dev_counter_alloc(struct vnic_dev *vdev, uint32_t *idx)
{
	u64 a0 = 0;
	u64 a1 = 0;
	int wait = 1000;

	if (vnic_dev_cmd(vdev, CMD_COUNTER_ALLOC, &a0, &a1, wait))
		return false;
	*idx = (uint32_t)a0;
	return true;
}

bool vnic_dev_counter_free(struct vnic_dev *vdev, uint32_t idx)
{
	u64 a0 = idx;
	u64 a1 = 0;
	int wait = 1000;

	return vnic_dev_cmd(vdev, CMD_COUNTER_FREE, &a0, &a1,
			    wait) == 0;
}

bool vnic_dev_counter_query(struct vnic_dev *vdev, uint32_t idx,
			    bool reset, uint64_t *packets, uint64_t *bytes)
{
	u64 a0 = idx;
	u64 a1 = reset ? 1 : 0;
	int wait = 1000;

	if (reset) {
		/* query/reset returns updated counters */
		if (vnic_dev_cmd(vdev, CMD_COUNTER_QUERY, &a0, &a1, wait))
			return false;
		*packets = a0;
		*bytes = a1;
	} else {
		/* Get values DMA'd from the adapter */
		*packets = vdev->flow_counters[idx].vcc_packets;
		*bytes = vdev->flow_counters[idx].vcc_bytes;
	}
	return true;
}

device_t dev_from_vnic_dev(struct vnic_dev *vdev) {
	return (vdev->softc->dev);
}

int vnic_dev_cmd_init(struct vnic_dev *vdev) {
	int err;
	void __iomem *res;

	res = vnic_dev_get_res(vdev, RES_TYPE_DEVCMD2, 0);
	if (res) {
		err = vnic_dev_init_devcmd2(vdev);
		if (err)
			device_printf(dev_from_vnic_dev(vdev),
			    "DEVCMD2 init failed, Using DEVCMD1\n");
		else
			return 0;
	}

	err = vnic_dev_init_devcmd1(vdev);

	return (err);
}
