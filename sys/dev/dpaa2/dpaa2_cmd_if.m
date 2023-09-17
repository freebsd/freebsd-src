#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright © 2021-2022 Dmitry Salychev
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

#include <machine/bus.h>
#include <dev/dpaa2/dpaa2_types.h>
#include <dev/dpaa2/dpaa2_mc.h>
#include <dev/dpaa2/dpaa2_mcp.h>

/**
 * @brief DPAA2 MC command interface.
 *
 * The primary purpose of the MC provided DPAA2 objects is to simplify DPAA2
 * hardware block usage through abstraction and encapsulation.
 */
INTERFACE dpaa2_cmd;

#
# Default implementation of the commands.
#
CODE {
	static void
	panic_on_mc(device_t dev)
	{
		if (strcmp(device_get_name(dev), "dpaa2_mc") == 0)
			panic("No one can handle a command above DPAA2 MC");
	}

	static int
	bypass_mng_get_version(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		uint32_t *major, uint32_t *minor, uint32_t *rev)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_MNG_GET_VERSION(device_get_parent(dev), child,
				cmd, major, minor, rev));
		return (ENXIO);
	}
	static int
	bypass_mng_get_soc_version(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		uint32_t *pvr, uint32_t *svr)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_MNG_GET_SOC_VERSION(
				device_get_parent(dev), child, cmd, pvr, svr));
		return (ENXIO);
	}
	static int
	bypass_mng_get_container_id(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		uint32_t *cont_id)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_MNG_GET_CONTAINER_ID(
				device_get_parent(dev), child, cmd, cont_id));
		return (ENXIO);
	}
	static int
	bypass_rc_open(device_t dev, device_t child, struct dpaa2_cmd *cmd, uint32_t cont_id,
		uint16_t *token)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_RC_OPEN(
				device_get_parent(dev), child, cmd, cont_id, token));
		return (ENXIO);
	}
	static int
	bypass_rc_close(device_t dev, device_t child, struct dpaa2_cmd *cmd)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_RC_CLOSE(
				device_get_parent(dev), child, cmd));
		return (ENXIO);
	}
	static int
	bypass_rc_get_obj_count(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		uint32_t *obj_count)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_RC_GET_OBJ_COUNT(
				device_get_parent(dev), child, cmd, obj_count));
		return (ENXIO);
	}
	static int
	bypass_rc_get_obj(device_t dev, device_t child, struct dpaa2_cmd *cmd, uint32_t obj_idx,
		struct dpaa2_obj *obj)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_RC_GET_OBJ(
				device_get_parent(dev), child, cmd, obj_idx, obj));
		return (ENXIO);
	}
	static int
	bypass_rc_get_obj_descriptor(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		uint32_t obj_id, enum dpaa2_dev_type type, struct dpaa2_obj *obj)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_RC_GET_OBJ_DESCRIPTOR(
				device_get_parent(dev), child, cmd, obj_id, type, obj));
		return (ENXIO);
	}
	static int
	bypass_rc_get_attributes(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		struct dpaa2_rc_attr *attr)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_RC_GET_ATTRIBUTES(
				device_get_parent(dev), child, cmd, attr));
		return (ENXIO);
	}
	static int
	bypass_rc_get_obj_region(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		uint32_t obj_id, uint8_t reg_idx, enum dpaa2_dev_type type,
		struct dpaa2_rc_obj_region *reg)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_RC_GET_OBJ_REGION(
				device_get_parent(dev), child, cmd, obj_id, reg_idx,
				type, reg));
		return (ENXIO);
	}
	static int
	bypass_rc_get_api_version(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		uint16_t *major, uint16_t *minor)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_RC_GET_API_VERSION(
				device_get_parent(dev), child, cmd, major, minor));
		return (ENXIO);
	}
	static int
	bypass_rc_set_irq_enable(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		uint8_t irq_idx, uint8_t enable)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_RC_SET_IRQ_ENABLE(
				device_get_parent(dev), child, cmd, irq_idx, enable));
		return (ENXIO);
	}
	static int
	bypass_rc_set_obj_irq(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		uint8_t irq_idx, uint64_t addr, uint32_t data, uint32_t irq_usr,
		uint32_t obj_id, enum dpaa2_dev_type type)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_RC_SET_OBJ_IRQ(
				device_get_parent(dev), child, cmd, irq_idx, addr, data,
				irq_usr, obj_id, type));
		return (ENXIO);
	}
	static int
	bypass_rc_get_conn(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		struct dpaa2_ep_desc *ep1_desc, struct dpaa2_ep_desc *ep2_desc,
		uint32_t *link_stat)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_RC_GET_CONN(
				device_get_parent(dev), child, cmd, ep1_desc, ep2_desc,
				link_stat));
		return (ENXIO);
	}

	static int
	bypass_ni_open(device_t dev, device_t child, struct dpaa2_cmd *cmd, uint32_t dpni_id,
		uint16_t *token)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_NI_OPEN(
				device_get_parent(dev), child, cmd, dpni_id, token));
		return (ENXIO);
	}
	static int
	bypass_ni_close(device_t dev, device_t child, struct dpaa2_cmd *cmd)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_NI_CLOSE(
				device_get_parent(dev), child, cmd));
		return (ENXIO);
	}
	static int
	bypass_ni_enable(device_t dev, device_t child, struct dpaa2_cmd *cmd)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_NI_ENABLE(
				device_get_parent(dev), child, cmd));
		return (ENXIO);
	}
	static int
	bypass_ni_disable(device_t dev, device_t child, struct dpaa2_cmd *cmd)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_NI_DISABLE(
				device_get_parent(dev), child, cmd));
		return (ENXIO);
	}
	static int
	bypass_ni_get_api_version(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		uint16_t *major, uint16_t *minor)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_NI_GET_API_VERSION(
				device_get_parent(dev), child, cmd, major, minor));
		return (ENXIO);
	}
	static int
	bypass_ni_reset(device_t dev, device_t child, struct dpaa2_cmd *cmd)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_NI_RESET(
				device_get_parent(dev), child, cmd));
		return (ENXIO);
	}
	static int
	bypass_ni_get_attributes(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		struct dpaa2_ni_attr *attr)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_NI_GET_ATTRIBUTES(
				device_get_parent(dev), child, cmd, attr));
		return (ENXIO);
	}
	static int
	bypass_ni_set_buf_layout(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		struct dpaa2_ni_buf_layout *bl)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_NI_SET_BUF_LAYOUT(
				device_get_parent(dev), child, cmd, bl));
		return (ENXIO);
	}
	static int
	bypass_ni_get_tx_data_off(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		uint16_t *offset)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_NI_GET_TX_DATA_OFF(
				device_get_parent(dev), child, cmd, offset));
		return (ENXIO);
	}
	static int
	bypass_ni_set_link_cfg(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		struct dpaa2_ni_link_cfg *cfg)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_NI_SET_LINK_CFG(
				device_get_parent(dev), child, cmd, cfg));
		return (ENXIO);
	}
	static int
	bypass_ni_get_link_cfg(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		struct dpaa2_ni_link_cfg *cfg)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_NI_GET_LINK_CFG(
				device_get_parent(dev), child, cmd, cfg));
		return (ENXIO);
	}
	static int
	bypass_ni_get_link_state(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		struct dpaa2_ni_link_state *state)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_NI_GET_LINK_STATE(
				device_get_parent(dev), child, cmd, state));
		return (ENXIO);
	}
	static int
	bypass_ni_get_port_mac_addr(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		uint8_t *mac)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_NI_GET_PORT_MAC_ADDR(
				device_get_parent(dev), child, cmd, mac));
		return (ENXIO);
	}
	static int
	bypass_ni_set_prim_mac_addr(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		uint8_t *mac)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_NI_SET_PRIM_MAC_ADDR(
				device_get_parent(dev), child, cmd, mac));
		return (ENXIO);
	}
	static int
	bypass_ni_get_prim_mac_addr(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		uint8_t *mac)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_NI_GET_PRIM_MAC_ADDR(
				device_get_parent(dev), child, cmd, mac));
		return (ENXIO);
	}
	static int
	bypass_ni_set_qos_table(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		struct dpaa2_ni_qos_table *tbl)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_NI_SET_QOS_TABLE(
				device_get_parent(dev), child, cmd, tbl));
		return (ENXIO);
	}
	static int
	bypass_ni_clear_qos_table(device_t dev, device_t child, struct dpaa2_cmd *cmd)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_NI_CLEAR_QOS_TABLE(
				device_get_parent(dev), child, cmd));
		return (ENXIO);
	}
	static int
	bypass_ni_set_pools(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		struct dpaa2_ni_pools_cfg *cfg)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_NI_SET_POOLS(
				device_get_parent(dev), child, cmd, cfg));
		return (ENXIO);
	}
	static int
	bypass_ni_set_err_behavior(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		struct dpaa2_ni_err_cfg *cfg)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_NI_SET_ERR_BEHAVIOR(
				device_get_parent(dev), child, cmd, cfg));
		return (ENXIO);
	}
	static int
	bypass_ni_get_queue(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		struct dpaa2_ni_queue_cfg *cfg)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_NI_GET_QUEUE(
				device_get_parent(dev), child, cmd, cfg));
		return (ENXIO);
	}
	static int
	bypass_ni_set_queue(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		struct dpaa2_ni_queue_cfg *cfg)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_NI_SET_QUEUE(
				device_get_parent(dev), child, cmd, cfg));
		return (ENXIO);
	}
	static int
	bypass_ni_get_qdid(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		enum dpaa2_ni_queue_type type, uint16_t *qdid)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_NI_GET_QDID(
				device_get_parent(dev), child, cmd, type, qdid));
		return (ENXIO);
	}
	static int
	bypass_ni_add_mac_addr(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		uint8_t *mac)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_NI_ADD_MAC_ADDR(
				device_get_parent(dev), child, cmd, mac));
		return (ENXIO);
	}
	static int
	bypass_ni_remove_mac_addr(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		uint8_t *mac)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_NI_REMOVE_MAC_ADDR(
				device_get_parent(dev), child, cmd, mac));
		return (ENXIO);
	}
	static int
	bypass_ni_clear_mac_filters(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		bool rm_uni, bool rm_multi)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_NI_CLEAR_MAC_FILTERS(
				device_get_parent(dev), child, cmd, rm_uni, rm_multi));
		return (ENXIO);
	}
	static int
	bypass_ni_set_mfl(device_t dev, device_t child, struct dpaa2_cmd *cmd, uint16_t length)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_NI_SET_MFL(
				device_get_parent(dev), child, cmd, length));
		return (ENXIO);
	}
	static int
	bypass_ni_set_offload(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		enum dpaa2_ni_ofl_type ofl_type, bool en)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_NI_SET_OFFLOAD(
				device_get_parent(dev), child, cmd, ofl_type, en));
		return (ENXIO);
	}
	static int
	bypass_ni_set_irq_mask(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		uint8_t irq_idx, uint32_t mask)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_NI_SET_IRQ_MASK(
				device_get_parent(dev), child, cmd, irq_idx, mask));
		return (ENXIO);
	}
	static int
	bypass_ni_set_irq_enable(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		uint8_t irq_idx, bool en)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_NI_SET_IRQ_ENABLE(
				device_get_parent(dev), child, cmd, irq_idx, en));
		return (ENXIO);
	}
	static int
	bypass_ni_get_irq_status(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		uint8_t irq_idx, uint32_t *status)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_NI_GET_IRQ_STATUS(
				device_get_parent(dev), child, cmd, irq_idx, status));
		return (ENXIO);
	}
	static int
	bypass_ni_set_uni_promisc(device_t dev, device_t child, struct dpaa2_cmd *cmd, bool en)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_NI_SET_UNI_PROMISC(
				device_get_parent(dev), child, cmd, en));
		return (ENXIO);
	}
	static int
	bypass_ni_set_multi_promisc(device_t dev, device_t child, struct dpaa2_cmd *cmd, bool en)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_NI_SET_MULTI_PROMISC(
				device_get_parent(dev), child, cmd, en));
		return (ENXIO);
	}
	static int
	bypass_ni_get_statistics(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		uint8_t page, uint16_t param, uint64_t *cnt)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_NI_GET_STATISTICS(
				device_get_parent(dev), child, cmd, page, param, cnt));
		return (ENXIO);
	}
	static int
	bypass_ni_set_rx_tc_dist(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		uint16_t dist_size, uint8_t tc, enum dpaa2_ni_dist_mode dist_mode,
		bus_addr_t key_cfg_buf)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_NI_SET_RX_TC_DIST(
				device_get_parent(dev), child, cmd, dist_size, tc,
				dist_mode, key_cfg_buf));
		return (ENXIO);
	}

	static int
	bypass_io_open(device_t dev, device_t child, struct dpaa2_cmd *cmd, uint32_t dpio_id,
		uint16_t *token)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_IO_OPEN(
				device_get_parent(dev), child, cmd, dpio_id, token));
		return (ENXIO);
	}
	static int
	bypass_io_close(device_t dev, device_t child, struct dpaa2_cmd *cmd)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_IO_CLOSE(
				device_get_parent(dev), child, cmd));
		return (ENXIO);
	}
	static int
	bypass_io_enable(device_t dev, device_t child, struct dpaa2_cmd *cmd)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_IO_ENABLE(
				device_get_parent(dev), child, cmd));
		return (ENXIO);
	}
	static int
	bypass_io_disable(device_t dev, device_t child, struct dpaa2_cmd *cmd)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_IO_DISABLE(
				device_get_parent(dev), child, cmd));
		return (ENXIO);
	}
	static int
	bypass_io_reset(device_t dev, device_t child, struct dpaa2_cmd *cmd)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_IO_RESET(
				device_get_parent(dev), child, cmd));
		return (ENXIO);
	}
	static int
	bypass_io_get_attributes(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		struct dpaa2_io_attr *attr)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_IO_GET_ATTRIBUTES(
				device_get_parent(dev), child, cmd, attr));
		return (ENXIO);
	}
	static int
	bypass_io_set_irq_mask(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		uint8_t irq_idx, uint32_t mask)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_IO_SET_IRQ_MASK(
				device_get_parent(dev), child, cmd, irq_idx, mask));
		return (ENXIO);
	}
	static int
	bypass_io_get_irq_status(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		uint8_t irq_idx, uint32_t *status)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_IO_GET_IRQ_STATUS(
				device_get_parent(dev), child, cmd, irq_idx, status));
		return (ENXIO);
	}
	static int
	bypass_io_set_irq_enable(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		uint8_t irq_idx, bool en)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_IO_SET_IRQ_ENABLE(
				device_get_parent(dev), child, cmd, irq_idx, en));
		return (ENXIO);
	}
	static int
	bypass_io_add_static_dq_chan(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		uint32_t dpcon_id, uint8_t *chan_idx)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_IO_ADD_STATIC_DQ_CHAN(
				device_get_parent(dev), child, cmd, dpcon_id, chan_idx));
		return (ENXIO);
	}

	static int
	bypass_bp_open(device_t dev, device_t child, struct dpaa2_cmd *cmd, uint32_t dpbp_id,
		uint16_t *token)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_BP_OPEN(
				device_get_parent(dev), child, cmd, dpbp_id, token));
		return (ENXIO);
	}
	static int
	bypass_bp_close(device_t dev, device_t child, struct dpaa2_cmd *cmd)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_BP_CLOSE(
				device_get_parent(dev), child, cmd));
		return (ENXIO);
	}
	static int
	bypass_bp_enable(device_t dev, device_t child, struct dpaa2_cmd *cmd)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_BP_ENABLE(
				device_get_parent(dev), child, cmd));
		return (ENXIO);
	}
	static int
	bypass_bp_disable(device_t dev, device_t child, struct dpaa2_cmd *cmd)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_BP_DISABLE(
				device_get_parent(dev), child, cmd));
		return (ENXIO);
	}
	static int
	bypass_bp_reset(device_t dev, device_t child, struct dpaa2_cmd *cmd)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_BP_RESET(
				device_get_parent(dev), child, cmd));
		return (ENXIO);
	}
	static int
	bypass_bp_get_attributes(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		struct dpaa2_bp_attr *attr)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_BP_GET_ATTRIBUTES(
				device_get_parent(dev), child, cmd, attr));
		return (ENXIO);
	}

	static int
	bypass_mac_open(device_t dev, device_t child, struct dpaa2_cmd *cmd, uint32_t dpmac_id,
		uint16_t *token)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_MAC_OPEN(
				device_get_parent(dev), child, cmd, dpmac_id, token));
		return (ENXIO);
	}
	static int
	bypass_mac_close(device_t dev, device_t child, struct dpaa2_cmd *cmd)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_MAC_CLOSE(
				device_get_parent(dev), child, cmd));
		return (ENXIO);
	}
	static int
	bypass_mac_reset(device_t dev, device_t child, struct dpaa2_cmd *cmd)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_MAC_RESET(
				device_get_parent(dev), child, cmd));
		return (ENXIO);
	}
	static int
	bypass_mac_mdio_read(device_t dev, device_t child, struct dpaa2_cmd *cmd, uint8_t phy,
		uint16_t reg, uint16_t *val)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_MAC_MDIO_READ(
				device_get_parent(dev), child, cmd, phy, reg, val));
		return (ENXIO);
	}
	static int
	bypass_mac_mdio_write(device_t dev, device_t child, struct dpaa2_cmd *cmd, uint8_t phy,
		uint16_t reg, uint16_t val)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_MAC_MDIO_WRITE(
				device_get_parent(dev), child, cmd, phy, reg, val));
		return (ENXIO);
	}
	static int
	bypass_mac_get_addr(device_t dev, device_t child, struct dpaa2_cmd *cmd, uint8_t *mac)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_MAC_GET_ADDR(
				device_get_parent(dev), child, cmd, mac));
		return (ENXIO);
	}
	static int
	bypass_mac_get_attributes(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		struct dpaa2_mac_attr *attr)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_MAC_GET_ATTRIBUTES(
				device_get_parent(dev), child, cmd, attr));
		return (ENXIO);
	}
	static int
	bypass_mac_set_link_state(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		struct dpaa2_mac_link_state *state)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_MAC_SET_LINK_STATE(
				device_get_parent(dev), child, cmd, state));
		return (ENXIO);
	}
	static int
	bypass_mac_set_irq_mask(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		uint8_t irq_idx, uint32_t mask)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_MAC_SET_IRQ_MASK(
				device_get_parent(dev), child, cmd, irq_idx, mask));
		return (ENXIO);
	}
	static int
	bypass_mac_set_irq_enable(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		uint8_t irq_idx, bool en)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_MAC_SET_IRQ_ENABLE(
				device_get_parent(dev), child, cmd, irq_idx, en));
		return (ENXIO);
	}
	static int
	bypass_mac_get_irq_status(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		uint8_t irq_idx, uint32_t *status)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_MAC_GET_IRQ_STATUS(
				device_get_parent(dev), child, cmd, irq_idx, status));
		return (ENXIO);
	}

	static int
	bypass_con_open(device_t dev, device_t child, struct dpaa2_cmd *cmd, uint32_t dpcon_id,
		uint16_t *token)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_CON_OPEN(
				device_get_parent(dev), child, cmd, dpcon_id, token));
		return (ENXIO);
	}
	static int
	bypass_con_close(device_t dev, device_t child, struct dpaa2_cmd *cmd)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_CON_CLOSE(
				device_get_parent(dev), child, cmd));
		return (ENXIO);
	}
	static int
	bypass_con_reset(device_t dev, device_t child, struct dpaa2_cmd *cmd)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_CON_RESET(
				device_get_parent(dev), child, cmd));
		return (ENXIO);
	}
	static int
	bypass_con_enable(device_t dev, device_t child, struct dpaa2_cmd *cmd)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_CON_ENABLE(
				device_get_parent(dev), child, cmd));
		return (ENXIO);
	}
	static int
	bypass_con_disable(device_t dev, device_t child, struct dpaa2_cmd *cmd)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_CON_DISABLE(
				device_get_parent(dev), child, cmd));
		return (ENXIO);
	}
	static int
	bypass_con_get_attributes(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		struct dpaa2_con_attr *attr)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_CON_GET_ATTRIBUTES(
				device_get_parent(dev), child, cmd, attr));
		return (ENXIO);
	}
	static int
	bypass_con_set_notif(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		struct dpaa2_con_notif_cfg *cfg)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_CON_SET_NOTIF(
				device_get_parent(dev), child, cmd, cfg));
		return (ENXIO);
	}

	/* Data Path MC Portal (DPMCP) commands. */

	static int
	bypass_mcp_create(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		uint32_t portal_id, uint32_t options, uint32_t *dpmcp_id)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_MCP_CREATE(
				device_get_parent(dev), child, cmd, portal_id,
				options, dpmcp_id));
		return (ENXIO);
	}
	static int
	bypass_mcp_destroy(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		uint32_t dpmcp_id)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_MCP_DESTROY(
				device_get_parent(dev), child, cmd, dpmcp_id));
		return (ENXIO);
	}
	static int
	bypass_mcp_open(device_t dev, device_t child, struct dpaa2_cmd *cmd,
		uint32_t dpmcp_id, uint16_t *token)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_MCP_OPEN(
				device_get_parent(dev), child, cmd, dpmcp_id,
				token));
		return (ENXIO);
	}
	static int
	bypass_mcp_close(device_t dev, device_t child, struct dpaa2_cmd *cmd)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_MCP_CLOSE(
				device_get_parent(dev), child, cmd));
		return (ENXIO);
	}
	static int
	bypass_mcp_reset(device_t dev, device_t child, struct dpaa2_cmd *cmd)
	{
		panic_on_mc(dev);
		if (device_get_parent(dev) != NULL)
			return (DPAA2_CMD_MCP_RESET(
				device_get_parent(dev), child, cmd));
		return (ENXIO);
	}
};

/**
 * @brief Data Path Management (DPMNG) commands.
 */

METHOD int mng_get_version {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	uint32_t	*major;
	uint32_t	*minor;
	uint32_t	*rev;
} DEFAULT bypass_mng_get_version;

METHOD int mng_get_soc_version {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	uint32_t	*pvr;
	uint32_t	*svr;
} DEFAULT bypass_mng_get_soc_version;

METHOD int mng_get_container_id {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	uint32_t	*cont_id;
} DEFAULT bypass_mng_get_container_id;

/**
 * @brief Data Path Resource Containter (DPRC) commands.
 */

METHOD int rc_open {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	uint32_t	 cont_id;
	uint16_t	*token;
} DEFAULT bypass_rc_open;

METHOD int rc_close {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
} DEFAULT bypass_rc_close;

METHOD int rc_get_obj_count {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	uint32_t	*obj_count;
} DEFAULT bypass_rc_get_obj_count;

METHOD int rc_get_obj {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	uint32_t	 obj_idx;
	struct dpaa2_obj *obj;
} DEFAULT bypass_rc_get_obj;

METHOD int rc_get_obj_descriptor {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	uint32_t	 obj_id;
	enum dpaa2_dev_type type;
	struct dpaa2_obj *obj;
} DEFAULT bypass_rc_get_obj_descriptor;

METHOD int rc_get_attributes {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	struct dpaa2_rc_attr *attr;
} DEFAULT bypass_rc_get_attributes;

METHOD int rc_get_obj_region {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	uint32_t	 obj_id;
	uint8_t		 reg_idx;
	enum dpaa2_dev_type type;
	struct dpaa2_rc_obj_region *reg;
} DEFAULT bypass_rc_get_obj_region;

METHOD int rc_get_api_version {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	uint16_t	*major;
	uint16_t	*minor;
} DEFAULT bypass_rc_get_api_version;

METHOD int rc_set_irq_enable {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	uint8_t		 irq_idx;
	uint8_t		 enable;
} DEFAULT bypass_rc_set_irq_enable;

METHOD int rc_set_obj_irq {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	uint8_t		 irq_idx;
	uint64_t	 addr;
	uint32_t	 data;
	uint32_t	 irq_usr;
	uint32_t	 obj_id;
	enum dpaa2_dev_type type;
} DEFAULT bypass_rc_set_obj_irq;

METHOD int rc_get_conn {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	struct dpaa2_ep_desc *ep1_desc;
	struct dpaa2_ep_desc *ep2_desc;
	uint32_t	*link_stat;
} DEFAULT bypass_rc_get_conn;

/**
 * @brief Data Path Network Interface (DPNI) commands.
 */

METHOD int ni_open {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	uint32_t	 dpni_id;
	uint16_t	*token;
} DEFAULT bypass_ni_open;

METHOD int ni_close {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
} DEFAULT bypass_ni_close;

METHOD int ni_enable {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
} DEFAULT bypass_ni_enable;

METHOD int ni_disable {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
} DEFAULT bypass_ni_disable;

METHOD int ni_get_api_version {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	uint16_t	*major;
	uint16_t	*minor;
} DEFAULT bypass_ni_get_api_version;

METHOD int ni_reset {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
} DEFAULT bypass_ni_reset;

METHOD int ni_get_attributes {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	struct dpaa2_ni_attr *attr;
} DEFAULT bypass_ni_get_attributes;

METHOD int ni_set_buf_layout {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	struct dpaa2_ni_buf_layout *bl;
} DEFAULT bypass_ni_set_buf_layout;

METHOD int ni_get_tx_data_off {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	uint16_t	*offset;
} DEFAULT bypass_ni_get_tx_data_off;

METHOD int ni_set_link_cfg {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	struct dpaa2_ni_link_cfg *cfg;
} DEFAULT bypass_ni_set_link_cfg;

METHOD int ni_get_link_cfg {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	struct dpaa2_ni_link_cfg *cfg;
} DEFAULT bypass_ni_get_link_cfg;

METHOD int ni_get_link_state {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	struct dpaa2_ni_link_state *state;
} DEFAULT bypass_ni_get_link_state;

METHOD int ni_get_port_mac_addr {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	uint8_t		*mac;
} DEFAULT bypass_ni_get_port_mac_addr;

METHOD int ni_set_prim_mac_addr {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	uint8_t		*mac;
} DEFAULT bypass_ni_set_prim_mac_addr;

METHOD int ni_get_prim_mac_addr {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	uint8_t		*mac;
} DEFAULT bypass_ni_get_prim_mac_addr;

METHOD int ni_set_qos_table {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	struct dpaa2_ni_qos_table *tbl;
} DEFAULT bypass_ni_set_qos_table;

METHOD int ni_clear_qos_table {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
} DEFAULT bypass_ni_clear_qos_table;

METHOD int ni_set_pools {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	struct dpaa2_ni_pools_cfg *cfg;
} DEFAULT bypass_ni_set_pools;

METHOD int ni_set_err_behavior {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	struct dpaa2_ni_err_cfg *cfg;
} DEFAULT bypass_ni_set_err_behavior;

METHOD int ni_get_queue {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	struct dpaa2_ni_queue_cfg *cfg;
} DEFAULT bypass_ni_get_queue;

METHOD int ni_set_queue {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	struct dpaa2_ni_queue_cfg *cfg;
} DEFAULT bypass_ni_set_queue;

METHOD int ni_get_qdid {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	enum dpaa2_ni_queue_type type;
	uint16_t	*qdid;
} DEFAULT bypass_ni_get_qdid;

METHOD int ni_add_mac_addr {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	uint8_t		*mac;
} DEFAULT bypass_ni_add_mac_addr;

METHOD int ni_remove_mac_addr {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	uint8_t		*mac;
} DEFAULT bypass_ni_remove_mac_addr;

METHOD int ni_clear_mac_filters {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	bool		 rm_uni;
	bool		 rm_multi;
} DEFAULT bypass_ni_clear_mac_filters;

METHOD int ni_set_mfl {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	uint16_t	 length;
} DEFAULT bypass_ni_set_mfl;

METHOD int ni_set_offload {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	enum dpaa2_ni_ofl_type ofl_type;
	bool		 en;
} DEFAULT bypass_ni_set_offload;

METHOD int ni_set_irq_mask {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	uint8_t		 irq_idx;
	uint32_t	 mask;
} DEFAULT bypass_ni_set_irq_mask;

METHOD int ni_set_irq_enable {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	uint8_t		 irq_idx;
	bool		 en;
} DEFAULT bypass_ni_set_irq_enable;

METHOD int ni_get_irq_status {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	uint8_t		 irq_idx;
	uint32_t	*status;
} DEFAULT bypass_ni_get_irq_status;

METHOD int ni_set_uni_promisc {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	bool		 en;
} DEFAULT bypass_ni_set_uni_promisc;

METHOD int ni_set_multi_promisc {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	bool		 en;
} DEFAULT bypass_ni_set_multi_promisc;

METHOD int ni_get_statistics {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	uint8_t		 page;
	uint16_t	 param;
	uint64_t	*cnt;
} DEFAULT bypass_ni_get_statistics;

METHOD int ni_set_rx_tc_dist {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	uint16_t	 dist_size;
	uint8_t		 tc;
	enum dpaa2_ni_dist_mode dist_mode;
	bus_addr_t	 key_cfg_buf;
} DEFAULT bypass_ni_set_rx_tc_dist;

/**
 * @brief Data Path I/O (DPIO) commands.
 */

METHOD int io_open {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	uint32_t	 dpio_id;
	uint16_t	*token;
} DEFAULT bypass_io_open;

METHOD int io_close {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
} DEFAULT bypass_io_close;

METHOD int io_enable {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
} DEFAULT bypass_io_enable;

METHOD int io_disable {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
} DEFAULT bypass_io_disable;

METHOD int io_reset {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
} DEFAULT bypass_io_reset;

METHOD int io_get_attributes {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	struct dpaa2_io_attr *attr;
} DEFAULT bypass_io_get_attributes;

METHOD int io_set_irq_mask {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	uint8_t		 irq_idx;
	uint32_t	 mask;
} DEFAULT bypass_io_set_irq_mask;

METHOD int io_get_irq_status {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	uint8_t		 irq_idx;
	uint32_t	*status;
} DEFAULT bypass_io_get_irq_status;

METHOD int io_set_irq_enable {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	uint8_t		 irq_idx;
	bool		 en;
} DEFAULT bypass_io_set_irq_enable;

METHOD int io_add_static_dq_chan {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	uint32_t	 dpcon_id;
	uint8_t		*chan_idx;
} DEFAULT bypass_io_add_static_dq_chan;

/**
 * @brief Data Path Buffer Pool (DPBP) commands.
 */

METHOD int bp_open {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	uint32_t	 dpbp_id;
	uint16_t	*token;
} DEFAULT bypass_bp_open;

METHOD int bp_close {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
} DEFAULT bypass_bp_close;

METHOD int bp_enable {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
} DEFAULT bypass_bp_enable;

METHOD int bp_disable {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
} DEFAULT bypass_bp_disable;

METHOD int bp_reset {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
} DEFAULT bypass_bp_reset;

METHOD int bp_get_attributes {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	struct dpaa2_bp_attr *attr;
} DEFAULT bypass_bp_get_attributes;

/**
 * @brief Data Path MAC (DPMAC) commands.
 */

METHOD int mac_open {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	uint32_t	 dpmac_id;
	uint16_t	*token;
} DEFAULT bypass_mac_open;

METHOD int mac_close {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
} DEFAULT bypass_mac_close;

METHOD int mac_reset {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
} DEFAULT bypass_mac_reset;

METHOD int mac_mdio_read {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	uint8_t		 phy;
	uint16_t	 reg;
	uint16_t	*val;
} DEFAULT bypass_mac_mdio_read;

METHOD int mac_mdio_write {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	uint8_t		 phy;
	uint16_t	 reg;
	uint16_t	 val;
} DEFAULT bypass_mac_mdio_write;

METHOD int mac_get_addr {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	uint8_t		*mac;
} DEFAULT bypass_mac_get_addr;

METHOD int mac_get_attributes {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	struct dpaa2_mac_attr *attr;
} DEFAULT bypass_mac_get_attributes;

METHOD int mac_set_link_state {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	struct dpaa2_mac_link_state *state;
} DEFAULT bypass_mac_set_link_state;

METHOD int mac_set_irq_mask {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	uint8_t		 irq_idx;
	uint32_t	 mask;
} DEFAULT bypass_mac_set_irq_mask;

METHOD int mac_set_irq_enable {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	uint8_t		 irq_idx;
	bool		 en;
} DEFAULT bypass_mac_set_irq_enable;

METHOD int mac_get_irq_status {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	uint8_t		 irq_idx;
	uint32_t	*status;
} DEFAULT bypass_mac_get_irq_status;

/**
 * @brief Data Path Concentrator (DPCON) commands.
 */

METHOD int con_open {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	uint32_t	 dpcon_id;
	uint16_t	*token;
} DEFAULT bypass_con_open;

METHOD int con_close {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
} DEFAULT bypass_con_close;

METHOD int con_reset {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
} DEFAULT bypass_con_reset;

METHOD int con_enable {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
} DEFAULT bypass_con_enable;

METHOD int con_disable {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
} DEFAULT bypass_con_disable;

METHOD int con_get_attributes {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	struct dpaa2_con_attr *attr;
} DEFAULT bypass_con_get_attributes;

METHOD int con_set_notif {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	struct dpaa2_con_notif_cfg *cfg;
} DEFAULT bypass_con_set_notif;

/**
 * @brief Data Path MC Portal (DPMCP) commands.
 */

METHOD int mcp_create {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	uint32_t	 portal_id;
	uint32_t	 options;
	uint32_t	*dpmcp_id;
} DEFAULT bypass_mcp_create;

METHOD int mcp_destroy {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	uint32_t	 dpmcp_id;
} DEFAULT bypass_mcp_destroy;

METHOD int mcp_open {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
	uint32_t	 dpmcp_id;
	uint16_t	*token;
} DEFAULT bypass_mcp_open;

METHOD int mcp_close {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
} DEFAULT bypass_mcp_close;

METHOD int mcp_reset {
	device_t	 dev;
	device_t	 child;
	struct dpaa2_cmd *cmd;
} DEFAULT bypass_mcp_reset;
